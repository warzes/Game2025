﻿#pragma once

struct LogSystemCreateInfo final
{

};

class LogSystem final
{
public:
	~LogSystem();

	bool Create(const LogSystemCreateInfo& createInfo);
	void Destroy();

	void Print(const std::string& msg);
	void Warning(const std::string& msg);
	void Error(const std::string& msg);
	void Fatal(const std::string& msg);
};