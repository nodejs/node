#include "stdafx.h"

UINT __stdcall BroadcastPathUpdate(MSIHANDLE hInstall) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, "BroadcastPathUpdateCustomAction");
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


// DllMain - Initialize and cleanup WiX custom action utils.
extern "C" BOOL WINAPI DllMain(__in HINSTANCE hInst,
                               __in ULONG ulReason,
                               __in LPVOID) {
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
