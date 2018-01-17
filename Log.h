#pragma once
#include <string>
#include <map>

static const int logDefaultLoglevel = 2;

extern std::map<std::string, int> logLevel;

std::ostream& logGetCout(int thr, const std::string& prefix, const std::string& module, int line);

#define loge_ [&]()->std::ostream& { return ::logGetCout(0, "\033[1;31m", __FILE__, __LINE__);}()
#define logw_ [&]()->std::ostream& { return ::logGetCout(1, "\033[1;34m", __FILE__, __LINE__);}()
#define logi_ [&]()->std::ostream& { return ::logGetCout(2, "\033[1;32m", __FILE__, __LINE__);}()
#define logd_ [&]()->std::ostream& { return ::logGetCout(3, "\033[1;36m", __FILE__, __LINE__);}()
