////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"
#include "JaniWorker.h"

int main(int _argc, char* _argv[])
{
    const char*     runtime_ip          = _argv[1];
    uint32_t        runtime_listen_port = std::stoi(std::string(_argv[2]));
    Jani::LayerHash layer_hash          = std::stoull(std::string(_argv[3]));

    std::cout << "Worker -> Worker spawned for runtime_ip{" << runtime_ip << "}, runtime_listen_port{" << runtime_listen_port << "}, layer_hash{" << layer_hash << "}" << std::endl;

    Jani::Worker worker(layer_hash);

    if (!worker.InitializeWorker(runtime_ip, runtime_listen_port))
    {
        std::cout << "Worker -> Unable to initialize worker!" << std::endl;
    }

    worker.RequestAuthentication().OnResponse(
        [](const Jani::Message::WorkerAuthenticationResponse& _response, bool _timeout)
        {
            if (!_response.succeed)
            {
                std::cout << "Worker -> Failed to authenticate!" << std::endl;
            }
            else
            {
                std::cout << "Worker -> Authentication succeeded!" << std::endl;
            }
        }).WaitResponse();

    std::cout << "Worker -> Starting main loop!" << std::endl;

    while (worker.IsConnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        worker.Update(20);
    }

    std::cout << "Worker -> Disconnected from the server!" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    return 0;
}