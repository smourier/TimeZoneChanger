#include <windows.h>
#define UCHAR_TYPE wchar_t
#include <icu.h>
#pragma comment(lib, "icu.lib")

#include <iostream>
#include <vector>

#include "MinHook\include\MinHook.h"
#pragma comment(lib, "MinHook\\lib\\libMinHook.x64.lib")

#define NETHOST_USE_AS_STATIC
#include "Hosting\coreclr_delegates.h"
#include "Hosting\hostfxr.h"
#include "Hosting\nethost.h"

#if _DEBUG
#pragma comment(lib, "hosting\\nethost.lib")
#else
#pragma comment(lib, "hosting\\libnethost.lib")
#endif

typedef struct REG_TZI_FORMAT
{
	LONG Bias;
	LONG StandardBias;
	LONG DaylightBias;
	SYSTEMTIME StandardDate;
	SYSTEMTIME DaylightDate;
};

typedef DWORD(WINAPI* GetDynamicTimeZoneInformation_fn)(PDYNAMIC_TIME_ZONE_INFORMATION);

GetDynamicTimeZoneInformation_fn getDynamicTimeZoneInformation = nullptr;
const wchar_t* timeZoneId = nullptr;
DYNAMIC_TIME_ZONE_INFORMATION hookedTzi{};

static DWORD WINAPI GetDynamicTimeZoneInformationHook(PDYNAMIC_TIME_ZONE_INFORMATION pTimeZoneInformation)
{
	if (pTimeZoneInformation)
	{
		CopyMemory(pTimeZoneInformation, &hookedTzi, sizeof(hookedTzi));

		// dotnet doesn't seem to care
		// cf https://github.com/dotnet/runtime/blob/main/src/libraries/System.Private.CoreLib/src/System/TimeZoneInfo.Win32.cs#L286
		return TIME_ZONE_ID_DAYLIGHT;
	}
	return TIME_ZONE_ID_INVALID;
}

hostfxr_initialize_for_dotnet_command_line_fn init_for_cmd_line_fptr;
hostfxr_run_app_fn run_app_fptr;
hostfxr_close_fn close_fptr;

static HRESULT load_hostfxr()
{
	char_t buffer[MAX_PATH];
	size_t buffer_size = sizeof(buffer) / sizeof(char_t);
	auto hr = get_hostfxr_path(buffer, &buffer_size, nullptr);
	if (FAILED(hr))
		return hr;

	auto lib = LoadLibrary(buffer);
	if (!lib)
		return HRESULT_FROM_WIN32(GetLastError());

	init_for_cmd_line_fptr = (hostfxr_initialize_for_dotnet_command_line_fn)GetProcAddress(lib, "hostfxr_initialize_for_dotnet_command_line");
	run_app_fptr = (hostfxr_run_app_fn)GetProcAddress(lib, "hostfxr_run_app");
	close_fptr = (hostfxr_close_fn)GetProcAddress(lib, "hostfxr_close");

	if (!init_for_cmd_line_fptr || !run_app_fptr || !close_fptr)
		return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);

	return S_OK;
}

