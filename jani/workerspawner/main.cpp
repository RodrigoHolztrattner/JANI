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

int main(int _argc, char* _argv[])
{
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