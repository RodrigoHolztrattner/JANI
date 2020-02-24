////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"
#include <Windows.h>

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
    if (_argc != 4)
    {
        return 1;
    }

    const char* dst_ip   = _argv[1];
    uint32_t    dst_port = std::stoi(std::string(_argv[2]));
    uint32_t    local_port = std::stoi(std::string(_argv[3]));

    Jani::Connection runtime_connection(0, local_port, dst_port, dst_ip);

    std::cout << "WorkerSpawner -> Listening for requests on dst_ip{" << dst_ip << "}, dst_port{" << dst_port << "}, local_port{" << local_port << "}" << std::endl << std::endl;

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::array<int8_t, Jani::Connection::MaximumDatagramSize> data;
        while (auto message_size = runtime_connection.Receive(data.data(), data.size()))
        {
            if (message_size.value() != sizeof(Jani::Message::WorkerSpawnRequest))
            {
                continue;
            }


            auto& spawn_request = *reinterpret_cast<Jani::Message::WorkerSpawnRequest*>(data.data());

            std::cout << "WorkerSpawner -> Received spawn request: runtime_ip{" << spawn_request.runtime_ip << "}, runtime_listen_port{" << spawn_request.runtime_listen_port << "}, layer_name{" << spawn_request.layer_name << "}" << std::endl;

            std::wstring process_parameters;
            process_parameters += char_to_wchar(spawn_request.runtime_ip);
            process_parameters += L" ";
            process_parameters += std::to_wstring(spawn_request.runtime_listen_port);
            process_parameters += L" ";
            process_parameters += char_to_wchar(spawn_request.layer_name);

            ExecuteExternalExeFileNGetReturnValue(L"C:\\Users\\rodri\\Documents\\JANI\\build\\Debug\\jani_worker.exe", process_parameters);
        }
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