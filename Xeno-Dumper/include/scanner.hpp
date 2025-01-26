#include <Windows.h>
#include <vector>

#include <queue>
#include <thread>
#include <mutex>

#include <condition_variable>
#include <atomic>

std::atomic<std::uintptr_t> found_address(0);

struct MemoryRegion {
    std::uintptr_t base_address;
    SIZE_T size;
};

void scan_memory(const std::vector<unsigned char>& pattern, HANDLE hProcess, MemoryRegion region) {
    const unsigned char* nPattern = pattern.data();
    SIZE_T pLength = pattern.size();

    std::unique_ptr<unsigned char[]> buffer(new unsigned char[region.size]);
    SIZE_T bytes_read;

    if (!ReadProcessMemory(hProcess, (LPCVOID)region.base_address, buffer.get(), region.size, &bytes_read))
        return;

    for (SIZE_T i = 0; i <= bytes_read - pLength; ++i) {
        if (found_address.load() != 0)
            return;

        SIZE_T j = 0;
        for (; j < pLength; ++j) {
            if (buffer[i + j] != nPattern[j])
                break;
        }

        if (j == pLength) {
            found_address.store(region.base_address + i);
            return;
        }
    }
}

void worker_thread(const std::vector<unsigned char>& pattern, HANDLE hProcess, 
    std::queue<MemoryRegion>& work_queue, 
    std::mutex& queue_mutex, 
    std::condition_variable& cv, 
    std::atomic<bool>& done) 
{
    while (!done || !work_queue.empty()) {
        MemoryRegion region;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [&] { return !work_queue.empty() || done; });

            if (work_queue.empty()) continue;

            region = work_queue.front();
            work_queue.pop();
        }

        scan_memory(pattern, hProcess, region);

        if (found_address.load() != 0)
            break;
    }
}

std::uintptr_t pattern_scan(const std::vector<unsigned char>& pattern, HANDLE hProcess) {
    const SIZE_T tMax = std::thread::hardware_concurrency();

    std::vector<std::thread> threads;
    std::queue<MemoryRegion> work_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> done(false);

    for (std::uintptr_t region = 0; region < 0x7FFFFFFFFFFF;) {
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQueryEx(hProcess, (LPCVOID)region, &mbi, sizeof(mbi));

        /*
        if (!(mbi.State & MEM_COMMIT)
            || (mbi.Protect & PAGE_NOACCESS)
            || (mbi.Protect & PAGE_GUARD)
            || (mbi.Protect & PAGE_NOCACHE)
            || !(mbi.Protect & PAGE_READWRITE))
        {
            region += mbi.RegionSize;
            continue;
        }
        */

        if (mbi.State != 0x1000 || mbi.Protect != 0x04 || mbi.AllocationProtect != 0x04) 
        {
            region += mbi.RegionSize;
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            work_queue.push({ region, mbi.RegionSize });
        }

        cv.notify_one();

        region += mbi.RegionSize;
    }

    for (SIZE_T i = 0; i < tMax; ++i)
    {
        threads.emplace_back(worker_thread, std::cref(pattern), hProcess, std::ref(work_queue), std::ref(queue_mutex), std::ref(cv), std::ref(done));
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        done = true;
    }

    cv.notify_all();

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    return found_address.load();
}