/*
This file implements a plugin for Ashita v4.

ImmHijack is designed to enable Final Fantasy XI to handle input from
the IME in Unicode format (specifically UTF-16, as used by Windows).
By doing so, the game can properly process Japanese input even on
systems where the locale is not set to Japanese (i.e., the system code
page is not 932). This eliminates the requirement for users to modify
system-level settings to enable Japanese input functionality.

Author: Xiyan (aka Hyururu

COMPILE THE DLL:
cl /EHsc /std:c++20 /O2 /c ImmHijack.cpp
link ImmHijack.obj /dll

!!!  Please make sure compile it as *x86 dll*.  !!!

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <https://unlicense.org/>
*/
#include "sdk/Ashita.h"

#pragma comment(lib, "imm32.lib")

#include <clocale>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void HookRaw(void *target, void *detour, void *originalBytesBuffer)
{
    DWORD oldProtect;
    VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

    memcpy(originalBytesBuffer, target, 5);

    uintptr_t relativeAddress = (uintptr_t)detour - (uintptr_t)target - 5;

    byte *patch = (byte*)target;
    patch[0] = 0xE9; // JMP
    *(uintptr_t*)(patch + 1) = relativeAddress;

    VirtualProtect(target, 5, oldProtect, &oldProtect);
}

void UnhookRaw(void *target, void *originalBytesBackup)
{
    DWORD oldProtect;
    VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

    memcpy(target, originalBytesBackup, 5);

    VirtualProtect(target, 5, oldProtect, &oldProtect);
}

int Hook(const char *moduleName, const char *functionName, void *detour, void *backupBuffer)
{
    HMODULE hModule = GetModuleHandleA(moduleName);
    if (!hModule) return -1;

    void *target = GetProcAddress(hModule, functionName);
    if (!target) return -2;

    HookRaw(target, detour, backupBuffer);

    return 0;
}

int Unhook(const char *moduleName, const char *functionName, void *backupBuffer)
{
    HMODULE hModule = GetModuleHandleA(moduleName);
    if (!hModule) return -1;

    void *target = GetProcAddress(hModule, functionName);
    if (!target) return -2;

    UnhookRaw(target, backupBuffer);

    return 0;
}

LONG WINAPI ImmGetCompositionStringSJis(HIMC hIMC, DWORD dwIndex, LPVOID lpBuf, DWORD dwBufLen)
{
    BYTE wideBuffer[512] = {0};


    LONG ret = ImmGetCompositionStringW(hIMC, dwIndex, wideBuffer, sizeof(wideBuffer));
    if (ret > 0)
    {
        int size = WideCharToMultiByte(932, 0, (LPCWCH)wideBuffer, -1, (LPSTR)lpBuf, dwBufLen, NULL, NULL);

        if (size > 0) return size;
    }
    return ret;
}

DWORD WINAPI ImmGetConversionListSJis(HKL hKL, HIMC hIMC, LPCSTR lpSrc, LPCANDIDATELIST lpDst, DWORD dwBufLen, UINT uFlag)
{
    BYTE buffer[512] = {0};

    MultiByteToWideChar(932, 0, lpSrc, -1, (LPWSTR)buffer, sizeof(buffer));
    return ImmGetConversionListW(hKL, hIMC, (LPCWCH)buffer, lpDst, dwBufLen, uFlag);
}

BOOL WINAPI ImmSetCompositionStringSJis(HIMC hIMC, DWORD dwIndex, LPVOID lpComp, DWORD dwCompLen, LPVOID lpRead, DWORD dwReadLen)
{
    BYTE buffer[512] = {0};

    int size = MultiByteToWideChar(932, 0, (LPCCH)lpRead, -1, (LPWSTR)buffer, sizeof(buffer));
    return ImmSetCompositionStringW(hIMC, dwIndex, lpComp, dwCompLen, (LPVOID)buffer, size);
}

DWORD WINAPI ImmGetCandidateListSjis(HIMC hIMC, DWORD deIndex, LPCANDIDATELIST lpCandList, DWORD dwBufLen)
{
    DWORD ret = ImmGetCandidateListW(hIMC, deIndex, lpCandList, dwBufLen);
    if (!dwBufLen) return ret;

    BYTE buffer[512] = {0};
    for (DWORD i = 0; i < lpCandList->dwCount; ++i)
    {
        BYTE *candidate = ((BYTE *)lpCandList + lpCandList->dwOffset[i]);
        int len = WideCharToMultiByte(932, 0, (LPCWCH)candidate, -1, (LPSTR)buffer, sizeof(buffer), NULL, NULL);
        strcpy((char *)candidate, (char *)buffer);
    }

    return ret;
}

DWORD WINAPI ImmGetCandidateListCountSjis(HIMC hImc, LPDWORD lpdwListCount)
{
    return ImmGetCandidateListCountW(hImc, lpdwListCount);
}

LRESULT WINAPI ImmEscapeSjis(HKL hKL, HIMC hIMC, UINT u, LPVOID lp)
{
    return ImmEscapeW(hKL, hIMC, u, lp);
}

BOOL WINAPI ImmIsUIMessageSjis(HWND hWnd, UINT u, WPARAM wparam, LPARAM lparam)
{
    return ImmIsUIMessageW(hWnd, u, wparam, lparam);
}

class ImmHijack : public IPlugin
{
    const char *GetName(void) const override
    {
        return "ImmHijack";
    }

    const char *GetAuthor(void) const override
    {
        return "xiyan";
    }

    const char *GetDescription(void) const override
    {
        return "Hook IMM related functions to convert input from unicode to cp932.";
    }

