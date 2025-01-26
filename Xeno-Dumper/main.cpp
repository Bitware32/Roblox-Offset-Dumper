#include <iostream>
#include <chrono>
#include <conio.h>

#include "xeno.h"
#include "ntdll.h"
#include "scanner.hpp"

#include <TlHelp32.h>

namespace settings {
    constexpr std::uint64_t oMin = 0x10;
    constexpr std::uint64_t oMax = 0x512;
    constexpr std::uint64_t oInc = 0x4;
}

namespace offsets {
    std::uint64_t Name;
    std::uint64_t Children;
    std::uint64_t Parent;
    std::uint64_t ClassDescriptor;
    std::uint64_t LocalPlayer;
}

namespace addresses {
    std::uintptr_t Players;
}

void SetConsoleWindowSize(int width, int height) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    COORD bufferSize = { static_cast<SHORT>(width), static_cast<SHORT>(height) };
    SetConsoleScreenBufferSize(hOut, bufferSize);

    SMALL_RECT windowSize = { 0, 0, static_cast<SHORT>(width - 1), static_cast<SHORT>(height - 1) };
    SetConsoleWindowInfo(hOut, TRUE, &windowSize);
}

inline void pausec() { // I don't like the system("pause") function
    std::cout << "\nPress any key to close this window..." << std::endl;
    while (!_kbhit())
        Sleep(50);
    _getch();
}

inline void assertc(bool c, const char* m = "An error occurred") {
    if (c) return;
    std::cerr << "[!] " << m << std::endl;
    pausec();
    exit(1);
}

void clear_console() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD console_size, chars_written;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    console_size = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, ' ', console_size, { 0, 0 }, &chars_written);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, console_size, { 0, 0 }, &chars_written);
    SetConsoleCursorPosition(hConsole, { 0, 0 });
}

std::uintptr_t GetBaseAddress(DWORD pid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32 moduleEntry;
    moduleEntry.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(snapshot, &moduleEntry)) {
        CloseHandle(snapshot);
        return (std::uintptr_t)moduleEntry.modBaseAddr;
    }

    CloseHandle(snapshot);
    return 0;
}

