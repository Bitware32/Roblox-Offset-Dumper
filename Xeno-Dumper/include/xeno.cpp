#include "xeno.h"

#include <TlHelp32.h>
#include <regex>
#include <fstream>
#include <filesystem>

#include "ntdll.h"
#include <iostream>

std::vector<std::uintptr_t> functions::GetChildrenAddresses(std::uintptr_t address, std::uint64_t Ochildren, HANDLE handle) {
    std::vector<std::uintptr_t> children;
    {
        std::uintptr_t childrenPtr = read_memory<std::uintptr_t>(address + Ochildren, handle);
        if (childrenPtr == 0)
            return children;

        std::uintptr_t childrenStart = read_memory<std::uintptr_t>(childrenPtr, handle);
        std::uintptr_t childrenEnd = read_memory<std::uintptr_t>(childrenPtr + 0x8, handle) + 1;

        for (std::uintptr_t childAddress = childrenStart; childAddress < childrenEnd; childAddress += 0x10) {
            std::uintptr_t childPtr = read_memory<std::uintptr_t>(childAddress, handle);
            if (childPtr != 0)
                children.push_back(childPtr);
        }
    }
    return children;
}

std::string functions::ReadRobloxString(std::uintptr_t address, HANDLE handle) {
    std::uint64_t stringCount = read_memory<std::uint64_t>(address + 0x10, handle);

    if (stringCount > 15000 || stringCount <= 0)
        return "";

    if (stringCount > 15)
        address = read_memory<std::uintptr_t>(address, handle);

    std::string buffer;
    buffer.resize(stringCount);

    MEMORY_BASIC_INFORMATION bi;
    VirtualQueryEx(handle, reinterpret_cast<LPCVOID>(address), &bi, sizeof(bi));

    NtReadVirtualMemory(handle, reinterpret_cast<LPCVOID>(address), buffer.data(), (ULONG)stringCount, nullptr);

    PVOID baddr = bi.AllocationBase;
    SIZE_T size = bi.RegionSize;
    NtUnlockVirtualMemory(handle, &baddr, &size, 1);

    return buffer;
}

std::uintptr_t FindFirstChildAddress(std::string_view name, std::uintptr_t address, std::uint64_t Ochildren, std::uint64_t oName, HANDLE handle) {
    std::vector<std::uintptr_t> childAddresses = functions::GetChildrenAddresses(address, Ochildren, handle);
    for (std::uintptr_t address : childAddresses) {
        if (functions::ReadRobloxString(read_memory<std::uintptr_t>(address + oName, handle), handle) == name)
            return address;
    }
    return 0;
}

DWORD GetProcessID(const std::wstring_view processName) {
    DWORD pid = 0;

    HANDLE snapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (snapShot == INVALID_HANDLE_VALUE)
        return pid;

    PROCESSENTRY32W entry = { sizeof(PROCESSENTRY32W) };

    if (Process32FirstW(snapShot, &entry)) {
        if (_wcsicmp(processName.data(), entry.szExeFile) == 0) {
            return entry.th32ProcessID;
        }
        while (Process32NextW(snapShot, &entry)) {
            if (_wcsicmp(processName.data(), entry.szExeFile) == 0) {
                return entry.th32ProcessID;
            }
        }
    }

    CloseHandle(snapShot);
    return pid;
}

