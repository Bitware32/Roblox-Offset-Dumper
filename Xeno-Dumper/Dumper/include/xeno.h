#pragma once
#include <cstdint>
#include <wtypes.h>
#include <vector>
#include <string>

template<typename T>
T read_memory(std::uintptr_t address, HANDLE handle);

template <typename T>
bool write_memory(std::uintptr_t address, const T& value, HANDLE handle);

namespace functions { // functions for rbx
    std::vector<std::uintptr_t> GetChildrenAddresses(std::uintptr_t address, std::uint64_t Ochildren, HANDLE handle);
    std::string ReadRobloxString(std::uintptr_t address, HANDLE handle);
}

std::uintptr_t FindFirstChildAddress(std::string_view name, std::uintptr_t address, std::uint64_t Ochildren, std::uint64_t oName, HANDLE handle);

const std::string t = "     ";
const std::string com = "\033[1;30m";
const std::string f = "\033[38;2;235;115;108m";
const std::string num = "\033[38;2;121;192;255m";
const std::string g = "\033[38;2;211;211;211m";
const std::string sOffset = t + f + "constexpr " + g + "std::" + f + "uint64_t " + g;
const std::string sAddress = f + "const " + g + "std::" + f + "uintptr_t " + g;

//std::uintptr_t GetRV(HANDLE handle);
//std::uintptr_t GetDataModel(HANDLE handle);
DWORD GetProcessID(const std::wstring_view processName);