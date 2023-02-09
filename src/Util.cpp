#include "pch.h"

#include "Util.h"

std::wstring Util::StringToWString(std::string input)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(input);
}

std::string Util::WStringToString(std::wstring input)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(input);
}