    const char *GetLink(void) const override
    {
        return "https://dummy.top/ashita-immhijack.html";
    }

    double GetVersion(void) const override
    {
        return 1.20;
    }

    double GetInterfaceVersion(void) const override
    {
        return ASHITA_INTERFACE_VERSION;
    }

    bool Initialize(IAshitaCore* core, ILogManager* logger, const uint32_t id) override
    {
        UNREFERENCED_PARAMETER(core);
        UNREFERENCED_PARAMETER(id);

        // char *ret = setlocale(LC_ALL, "Japanese_Japan.932");
        // if (ret == nullptr) return false;
        // logger->Logf(4, "ImmHijack", "Set locale to %s successfully.", ret);

        if (Hook("Imm32.dll", "ImmGetCompositionStringA", ImmGetCompositionStringSJis, ImmGetCompositionStringAOri))
        {
            logger->Log(3, "ImmHijack", "Failed to hook ImmGetCompositionStringA");
            return false;
        }
        ImmGetCompositionStringAOk = true;

        // if (Hook("Imm32.dll", "ImmGetConversionListA", ImmGetConversionListSJis, ImmGetConversionListAOri))
        // {
        //     logger->Log(3, "ImmHijack", "Failed to hook ImmGetConversionListA");
        //     return false;
        // }
        // ImmGetConversionListAOk = true;

        if (Hook("Imm32.dll", "ImmSetCompositionStringA", ImmSetCompositionStringSJis, ImmSetCompositionStringAOri))
        {
            logger->Log(3, "ImmHijack", "Failed to hook ImmSetCompositionStringA");
            return false;
        }
        ImmSetCompositionStringAOk = true;

        if (Hook("Imm32.dll", "ImmGetCandidateListA", ImmGetCandidateListSjis, ImmGetCandidateListAOri))
        {
            logger->Log(3, "ImmHijack", "Failed to hook ImmGetCandidateListA");
            return false;
        }
        ImmGetCandidateListAOk = true;

        if (Hook("Imm32.dll", "ImmGetCandidateListCountA", ImmGetCandidateListCountSjis, ImmGetCandidateListCountAOri))
        {
            logger->Log(3, "ImmHijack", "Failed to hook ImmGetCandidateListCountA");
            return false;
        }
        ImmGetCandidateListCountAOk = true;

        if (Hook("Imm32.dll", "ImmIsUIMessageA", ImmIsUIMessageSjis, ImmIsUIMessageAOri))
        {
            logger->Log(3, "ImmHijack", "Failed to hook ImmIsUIMessageA");
            return false;
        }
        ImmIsUIMessageAOk = true;

        if (Hook("Imm32.dll", "ImmEscapeA", ImmEscapeSjis, ImmEscapeAOri))
        {
            logger->Log(3, "ImmHijack", "Failed to hook ImmEscapeA");
            return false;
        }
        ImmEscapeAOk = true;

        logger->Log(1, "ImmHijack", "Imm ANSI functions is hijacked successfully.");
        return true;
    }

    void Release(void) override
    {
        // Reset to user default
        // setlocale(LC_ALL, "");

        if (ImmGetCompositionStringAOk) Unhook("Imm32.dll", "ImmGetCompositionStringA", ImmGetCompositionStringAOri);
        ImmGetCompositionStringAOk = false;
        if (ImmGetConversionListAOk) Unhook("Imm32.dll", "ImmGetConversionListA", ImmGetConversionListAOri);
        ImmGetConversionListAOk = false;
        if (ImmSetCompositionStringAOk) Unhook("Imm32.dll", "ImmSetCompositionStringA", ImmSetCompositionStringAOri);
        ImmSetCompositionStringAOk = false;
        if (ImmGetCandidateListAOk) Unhook("Imm32.dll", "ImmGetCandidateListA", ImmGetCandidateListAOri);
        ImmGetCandidateListAOk = false;
        if (ImmGetCandidateListCountAOk) Unhook("Imm32.dll", "ImmGetCandidateListCountA", ImmGetCandidateListCountAOri);
        ImmGetCandidateListCountAOk = false;
        if (ImmEscapeAOk) Unhook("Imm32.dll", "ImmEscapeA", ImmEscapeAOri);
        ImmEscapeAOk = false;
        if (ImmIsUIMessageAOk) Unhook("Imm32.dll", "ImmIsUIMessageA", ImmIsUIMessageAOri);
        ImmIsUIMessageAOk = false;
    }

    char ImmGetCompositionStringAOri[5];
    bool ImmGetCompositionStringAOk = false;
    char ImmGetConversionListAOri[5];
    bool ImmGetConversionListAOk = false;
    char ImmSetCompositionStringAOri[5];
    bool ImmSetCompositionStringAOk = false;
    char ImmGetCandidateListAOri[5];
    bool ImmGetCandidateListAOk = false;
    char ImmGetCandidateListCountAOri[5];
    bool ImmGetCandidateListCountAOk = false;
    char ImmIsUIMessageAOri[5];
    bool ImmIsUIMessageAOk = false;
    char ImmEscapeAOri[5];
    bool ImmEscapeAOk = false;
};

#pragma comment(linker, "/EXPORT:expCreatePlugin=_expCreatePlugin@4")
#pragma comment(linker, "/EXPORT:expGetInterfaceVersion=_expGetInterfaceVersion@0")

extern "C" {
    __declspec(dllexport) IPlugin* __stdcall expCreatePlugin(const char *args)
    {
        return new ImmHijack();
    }

    __declspec(dllexport) double __stdcall expGetInterfaceVersion(void)
    {
        return ASHITA_INTERFACE_VERSION;
    }
}
