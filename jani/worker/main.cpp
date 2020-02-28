////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"

int main(int _argc, char* _argv[])
{
    const char*     runtime_ip          = _argv[1];
    uint32_t        runtime_listen_port = std::stoi(std::string(_argv[2]));
    Jani::LayerHash layer_hash          = std::stoull(std::string(_argv[3]));

    std::cout << "Worker -> Worker spawned for runtime_ip{" << runtime_ip << "}, runtime_listen_port{" << runtime_listen_port << "}, layer_hash{" << layer_hash << "}" << std::endl;

    auto actual_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    srand(actual_time);
    int random_port = rand() % 10000 + 5000;
    
    std::cout << "Worker -> Selected random port: port{" << random_port << "}" << std::endl << std::endl;

    Jani::Connection<> runtime_connection(random_port, runtime_listen_port, runtime_ip);
    Jani::RequestMaker request_maker;

    Jani::Message::WorkerAuthenticationRequest worker_connection_request;
    std::strcpy(worker_connection_request.ip, "127.0.0.1");
    worker_connection_request.port = random_port;
    worker_connection_request.layer_hash = layer_hash;
    worker_connection_request.access_token = -1;
    worker_connection_request.worker_authentication = -1;

    if (!request_maker.MakeRequest(runtime_connection, Jani::RequestType::WorkerAuthentication, worker_connection_request))
    {
        std::cout << "Problem sending worker connection request!" << std::endl;
        return 1;
    }

    bool exit = false;
    while (!exit)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        runtime_connection.Update();

        runtime_connection.DidTimeout(
            [&](std::optional<Jani::Connection<>::ClientHash> _client_hash)
            {
                exit = true;
            });

        request_maker.CheckResponses(
            runtime_connection,
            [](const Jani::Request& _original_request, const Jani::RequestResponse& _response) -> void
            {
                switch (_original_request.type)
                {
                    case Jani::RequestType::WorkerAuthentication:
                    {
                        auto response = _response.GetResponse<Jani::Message::WorkerAuthenticationResponse>();
                        if (!response.succeed)
                        {
                            std::cout << "Failed to authenticate!" << std::endl;
                        }
                        else
                        {
                            std::cout << "Authentication succeeded!" << std::endl;
                        }
                    }
                }
            });
    }

    return 0;
}