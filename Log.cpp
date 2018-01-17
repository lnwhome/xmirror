#include <fstream>
#include <iostream>

#include "Log.h"

std::map<std::string, int> logLevel;

std::ostream& logGetCout(int thr, const std::string& prefix, const std::string& module, int line)
{
    static std::ofstream logNullCout("/dev/null");
    auto it = logLevel.find(module);
    auto level = (it != logLevel.end()) ? it->second : logDefaultLoglevel;

    if (level > thr)
    {
        std::cout << prefix << module << ":" << line << " ";
        return std::cout;
    }
    else
    {
        return logNullCout;
    }
};
