#include "stdafx.h"
#include "StringUtils.h"
//=============================================================================
std::string UnicodeToASCII(const PWSTR pwstr)
{
	const std::wstring wstr(pwstr);
#if _WIN32
	// https://codingtidbit.com/2020/02/09/c17-codecvt_utf8-is-deprecated/
	if (wstr.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
#else
	return std::string(wstr.begin(), wstr.end());  // results in warning in C++17
#endif
}
//=============================================================================