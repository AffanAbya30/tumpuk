#pragma once
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#endif

inline std::filesystem::path executableFolder() {
#ifdef _WIN32
    std::array<wchar_t,32768> name{};
    const DWORD size=GetModuleFileNameW(nullptr,name.data(),static_cast<DWORD>(name.size()));
    if(size && size<name.size()) return std::filesystem::path(name.data()).parent_path();
#elif defined(__linux__)
    std::error_code ec;
    auto name=std::filesystem::read_symlink("/proc/self/exe",ec);
    if(!ec) return name.parent_path();
#endif
    std::error_code error;
    auto folder=std::filesystem::current_path(error);
    return error?std::filesystem::path("."):folder;
}
