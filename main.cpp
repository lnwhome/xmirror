#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cmath>
#include <iostream>
#include <sstream>
#include <thread>

#include "RenderingEngine.h"
#include "WebCamClient.h"
#include "XServerMirror.h"
#include "TypesConf.h"

int main(int argc, char **arg)
{
    std::vector<std::shared_ptr<Client> > clients;
    
    logLevel["/home/darek/lnw/lnw/Projects/opencl/new_vr/XServerMirror.cpp"] = 2;
    logLevel["/home/darek/lnw/lnw/Projects/opencl/new_vr/XServerMirror.h"] = 4;
    
    RenderingEngine REObj(argc, arg, clients);

    try {
        clients.push_back(std::make_shared<XServerMirror>("master_list", "black_list"));
        clients.push_back(std::make_shared<WebCamera>("ANY", true));
    } catch (const Error& e)
    {
        std::cout << "ERROR: Got exception with message: " << e.mMsg << "\n";
        return -1;
    }
    
    // run core
    auto exit{false};
    for (auto client : clients)
    {
        client->run(&REObj, &exit);
    }
    REObj.run();
    exit = true;
    for (auto client : clients)
    {
        client->join();
    }
   
    return 0;
}
