////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniInternal.h"
#include <Windows.h>
#include <shellapi.h>

HANDLE ExecuteExternalExeFileNGetReturnValue(std::wstring _path, std::wstring _parameters)
{
    SHELLEXECUTEINFO ShExecInfo = { 0 };
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.hwnd = NULL;
    ShExecInfo.lpVerb = NULL;
    ShExecInfo.lpFile = _path.c_str();
    ShExecInfo.lpParameters = _parameters.c_str();
    ShExecInfo.lpDirectory = NULL;
    ShExecInfo.nShow = SW_SHOW;
    ShExecInfo.hInstApp = NULL;
    ShellExecuteEx(&ShExecInfo);

    return ShExecInfo.hProcess;
}

static std::wstring char_to_wchar(const char* text)
{
    const size_t size = std::strlen(text);
    std::wstring wstr;
    if (size > 0) {
        wstr.resize(size);
        std::mbstowcs(&wstr[0], text, size);
    }
    return wstr;
}

int main(int _argc, char* _argv[])
{
    Jani::InitializeStandardConsole();

    if (_argc != 4)
    {
        //return 1;
    }

    const char* dst_ip = "127.0.0.1";
    uint32_t    dst_port = 13001;
    uint32_t    local_port = 8092;

    Jani::Connection<> runtime_connection(local_port);
    Jani::RequestManager request_manager;

    Jani::MessageLog().Info("WorkerSpawner -> Listening for requests on dst_ip {}, dst_port {}, local_port {}", dst_ip, dst_port, local_port);

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        runtime_connection.Update();

        runtime_connection.DidTimeout(
            [&](auto _client_hash)
            {
            });

        request_manager.Update(
            runtime_connection,
            [&](auto _client_hash, const Jani::RequestInfo& _request, const Jani::RequestPayload& _request_payload, Jani::ResponsePayload& _response_payload)
            {
                switch (_request.type)
                {
                    case Jani::RequestType::SpawnWorkerForLayer:
                    {
                        auto worker_spawn_request = _request_payload.GetRequest<Jani::Message::WorkerSpawnRequest>();

                        Jani::MessageLog().Info("WorkerSpawner -> Received spawn request: runtime_ip {} , runtime_listen_port {}, layer_id {}", worker_spawn_request.runtime_ip, worker_spawn_request.runtime_worker_connection_port, worker_spawn_request.layer_id);

                        std::wstring process_parameters;
                        process_parameters += char_to_wchar(worker_spawn_request.runtime_ip.c_str());
                        process_parameters += L" ";
                        process_parameters += std::to_wstring(worker_spawn_request.runtime_worker_connection_port);
                        process_parameters += L" ";
                        process_parameters += std::to_wstring(worker_spawn_request.layer_id);

#if NDEBUG
                        bool result = ExecuteExternalExeFileNGetReturnValue(L"C:\\Users\\rodri\\Documents\\JANI\\build\\Release\\jani_worker.exe", process_parameters) != 0;
#else
                        bool result = ExecuteExternalExeFileNGetReturnValue(L"C:\\Users\\rodri\\Documents\\JANI\\build\\Debug\\jani_worker.exe", process_parameters) != 0;
#endif
                        Jani::Message::WorkerSpawnResponse authentication_response = { result };
                        {
                            _response_payload.PushResponse(std::move(authentication_response));
                        }
                    }
                }
            });
    }

    return 0;

    auto handle = ExecuteExternalExeFileNGetReturnValue(L"C:\\Users\\rodri\\Documents\\JANI\\build\\Debug\\jani_worker.exe", L"arg1 arg2 arg3");

    DWORD exitCode;
    if (WaitForSingleObject(handle, INFINITE) == 0) 
    {
        GetExitCodeProcess(handle, &exitCode);
        if (exitCode != 0)
        {
            return false;
        }
        else 
        {
            return true;
        }
    }
    else 
    {
        return false;
    }

    return 0;
}