int wmain(int argc, const wchar_t** argv)
{
	SYSTEMTIME st = {};
	GetSystemTime(&st);
	wprintf(L"TimeZoneChanger - Copyright (C) 2024-%u Simon Mourier. All rights reserved.\n", st.wYear);
	wprintf(L"Runs a .NET Core program under a specific time zone.\n");
	DYNAMIC_TIME_ZONE_INFORMATION tzi{};
	GetDynamicTimeZoneInformation(&tzi);
	wprintf(L"\n");
	wprintf(L"Current Time Zone id is '%s'.\n", tzi.StandardName);

	if (argc < 3)
	{
		wprintf(L"\n");
		wprintf(L"Format is TimeZoneChanger.exe <dll path> <time zone id> ... other arguments ...\n");
		wprintf(L"\n");
		wprintf(L"Example: TimeZoneChanger.exe c:\\mypath\\myexe.dll America/Guadeloupe\n");
		return -1;
	}

	auto dllPath = argv[1];
	timeZoneId = *(&argv[2]);

	// try to get a Windows ID
	wchar_t windowsId[128] = L"";
	UErrorCode status = U_ZERO_ERROR;

	char region[64] = "";
	*windowsId = 0;
	ucal_getTimeZoneIDForWindowsID(timeZoneId, -1, region, windowsId, ARRAYSIZE(windowsId), &status);
	if (U_FAILURE(status) || *windowsId == 0)
	{
		// maybe IANA?
		ucal_getWindowsTimeZoneID(timeZoneId, -1, windowsId, ARRAYSIZE(windowsId), &status);
		if (U_FAILURE(status) || *windowsId == 0)
		{
			wprintf(L"TZC Error getting Windows time zone id from '%s'. Be aware that IANA time zone ids are case-sensitive.\n", timeZoneId);
			MH_Uninitialize();
			return 1;
		}
	}
	else
	{
		wcscpy_s(windowsId, timeZoneId);
	}

	wprintf(L"Hook Windows Time Zone id is '%s'.\n", windowsId);

	// read Time Zone info from registry
	// https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/ns-timezoneapi-time_zone_information
	std::wstring regPath = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\";
	regPath += windowsId;
	HKEY key;
	if (RegOpenKey(HKEY_LOCAL_MACHINE, regPath.c_str(), &key) != ERROR_SUCCESS)
	{
		wprintf(L"TZC Error loading reading time zone information from registry.\n");
		MH_Uninitialize();
		return -1;
	}

	DWORD size = 32 * 2;
	RegGetValue(key, nullptr, L"Std", RRF_RT_ANY, nullptr, &hookedTzi.StandardName, &size);
	RegGetValue(key, nullptr, L"Dlt", RRF_RT_ANY, nullptr, &hookedTzi.DaylightName, &size);

	REG_TZI_FORMAT tziFormat{};
	size = sizeof(REG_TZI_FORMAT);
	RegGetValue(key, nullptr, L"TZI", RRF_RT_ANY, nullptr, &tziFormat, &size);

	hookedTzi.Bias = tziFormat.Bias;
	hookedTzi.DaylightBias = tziFormat.DaylightBias;
	hookedTzi.DaylightDate = tziFormat.DaylightDate;
	hookedTzi.StandardBias = tziFormat.StandardBias;
	hookedTzi.StandardDate = tziFormat.StandardDate;
	wcscpy_s(hookedTzi.TimeZoneKeyName, windowsId);

	RegCloseKey(key);

	// build & enable hook
	auto mh = MH_Initialize();
	if (mh != MH_OK)
	{
		wprintf(L"TZC Error %u initializing minhook.\n", mh);
		return -1;
	}

	mh = MH_CreateHook(&GetDynamicTimeZoneInformation, &GetDynamicTimeZoneInformationHook, (LPVOID*)(&getDynamicTimeZoneInformation));
	if (mh != MH_OK)
	{
		wprintf(L"TZC Error %u hooking GetDynamicTimeZoneInformation.\n", mh);
		MH_Uninitialize();
		return 1;
	}

	mh = MH_EnableHook(&GetDynamicTimeZoneInformation);
	if (mh != MH_OK)
	{
		wprintf(L"TZC Error %u enabling GetDynamicTimeZoneInformation hook.\n", mh);
		MH_Uninitialize();
		return 1;
	}

	// load .NET runtime
	auto hr = load_hostfxr();
	if (FAILED(hr))
	{
		wprintf(L"TZC Error loading hostfxr, make sure DotNet is installed.\n");
		MH_Uninitialize();
		return -1;
	}

	// build command line
	hostfxr_handle h;
	std::vector<const char_t*> args{ dllPath };

	// add arguments
	for (auto i = 3; i < argc; i++)
	{
		args.push_back(argv[i]);
	}

	hr = (HRESULT)init_for_cmd_line_fptr((int)args.size(), args.data(), nullptr, &h);
	if (FAILED(hr))
	{
		wprintf(L"TZC Error initializing '%s' application: 0x%08X\n", argv[0], hr);
		MH_Uninitialize();
		return -1;
	}

	// run & close app
	wprintf(L"\n");
	auto ret = run_app_fptr(h);
	close_fptr(h);

	// disable hook
	mh = MH_DisableHook(&GetDynamicTimeZoneInformation);
	if (mh != MH_OK)
	{
		wprintf(L"TZC Error %u disabling GetDynamicTimeZoneInformation hook.\n", mh);
		MH_Uninitialize();
		return 1;
	}

	MH_Uninitialize();
	return ret;
}
