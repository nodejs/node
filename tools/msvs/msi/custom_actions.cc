#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <msiquery.h>
#include <wcautil.h>
#include <sddl.h>
#include <Lmcons.h>

#define GUID_BUFFER_SIZE 39 // {8-4-4-4-12}\0


extern "C" UINT WINAPI SetInstallScope(MSIHANDLE hInstall) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;
  PMSIHANDLE hDB;
  PMSIHANDLE hView;
  PMSIHANDLE hRecord;

  hr = WcaInitialize(hInstall, "SetInstallScope");
  ExitOnFailure(hr, "Failed to initialize");

  hDB = MsiGetActiveDatabase(hInstall);
  ExitOnNull(hDB, hr, S_FALSE, "Failed to get active database");

  LPCTSTR query = TEXT("SELECT DISTINCT UpgradeCode FROM Upgrade");
  er = MsiDatabaseOpenView(hDB, query, &hView);
  ExitOnWin32Error(er, hr, "Failed MsiDatabaseOpenView");

  er = MsiViewExecute(hView, 0);
  ExitOnWin32Error(er, hr, "Failed MsiViewExecute");

  for (;;) {
    er = MsiViewFetch(hView, &hRecord);
    if (er == ERROR_NO_MORE_ITEMS) break;
    ExitOnWin32Error(er, hr, "Failed MsiViewFetch");

    TCHAR upgrade_code[GUID_BUFFER_SIZE];
    DWORD upgrade_code_len = GUID_BUFFER_SIZE;
    er = MsiRecordGetString(hRecord, 1, upgrade_code, &upgrade_code_len);
    ExitOnWin32Error(er, hr, "Failed to read UpgradeCode");

    DWORD iProductIndex;
    for (iProductIndex = 0;; iProductIndex++) {
      TCHAR product_code[GUID_BUFFER_SIZE];
      er = MsiEnumRelatedProducts(upgrade_code, 0, iProductIndex,
                                  product_code);
      if (er == ERROR_NO_MORE_ITEMS) break;
      ExitOnWin32Error(er, hr, "Failed to get related product code");

      TCHAR assignment_type[2];
      DWORD assignment_type_len = 2;
      er = MsiGetProductInfo(product_code, INSTALLPROPERTY_ASSIGNMENTTYPE,
                             assignment_type, &assignment_type_len);
      ExitOnWin32Error(er, hr, "Failed to get the assignment type property "
                       "from related product");

      // '0' = per-user; '1' = per-machine
      if (assignment_type[0] == '0') {
        /* When old versions which were installed as per-user are detected,
         * the installation scope has to be set to per-user to be able to do
         * an upgrade. If not, two versions will be installed side-by-side:
         * one as per-user and the other as per-machine.
         *
         * If we wanted to disable backward compatibility, the installer
         * should abort here, and request the previous version to be manually
         * uninstalled before installing this one.
         */
        er = MsiSetProperty(hInstall, TEXT("ALLUSERS"), TEXT(""));
        ExitOnWin32Error(er, hr, "Failed to set the install scope to per-user");
        goto LExit;
      }
    }
  }

LExit:
  // Always succeed. This should not block the installation.
  return WcaFinalize(ERROR_SUCCESS);
}


extern "C" UINT WINAPI BroadcastEnvironmentUpdate(MSIHANDLE hInstall) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;

  hr = WcaInitialize(hInstall, "BroadcastEnvironmentUpdate");
  ExitOnFailure(hr, "Failed to initialize");

  SendMessageTimeoutW(HWND_BROADCAST,
                      WM_SETTINGCHANGE,
                      0,
                      (LPARAM) L"Environment",
                      SMTO_ABORTIFHUNG,
                      5000,
                      NULL);

LExit:
  er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
  return WcaFinalize(er);
}

#define AUTHENTICATED_USERS_SID L"S-1-5-11"

extern "C" UINT WINAPI GetLocalizedUserNames(MSIHANDLE hInstall) {
  HRESULT hr = S_OK;
  UINT er = ERROR_SUCCESS;
  TCHAR userName[UNLEN + 1] = {0};
  DWORD userNameSize = UNLEN + 1;
  TCHAR domain[DNLEN + 1] = {0};
  DWORD domainSize = DNLEN + 1;
  PSID sid;
  SID_NAME_USE nameUse;

  hr = WcaInitialize(hInstall, "GetLocalizedUserNames");
  ExitOnFailure(hr, "Failed to initialize");
  
  er = ConvertStringSidToSidW(AUTHENTICATED_USERS_SID, &sid);
  ExitOnLastError(er, "Failed to convert security identifier");

  er = LookupAccountSidW(NULL, sid, userName, &userNameSize, domain, &domainSize, &nameUse);
  ExitOnLastError(er, "Failed to lookup security identifier");

  MsiSetProperty(hInstall, L"AUTHENTICATED_USERS", userName);
  ExitOnWin32Error(er, hr, "Failed to set localized Authenticated User name");

LExit:
  er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
  LocalFree(sid);
  return WcaFinalize(er);
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hInst, ULONG ulReason, VOID* dummy) {
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
