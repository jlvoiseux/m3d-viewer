#pragma once
#include <string>

class Util {

public:
	
	static std::wstring StringToWString(std::string input);
	static std::string WStringToString(std::wstring input);
};