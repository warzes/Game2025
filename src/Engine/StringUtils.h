#pragma once

inline std::wstring ASCIIToUnicode(const std::string& str) { return std::wstring(str.begin(), str.end()); }

template<unsigned STR_SIZE>
std::string UnicodeToASCII(const WCHAR wchars[STR_SIZE])
{
	char ascii[STR_SIZE];
	size_t numCharsConverted = 0;
	wcstombs_s(&numCharsConverted, ascii, wchars, STR_SIZE);
	return std::string(ascii);
}