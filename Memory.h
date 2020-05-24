#pragma once // Compile only once.
 
#include <Windows.h> //Contains WinAPI Functions to read/write into the game's process.
 
#include <TlHelp32.h> //Contains functions useful for retrieving modules (.dll) from the game.
 
class Memory { // Handles game memory process.
    private:
        DWORD dwPID; // ProcessID of game process.
        HANDLE hProc; // Handle of game process.
 
    public: // Made public to quickly access the elements of MODULEENTRY32 struct and operate externally.
        MODULEENTRY32 ClientDLL;
        MODULEENTRY32 EngineDLL;
        DWORD ClientDLL_Base, ClientDLL_Size;
        DWORD EngineDLL_Base, EngineDLL_Size;

    bool AttachProcess(char* ProcessName) { // Gets process handle of game.
	    HANDLE hPID = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	    PROCESSENTRY32 procEntry;
	    procEntry.dwSize = sizeof(procEntry);
 
	    const WCHAR* procNameChar;
	    int nChars = MultiByteToWideChar(CP_ACP, 0, ProcessName, -1, NULL, 0);
	    procNameChar = new WCHAR[nChars];
	    MultiByteToWideChar(CP_ACP, 0, ProcessName, -1, (LPWSTR)procNameChar, nChars);
 
	    do
		    if (!wcscmp(procEntry.szExeFile, procNameChar))
		    {
			    this->dwPID = procEntry.th32ProcessID;
			    CloseHandle(hPID);
			    this->hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, this->dwPID);
			    return true;
		    }
	    while (Process32Next(hPID, &procEntry));
            CloseHandle(hPID);
	    return false;
    }

    MODULEENTRY32 GetModule(char* ModuleName) { // Retrieves moduleentry32 struct for game .dll.
        HANDLE hModule = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, dwPID);
	    MODULEENTRY32 mEntry;
	    mEntry.dwSize = sizeof(mEntry);
 
	    const WCHAR *modNameChar;
            int nChars = MultiByteToWideChar(CP_ACP, 0, ModuleName, -1, NULL, 0);
            modNameChar = new WCHAR[nChars];
            MultiByteToWideChar(CP_ACP, 0, ModuleName, -1, (LPWSTR)modNameChar, nChars);
 
	    do
		    if (!wcscmp(mEntry.szModule, modNameChar))
		    {
			    CloseHandle(hModule);
			    return mEntry;
		    }
	    while (Module32Next(hModule, &mEntry));
 
            CloseHandle(hModule);
	    mEntry.modBaseAddr = 0x0;
	    return mEntry;
    }

    // Implement Read and Write functions next

};