int APIENTRY WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd
) {
    AllocConsole(); FILE* fp; freopen_s(&fp, "CONOUT$", "w", stdout); freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONOUT$", "w", stdin);
    SetConsoleTitleW(L"Xeno Dumper - By Rizve");
    SetConsoleWindowSize(40, 5);

    HMODULE ntdll = LoadLibraryA("ntdll.dll");
    assertc(ntdll, "Could not load ntdll.dll");
    NTDLL_INIT_FCNS(ntdll);

    DWORD pid = GetProcessID(L"RobloxPlayerBeta.exe");
    assertc(pid != 0, "Roblox Client not found.");

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
    assertc(hProcess != 0, "Could not open Roblox process.");

    std::vector<unsigned char> pattern = {
        0x52, 0x65, 0x6e, 0x64, 0x65, 0x72, 0x4a, 0x6f,
        0x62, 0x28, 0x45, 0x61, 0x72, 0x6c, 0x79, 0x52,
        0x65, 0x6e, 0x64, 0x65, 0x72, 0x69, 0x6e, 0x67,
        0x3b
    }; // HBytes: 52 65 6e 64 65 72 4a 6f 62 28 45 61 72 6c 79 52 65 6e 64 65 72 69 6e 67 3b
       // String: RenderJob(EarlyRendering;

    std::cout << "Scanning For DataModel..." << std::endl;

    std::uintptr_t stringAddress = pattern_scan(pattern, hProcess);
    assertc(stringAddress != 0, "String address not found.");

    std::uintptr_t RenderView = read_memory<std::uintptr_t>(stringAddress + 0x1e8, hProcess);
    std::uintptr_t dm1 = read_memory<std::uintptr_t>(RenderView + 0x118, hProcess);
    std::uintptr_t DataModel = read_memory<std::uintptr_t>(dm1 + 0x1a8, hProcess);

    clear_console();

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    SetConsoleWindowSize(97, 30);

    auto start = std::chrono::high_resolution_clock::now();

    std::cout << std::hex << sAddress << "DataModel = " << num << "0x" << DataModel << g << ";\n\n";
    std::cout << f << "namespace " << g << "offsets {\n";
    std::cout << t << com << "// Instance\n";
    std::cout << sOffset << "This = " << num << "0x8" << g << ";\n";

    for (std::uint64_t offset = settings::oMin; offset <= settings::oMax; offset += settings::oInc) {
        std::string Name = functions::ReadRobloxString(read_memory<std::uintptr_t>(DataModel + offset, hProcess), hProcess);
        if (Name == "Ugc" || Name == "LuaApp" || Name == "Game" || Name == "App") { // or just check length
            //std::cout << "Name: 0x" << offset << std::endl;
            std::cout << sOffset << "Name = " << num << "0x" << offset << g << ";\n";
            offsets::Name = offset;
            break;
        }
    }
    assertc(offsets::Name != 0, "Could not get Name offset");

    for (std::uint64_t offset = settings::oMin; offset <= settings::oMax; offset += settings::oInc) {
        std::uintptr_t childrenPtr = read_memory<std::uintptr_t>(DataModel + offset, hProcess);
        if (childrenPtr == 0)
            continue;

        std::uintptr_t childrenStart = read_memory<std::uintptr_t>(childrenPtr, hProcess);
        if (childrenStart == 0)
            continue;

        std::uintptr_t childrenEnd = read_memory<std::uintptr_t>(childrenPtr + 0x8, hProcess) + 1;

        if (childrenEnd < childrenStart)
            continue;

        if (childrenEnd > DataModel * 2)
            continue;

        for (std::uintptr_t childAddress = childrenStart; childAddress < childrenEnd; childAddress += 0x10) {
            std::uintptr_t childPtr = read_memory<std::uintptr_t>(childAddress, hProcess);
            if (childPtr == 0)
                break;

            if (functions::ReadRobloxString(read_memory<std::uintptr_t>(childPtr + offsets::Name, hProcess), hProcess) == "Players") {
                //std::cout << "Children: 0x" << offset << std::endl;
                std::cout << sOffset << "Children = " << num << "0x" << offset << g << ";\n";
                offsets::Children = offset;
                addresses::Players = childPtr;
                break;
            }
        }

        if (offsets::Children != 0)
            break;
    }

    for (std::uint64_t offset = settings::oMin; offset <= settings::oMax; offset += settings::oInc) {
        if (read_memory<std::uintptr_t>(addresses::Players + offset, hProcess) == DataModel) {
            //std::cout << "Parent: 0x" << offset << std::endl;
            std::cout << sOffset << "Parent = " << num << "0x" << offset << g << ";\n";
            offsets::Parent = offset;
            break;
        }
    }

    for (std::uint64_t offset = settings::oMin; offset <= settings::oMax; offset += settings::oInc) {
        if (functions::ReadRobloxString(read_memory<std::uintptr_t>(read_memory<std::uintptr_t>(DataModel + offset, hProcess) + 0x8, hProcess), hProcess) == "DataModel") {
            //std::cout << "ClassDescriptor: 0x" << offset << std::endl;
            std::cout << "\n" << sOffset << "ClassDescriptor = " << num << "0x" << offset << g << ";\n";
            std::cout << sOffset << "ClassName = " << num << "0x8" << g << ";\n";
            offsets::ClassDescriptor = offset;
            break;
        }
    }

    for (std::uint64_t offset = settings::oMin; offset <= settings::oMax; offset += settings::oInc) {
        if (offset == offsets::Name || offset == offsets::Parent || offset == offsets::Children)
            continue;

        std::uintptr_t pPlayer = read_memory<std::uintptr_t>(addresses::Players + offset, hProcess);
        if (pPlayer == 0)
            continue;

        std::string Name = functions::ReadRobloxString(read_memory<std::uintptr_t>(pPlayer + offsets::Name, hProcess), hProcess);
        if (Name.length() < 3) // Minimum username length: 3
            continue;

        //std::cout << "LocalPlayer: 0x" << offset << std::endl;
        offsets::LocalPlayer = offset;
        break;
    }

    std::string Name = functions::ReadRobloxString(read_memory<std::uintptr_t>(DataModel + offsets::Name, hProcess), hProcess);
    if (Name == "App" || Name == "LuaApp") {
        std::cout << "\n" << t << com << "// Other\n";
        std::cout << sOffset << "LocalPlayer = " << num << "0x" << offsets::LocalPlayer << g << ";\n";
        std::cout << "}\n\n";
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Took " << num << duration.count() << g << " seconds to get all offsets.\n" << com << std::endl;
        assertc(false, "Could not retrieve the remaining offsets from the Roblox menu. Please join a Roblox game.");
    }

    std::uintptr_t StarterPlayer = FindFirstChildAddress("StarterPlayer", DataModel, offsets::Children, offsets::Name, hProcess);
    assertc(StarterPlayer != 0, "Could not get DataModel -> StarterPlayer");

    std::uintptr_t StarterPlayerScripts = FindFirstChildAddress("StarterPlayerScripts", StarterPlayer, offsets::Children, offsets::Name, hProcess);
    assertc(StarterPlayerScripts != 0, "Could not get DataModel -> StarterPlayer -> StarterPlayerScripts");

    std::uintptr_t PlayerModule = FindFirstChildAddress("PlayerModule", StarterPlayerScripts, offsets::Children, offsets::Name, hProcess);
    assertc(PlayerModule != 0, "Could not get DataModel -> StarterPlayer -> StarterPlayerScripts -> PlayerModule");

    std::cout << "\n" << t << com << "// Scripts";
    for (std::uint64_t offset = settings::oMin; offset <= settings::oMax; offset += settings::oInc) {
        if (offset == offsets::Name || offset == offsets::Parent || offset == offsets::Children)
            continue;

        std::uint64_t zx = read_memory<std::uint64_t>(PlayerModule + offset, hProcess);
        std::uint64_t zy = read_memory<std::uint64_t>(PlayerModule + (offset - 0x8), hProcess);
        std::uint64_t xy = read_memory<std::uint64_t>(PlayerModule + (offset - 0x48), hProcess);

        if (zx == 0x8 /*0x1f0*/ && zy == 0x7 /*0x1e8*/ && xy == 0x2f /*0x1a8*/) { /*-> 0x168*/
            std::cout << "\n" << sOffset << "ModuleScriptEmbedded = " << num << "0x" << offset - 0x88 << g << ";\n";
            std::cout << sOffset << "IsCoreScript = " << num << "0x" << offset - 0x40 << g << ";\n";
            std::cout << sOffset << "ModuleFlags = IsCoreScript - " << num << "0x4" << g << ";\n";
            break;
        }
    }

    std::uintptr_t PlayerScriptsLoader = FindFirstChildAddress("PlayerScriptsLoader", StarterPlayerScripts, offsets::Children, offsets::Name, hProcess);
    assertc(PlayerScriptsLoader != 0, "Could not get DataModel -> StarterPlayer -> StarterPlayerScripts -> PlayerScriptsLoader");

    for (std::uint64_t offset = settings::oMin; offset <= settings::oMax; offset += settings::oInc) {
        if (offset == offsets::Name || offset == offsets::Parent || offset == offsets::Children)
            continue;

        std::uintptr_t pBytecode = read_memory<std::uintptr_t>(PlayerScriptsLoader + offset, hProcess);
        if (pBytecode == 0)
            continue;

        std::uintptr_t pBytecodeSize = read_memory<std::uintptr_t>(pBytecode + 0x20, hProcess);
        if (pBytecodeSize < 100 || pBytecodeSize > 200)
            continue;

        std::cout << sOffset << "LocalScriptEmbedded = " << num << "0x" << offset << g << ";\n";
        break;
    }

    std::cout << "\n" << sOffset << "Bytecode = " << num << "0x10" << g << ";\n";
    std::cout << sOffset << "BytecodeSize = " << num << "0x20" << g << ";\n";

    std::cout << "\n" << t << com << "// Other\n";
    std::cout << sOffset << "LocalPlayer = " << num << "0x" << offsets::LocalPlayer << g << ";\n";
    std::cout << sOffset << "ObjectValue = " << num << "0" << g << ";" << com << " // Shift between bytes of the other changed offsets\n" << g;

    std::cout << "}\n\n";

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Took " << num << duration.count() << g << " seconds to get all offsets." << std::endl;

    pausec();

    return 0;
}