/*
std::string GetFileName(HANDLE handle) {
    char buffer[MAX_PATH] = { 0 };
    if (GetFinalPathNameByHandleA(handle, buffer, MAX_PATH, 0)) {
        std::string path(buffer);

        const std::string prefix = "\\\\?\\";
        if (path.compare(0, prefix.length(), prefix) == 0) {
            path = path.substr(prefix.length());
        }

        return path;
    }
    return "";
}

#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)

std::uintptr_t GetRV(HANDLE handle) {
    NTSTATUS status;

    ULONG bufferSize = 0x10000;
    std::vector<BYTE> buffer;

    ULONG returnLength = 0;

    // dynamically adjust buffer size matching sys requirements
    do {
        buffer.resize(bufferSize);
        status = NtQuerySystemInformation(SystemHandleInformation, buffer.data(), bufferSize, &returnLength);
        if (status == STATUS_INFO_LENGTH_MISMATCH)
            bufferSize *= 2;
    } while (status == STATUS_INFO_LENGTH_MISMATCH);

    if (status != 0)
        return 0;

    DWORD ProcessId = GetProcessId(handle);

    std::string logPath;
    int tr = 0;
    do {
        if (tr > 5)
            return 0;

        PSYSTEM_HANDLE_INFORMATION handleInfo = (PSYSTEM_HANDLE_INFORMATION)buffer.data();
        for (ULONG i = 0; i < handleInfo->HandleCount; ++i) {
            SYSTEM_HANDLE handle = handleInfo->Handles[i];
            if (handle.ProcessId != ProcessId)
                continue;

            HANDLE dupHandle;
            HANDLE targetProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, ProcessId);
            if (!targetProcess)
                continue;

            if (DuplicateHandle(targetProcess, (HANDLE)handle.Handle, GetCurrentProcess(),
                &dupHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
            {
                std::string fileName = GetFileName(dupHandle);
                std::string ending = "_last.log"; // matching the log file path via using string may not be optimal. try directory checking?
                CloseHandle(dupHandle);

                if (fileName.size() >= ending.size() &&
                    fileName.compare(fileName.size() - ending.size(), ending.size(), ending) == 0)
                {
                    logPath = fileName;
                    break;
                }
            }

            CloseHandle(targetProcess);
        }
        if (logPath.empty()) {
            Sleep(500);
            tr++;
        }
    } while (logPath.empty());

    if (logPath.empty())
        return 0;

    std::ifstream logFile(logPath);
    if (!logFile.is_open())
        return 0;

    std::string renderview;
    std::stringstream fbuffer;
    fbuffer << logFile.rdbuf();
    std::string content = fbuffer.str();

    std::regex rgx(R"(\bSurfaceController\[_:1\]::initialize view\((.*?)\))");
    std::smatch match;

    if (std::regex_search(content, match, rgx)) {
        if (match.size() <= 1)
            return 0;
        renderview = match[1].str();
    }

    logFile.close();

    std::uintptr_t renderviewAddress = std::strtoull(renderview.c_str(), nullptr, 16);
    std::uintptr_t fakeDataModel = read_memory<std::uintptr_t>(renderviewAddress + 0x118, handle);
    if (fakeDataModel != 0)
        return renderviewAddress;

    return 0;
}

inline bool checkDataModel(std::uintptr_t address, HANDLE handle) {
    std::uintptr_t ptr = read_memory<std::uintptr_t>(address + 0x68, handle);
    if (ptr == 0)
        return 0;

    std::string name = functions::ReadRobloxString(ptr, handle);

    if (name == "Ugc" || name == "LuaApp" || name == "Game" || name == "App")
        return 1;

    return 0;
}

std::uintptr_t GetDataModel(HANDLE handle) {
    std::string pattern = "\x40\xD8\x00\x00\xF0\x0F\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xF0\x0F\x00\x00";
    std::string mask = "xx??xx??xxxxxxxxxxxxxxxxxx";

    scanner pscanner(GetProcessId(handle));
    std::uintptr_t address = pscanner.findsignature(pattern, mask);
    if (checkDataModel(address, handle))
        return address; 

    for (const auto addr : pscanner.returnaddreses()) {
        if (checkDataModel(addr, handle))
            return addr;
    }

    return 0;
}
*/
template<typename T>
T read_memory(std::uintptr_t address, HANDLE handle)
{
    T value = 0;
    MEMORY_BASIC_INFORMATION bi;

    VirtualQueryEx(handle, reinterpret_cast<LPCVOID>(address), &bi, sizeof(bi));

    NtReadVirtualMemory(handle, reinterpret_cast<LPCVOID>(address), &value, sizeof(value), nullptr);

    PVOID baddr = bi.AllocationBase;
    SIZE_T size = bi.RegionSize;
    NtUnlockVirtualMemory(handle, &baddr, &size, 1);

    return value;
}

template <typename T>
bool write_memory(std::uintptr_t address, const T& value, HANDLE handle)
{
    SIZE_T bytesWritten;
    DWORD oldProtection;

    if (!VirtualProtectEx(handle, reinterpret_cast<LPVOID>(address), sizeof(value), PAGE_READWRITE, &oldProtection)) {
        return false;
    }

    if (NtWriteVirtualMemory(handle, reinterpret_cast<PVOID>(address), (PVOID)&value, sizeof(value), &bytesWritten) || bytesWritten != sizeof(value)) {
        return false;
    }

    DWORD d;
    if (!VirtualProtectEx(handle, reinterpret_cast<LPVOID>(address), sizeof(value), oldProtection, &d)) {
        return false;
    }

    return true;
}