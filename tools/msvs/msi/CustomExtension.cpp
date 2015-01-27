
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <msiquery.h>
#include <wcautil.h>

UINT WINAPI BroadcastEnvironmentUpdate(MSIHANDLE hInstall) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, "BroadcastEnvironmentUpdate");
  ExitOnFailure(hr, "Failed to initialize");

  WcaLog(LOGMSG_STANDARD, "Initialized.");

  SendMessageTimeout(HWND_BROADCAST,
                     WM_SETTINGCHANGE,
                     0,
                     (LPARAM)L"Environment",
                     SMTO_ABORTIFHUNG,
                     5000,
                     NULL);

LExit:
  er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
  return WcaFinalize(er);
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hInst, ULONG ulReason, LPVOID) {
  switch (ulReason) {
    case DLL_PROCESS_ATTACH:
      WcaGlobalInitialize(hInst);
      break;

    case DLL_PROCESS_DETACH:
      WcaGlobalFinalize();
      break;
  }

  return TRUE;
}
