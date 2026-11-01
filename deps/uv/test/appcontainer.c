/* Copyright libuv contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * appcontainer.c - Run a program inside a Windows AppContainer.
 *
 * Usage: appcontainer.exe program.exe [args...]
 *
 * Creates an AppContainer profile, grants it read/write access to the
 * current directory tree and temp directory, adds loopback exemption
 * for localhost network access, launches the given program inside the
 * AppContainer, waits for it to exit, and returns the child's exit code.
 */

#include <windows.h>
#include <userenv.h>
#include <sddl.h>
#include <aclapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Well-known capability SIDs for network access.
 * See https://devblogs.microsoft.com/oldnewthing/20220503-00/?p=106557
 */
#define INTERNET_CLIENT_SID              "S-1-15-3-1"
#define INTERNET_CLIENT_SERVER_SID       "S-1-15-3-2"
#define PRIVATE_NETWORK_CLIENT_SERVER_SID "S-1-15-3-3"

typedef LONG NTSTATUS;

NTSTATUS NTAPI NtSetSecurityObject(HANDLE, SECURITY_INFORMATION,
                                   PSECURITY_DESCRIPTOR);
ULONG NTAPI RtlNtStatusToDosError(NTSTATUS);

/* Modify the DACL on the given path for the AppContainer SID.
 * If grant, add an ACE with the given perms and inheritance.
 * If !grant, remove the SID's ACE (perms and inheritance are ignored).
 *
 * For non-inheritable ACEs, uses NtSetSecurityObject to set the DACL
 * on the single object without a tree walk.  The Win32 wrapper
 * SetNamedSecurityInfo recursively re-propagates every inheritable ACE
 * to all descendants even when the ACE being added is non-inheritable,
 * which is extremely slow on large directory trees.
 *
 * For inheritable ACEs, uses SetNamedSecurityInfo so that Windows
 * propagates the new ACE to existing descendants. */
static DWORD modify_access(PSID sid, const char* path, DWORD perms,
                           DWORD inheritance, int grant) {
  EXPLICIT_ACCESSA ea;
  PACL old_acl = NULL;
  PACL new_acl = NULL;
  PSECURITY_DESCRIPTOR sd = NULL;
  SECURITY_DESCRIPTOR sd_new;
  HANDLE h;
  NTSTATUS status;
  DWORD err;
  ULONGLONG t1 = GetTickCount64();

  /* Open the file or directory.  FILE_FLAG_BACKUP_SEMANTICS is
   * required to obtain a handle to a directory. */
  h = CreateFileA(path,
                  READ_CONTROL | WRITE_DAC,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  NULL,
                  OPEN_EXISTING,
                  FILE_FLAG_BACKUP_SEMANTICS,
                  NULL);
  if (h == INVALID_HANDLE_VALUE) {
    err = GetLastError();
    fprintf(stderr, "appcontainer: modify_access %s %s perms=0x%lx inherit=0x%lx: "
            "CreateFile error %lu (%.3f s)\n",
            grant ? "grant" : "revoke", path, perms, inheritance,
            err, (GetTickCount64() - t1) / 1000.0);
    return err;
  }

  /* Read the existing DACL. */
  err = GetSecurityInfo(h,
                        SE_FILE_OBJECT,
                        DACL_SECURITY_INFORMATION,
                        NULL, NULL, &old_acl, NULL, &sd);
  if (err != ERROR_SUCCESS) {
    fprintf(stderr, "appcontainer: modify_access %s %s perms=0x%lx inherit=0x%lx: "
            "GetSecurityInfo error %lu (%.3f s)\n",
            grant ? "grant" : "revoke", path, perms, inheritance,
            err, (GetTickCount64() - t1) / 1000.0);
    CloseHandle(h);
    return err;
  }

  /* Build updated ACL. */
  memset(&ea, 0, sizeof(ea));
  ea.grfAccessPermissions = grant ? perms : 0;
  ea.grfAccessMode = grant ? SET_ACCESS : REVOKE_ACCESS;
  ea.grfInheritance = grant ? inheritance : NO_INHERITANCE;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  ea.Trustee.ptstrName = (LPSTR)sid;

  err = SetEntriesInAclA(1, &ea, old_acl, &new_acl);
  if (err != ERROR_SUCCESS) {
    fprintf(stderr, "appcontainer: modify_access %s %s perms=0x%lx inherit=0x%lx: "
            "SetEntriesInAcl error %lu (%.3f s)\n",
            grant ? "grant" : "revoke", path, perms, inheritance,
            err, (GetTickCount64() - t1) / 1000.0);
    LocalFree(sd);
    CloseHandle(h);
    return err;
  }

  /* For non-inheritable ACEs (e.g. parent directory grants), use
   * NtSetSecurityObject to set the DACL on this single object without
   * the tree walk that SetNamedSecurityInfo performs.  For inheritable
   * ACEs, use SetNamedSecurityInfo so that Windows propagates the new
   * ACE to existing descendants. */
  if (inheritance == NO_INHERITANCE) {
    InitializeSecurityDescriptor(&sd_new, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd_new, TRUE, new_acl, FALSE);
    status = NtSetSecurityObject(h, DACL_SECURITY_INFORMATION, &sd_new);
    err = (status == 0) ? ERROR_SUCCESS : RtlNtStatusToDosError(status);
  } else {
    err = SetNamedSecurityInfoA((LPSTR)path,
                                SE_FILE_OBJECT,
                                DACL_SECURITY_INFORMATION,
                                NULL, NULL, new_acl, NULL);
  }

  fprintf(stderr, "appcontainer: modify_access %s %s perms=0x%lx inherit=0x%lx: "
          "%s (%.3f s)\n",
          grant ? "grant" : "revoke", path, perms, inheritance,
          err == ERROR_SUCCESS ? "ok" : "error",
          (GetTickCount64() - t1) / 1000.0);

  LocalFree(new_acl);
  LocalFree(sd);
  CloseHandle(h);
  return err;
}

/* Modify access on all parent directories up to the drive root.
 * If grant is true, add read+execute; otherwise revoke. */
static void modify_parents(PSID sid, const char* path, int grant) {
  char parent[MAX_PATH];
  size_t len;

  strncpy(parent, path, MAX_PATH - 1);
  parent[MAX_PATH - 1] = '\0';

  for (;;) {
    len = strlen(parent);
    while (len > 0 && parent[len - 1] != '\\')
      len--;
    if (len == 0)
      break;
    /* Keep the trailing backslash only for drive roots (e.g. C:\). */
    if (len > 1 && parent[len - 2] != ':')
      parent[len - 1] = '\0';
    else
      parent[len] = '\0';

    DWORD err = modify_access(sid, parent,
                              GENERIC_READ | GENERIC_EXECUTE,
                              NO_INHERITANCE, grant);
    if (grant && err != ERROR_SUCCESS)
      fprintf(stderr, "appcontainer: warning: grant parent %s: %lu\n",
              parent, err);

    /* Stop at drive root (e.g. "C:\"). */
    if (len <= 3)
      break;
    parent[len - 1] = '\0';
  }
}

/* Grant or revoke access to a directory and its parents. */
static void modify_dir_and_parents(PSID sid, const char* path, int grant) {
  ULONGLONG t1 = GetTickCount64();
  DWORD err = modify_access(sid, path, GENERIC_ALL,
                            CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE,
                            grant);
  if (grant && err != ERROR_SUCCESS) {
    fprintf(stderr, "appcontainer: warning: grant %s: %lu\n", path, err);
    return;
  }

  modify_parents(sid, path, grant);

  fprintf(stderr, "appcontainer: %s tree %s: %.3f s\n",
          grant ? "grant" : "revoke", path,
          (GetTickCount64() - t1) / 1000.0);
}

/* Grant or revoke the AppContainer SID's access to the NUL device.
 * uv_spawn needs to open NUL for ignored stdio handles.
 * NUL is a device object, so we must use handle-based
 * GetSecurityInfo/SetSecurityInfo (the named variants silently
 * fail on devices). */
static void modify_nul_access(PSID sid, int grant) {
  HANDLE h;
  PACL old_acl = NULL;
  PACL new_acl = NULL;
  PSECURITY_DESCRIPTOR sd = NULL;
  EXPLICIT_ACCESSA ea;
  DWORD err;
  ULONGLONG t1 = GetTickCount64();

  /* Open NUL with permission to read and modify the DACL. */
  h = CreateFileA("\\\\.\\NUL",
                  READ_CONTROL | WRITE_DAC,
                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                  NULL,
                  OPEN_EXISTING,
                  0,
                  NULL);
  if (h == INVALID_HANDLE_VALUE) {
    if (grant)
      fprintf(stderr, "appcontainer: warning: open NUL: %lu\n", GetLastError());
    return;
  }

  err = GetSecurityInfo(h,
                        SE_FILE_OBJECT,
                        DACL_SECURITY_INFORMATION,
                        NULL, NULL, &old_acl, NULL, &sd);
  if (err != ERROR_SUCCESS) {
    if (grant)
      fprintf(stderr, "appcontainer: warning: GetSecurityInfo NUL: %lu\n", err);
    CloseHandle(h);
    return;
  }

  memset(&ea, 0, sizeof(ea));
  ea.grfAccessPermissions = grant ? (GENERIC_READ | GENERIC_WRITE) : 0;
  ea.grfAccessMode = grant ? SET_ACCESS : REVOKE_ACCESS;
  ea.grfInheritance = NO_INHERITANCE;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  ea.Trustee.ptstrName = (LPSTR)sid;

  err = SetEntriesInAclA(1, &ea, old_acl, &new_acl);
  if (err != ERROR_SUCCESS) {
    if (grant)
      fprintf(stderr, "appcontainer: warning: SetEntriesInAcl NUL: %lu\n", err);
    LocalFree(sd);
    CloseHandle(h);
    return;
  }

  err = SetSecurityInfo(h,
                        SE_FILE_OBJECT,
                        DACL_SECURITY_INFORMATION,
                        NULL, NULL, new_acl, NULL);
  if (grant) {
    if (err != ERROR_SUCCESS)
      fprintf(stderr, "appcontainer: warning: SetSecurityInfo NUL: %lu\n", err);
    else
      fprintf(stderr, "appcontainer: NUL device access granted\n");
  }

  fprintf(stderr, "appcontainer: %s NUL: %.3f s\n",
          grant ? "grant" : "revoke",
          (GetTickCount64() - t1) / 1000.0);

  LocalFree(new_acl);
  LocalFree(sd);
  CloseHandle(h);
}

/* Add or remove the AppContainer SID from the loopback exemption list
 * so that TCP/UDP tests can connect to localhost. */
static void modify_loopback_exemption(PSID sid, int grant) {
  /* Load NetworkIsolationGetAppContainerConfig and NetworkIsolationSetAppContainerConfig
   * directly from FirewallAPI.dll, since netfw.h does not always contain their declarations.
   * See https://learn.microsoft.com/en-us/windows/win32/api/netfw/nf-netfw-networkisolationsetappcontainerconfig */
  typedef DWORD (WINAPI *fnGetConfig)(DWORD*, PSID_AND_ATTRIBUTES*);
  typedef DWORD (WINAPI *fnSetConfig)(DWORD, PSID_AND_ATTRIBUTES);
  HMODULE hFirewall;
  fnGetConfig getConfig;
  fnSetConfig setConfig;
  DWORD numSids = 0;
  PSID_AND_ATTRIBUTES oldSids = NULL;
  PSID_AND_ATTRIBUTES newSids = NULL;
  DWORD newCount = 0;
  DWORD err;
  DWORD i;
  ULONGLONG t1 = GetTickCount64();

  hFirewall = LoadLibraryA("FirewallAPI.dll");
  if (!hFirewall) {
    if (grant)
      fprintf(stderr, "appcontainer: warning: LoadLibrary(FirewallAPI.dll) "
              "failed: %lu\n", GetLastError());
    return;
  }

  getConfig = (fnGetConfig)GetProcAddress(hFirewall, "NetworkIsolationGetAppContainerConfig");
  setConfig = (fnSetConfig)GetProcAddress(hFirewall, "NetworkIsolationSetAppContainerConfig");
  if (!getConfig || !setConfig) {
    if (grant)
      fprintf(stderr, "appcontainer: warning: NetworkIsolation API not found\n");
    FreeLibrary(hFirewall);
    return;
  }

  /* Get current exemption list. */
  err = getConfig(&numSids, &oldSids);
  if (err != ERROR_SUCCESS) {
    if (grant)
      fprintf(stderr, "appcontainer: warning: NetworkIsolationGetAppContainerConfig: %lu\n", err);
    FreeLibrary(hFirewall);
    return;
  }

  /* Build new list: copy existing entries (excluding our SID to avoid
   * duplicates), then append our SID if granting. */
  newSids = (PSID_AND_ATTRIBUTES)malloc(
    (numSids + 1) * sizeof(SID_AND_ATTRIBUTES));
  if (!newSids) {
    FreeLibrary(hFirewall);
    return;
  }

  for (i = 0; i < numSids; i++) {
    if (!EqualSid(oldSids[i].Sid, sid))
      newSids[newCount++] = oldSids[i];
  }
  if (grant) {
    newSids[newCount].Sid = sid;
    newSids[newCount].Attributes = SE_GROUP_ENABLED;
    newCount++;
  }

  err = setConfig(newCount, newSids);
  if (grant) {
    if (err != ERROR_SUCCESS)
      fprintf(stderr, "appcontainer: warning: NetworkIsolationSetAppContainerConfig: %lu\n", err);
    else
      fprintf(stderr, "appcontainer: loopback exemption added\n");
  }

  fprintf(stderr, "appcontainer: %s loopback exemption: %.3f s\n",
          grant ? "grant" : "revoke",
          (GetTickCount64() - t1) / 1000.0);

  free(newSids);
  FreeLibrary(hFirewall);
}

/* Launch a child process inside the AppContainer and return its
 * exit code. */
static int run_child(const char* abs_exe, const char* cmdline,
                     SECURITY_CAPABILITIES* sc) {
  STARTUPINFOEXA si;
  PROCESS_INFORMATION pi;
  SIZE_T attr_size;
  DWORD exit_code = 1;

  memset(&si, 0, sizeof(si));
  memset(&pi, 0, sizeof(pi));

  /* Allocate the proc thread attribute list. */
  attr_size = 0;
  InitializeProcThreadAttributeList(NULL, 1, 0, &attr_size);
  si.StartupInfo.cb = sizeof(si);
  si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)malloc(attr_size);
  if (!si.lpAttributeList) {
    fprintf(stderr, "malloc failed\n");
    return 1;
  }
  if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0,
                                          &attr_size)) {
    fprintf(stderr, "InitializeProcThreadAttributeList failed: %lu\n",
            GetLastError());
    free(si.lpAttributeList);
    return 1;
  }

  if (!UpdateProcThreadAttribute(si.lpAttributeList,
                                 0,
                                 PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
                                 sc,
                                 sizeof(*sc),
                                 NULL,
                                 NULL)) {
    fprintf(stderr, "UpdateProcThreadAttribute failed: %lu\n", GetLastError());
    DeleteProcThreadAttributeList(si.lpAttributeList);
    free(si.lpAttributeList);
    return 1;
  }

  /* Inherit handles so stdout/stderr flow through. */
  si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  si.StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  si.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  si.StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);

  if (!CreateProcessA(abs_exe,
                      (LPSTR)cmdline,
                      NULL,
                      NULL,
                      TRUE,
                      EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW,
                      NULL,
                      NULL,
                      &si.StartupInfo,
                      &pi)) {
    fprintf(stderr, "CreateProcessA failed: %lu\n", GetLastError());
    DeleteProcThreadAttributeList(si.lpAttributeList);
    free(si.lpAttributeList);
    return 1;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  GetExitCodeProcess(pi.hProcess, &exit_code);

  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  DeleteProcThreadAttributeList(si.lpAttributeList);
  free(si.lpAttributeList);
  return (int)exit_code;
}

int main(int argc, char* argv[]) {
  /* These must be wide strings: CreateAppContainerProfile has no A variant. */
  static const wchar_t profile_name[] = L"libuv-test-appcontainer";
  static const wchar_t profile_display[] = L"libuv test";
  static const wchar_t profile_desc[] = L"AppContainer for libuv tests";

  PSID sid = NULL;
  HRESULT hr;
  char cwd[MAX_PATH];
  char abs_exe[MAX_PATH];
  char tmpdir[MAX_PATH];
  char cmdline[32768];
  SECURITY_CAPABILITIES sc;
  LPSTR sid_str = NULL;
  int i;
  int exit_code;
  size_t pos;
  ULONGLONG start_time;

  /* Capability SIDs to add. */
  PSID net_sid;
  SID_AND_ATTRIBUTES caps[1];
  int num_caps = 0;

  start_time = GetTickCount64();

  if (argc < 2) {
    fprintf(stderr, "usage: appcontainer.exe program.exe [args...]\n");
    return 1;
  }

  /* Get the current directory. */
  if (!GetCurrentDirectoryA(MAX_PATH, cwd)) {
    fprintf(stderr, "GetCurrentDirectoryA failed: %lu\n", GetLastError());
    return 1;
  }

  /* Resolve the exe to an absolute path. */
  if (!GetFullPathNameA(argv[1], MAX_PATH, abs_exe, NULL)) {
    fprintf(stderr, "GetFullPathNameA failed: %lu\n", GetLastError());
    return 1;
  }

  /* Get the temp directory. */
  if (!GetTempPathA(MAX_PATH, tmpdir)) {
    fprintf(stderr, "GetTempPathA failed: %lu\n", GetLastError());
    return 1;
  }
  {
    size_t tlen = strlen(tmpdir);
    if (tlen > 3 && tmpdir[tlen - 1] == '\\')
      tmpdir[tlen - 1] = '\0';
  }

  fprintf(stderr, "appcontainer: cwd=%s\n", cwd);
  fprintf(stderr, "appcontainer: exe=%s\n", abs_exe);
  fprintf(stderr, "appcontainer: tmp=%s\n", tmpdir);

  /* Verify the exe exists. */
  if (GetFileAttributesA(abs_exe) == INVALID_FILE_ATTRIBUTES) {
    fprintf(stderr, "appcontainer: exe not found: %s (error %lu)\n",
            abs_exe, GetLastError());
    return 1;
  }

  /* Create capability SIDs for network access. */
  if (!ConvertStringSidToSidA(INTERNET_CLIENT_SERVER_SID, &net_sid)) {
    fprintf(stderr, "appcontainer: warning: ConvertStringSidToSidA(%s): %lu\n",
            INTERNET_CLIENT_SERVER_SID, GetLastError());
  } else {
    caps[num_caps].Sid = net_sid;
    caps[num_caps].Attributes = SE_GROUP_ENABLED;
    num_caps++;
  }

  /* Delete any leftover profile from a previous run. */
  DeleteAppContainerProfile(profile_name);

  /* Create the AppContainer profile. */
  {
    ULONGLONG t1 = GetTickCount64();
    hr = CreateAppContainerProfile(profile_name,
                                   profile_display,
                                   profile_desc,
                                   num_caps > 0 ? caps : NULL,
                                   num_caps,
                                   &sid);
    fprintf(stderr, "appcontainer: CreateAppContainerProfile: %.3f s\n",
            (GetTickCount64() - t1) / 1000.0);
  }
  if (FAILED(hr)) {
    fprintf(stderr, "CreateAppContainerProfile failed: 0x%08lx\n", hr);
    return 1;
  }

  if (ConvertSidToStringSidA(sid, &sid_str)) {
    fprintf(stderr, "appcontainer: SID=%s\n", sid_str);
    LocalFree(sid_str);
  }

  /* Grant access. */
  {
    ULONGLONG t1 = GetTickCount64();
    modify_dir_and_parents(sid, cwd, 1);
    modify_dir_and_parents(sid, tmpdir, 1);
    modify_nul_access(sid, 1);
    modify_loopback_exemption(sid, 1);
    fprintf(stderr, "appcontainer: total setup grants: %.3f s\n",
            (GetTickCount64() - t1) / 1000.0);
  }

  /* Set up SECURITY_CAPABILITIES. */
  memset(&sc, 0, sizeof(sc));
  sc.AppContainerSid = sid;
  if (num_caps > 0) {
    sc.Capabilities = caps;
    sc.CapabilityCount = num_caps;
  }

  /* Build the command line. */
  pos = 0;
  cmdline[pos++] = '"';
  {
    size_t elen = strlen(abs_exe);
    memcpy(&cmdline[pos], abs_exe, elen);
    pos += elen;
  }
  cmdline[pos++] = '"';

  for (i = 2; i < argc; i++) {
    size_t arglen = strlen(argv[i]);
    cmdline[pos++] = ' ';
    cmdline[pos++] = '"';
    memcpy(&cmdline[pos], argv[i], arglen);
    pos += arglen;
    cmdline[pos++] = '"';
  }
  cmdline[pos] = '\0';

  fprintf(stderr, "appcontainer: launching: %s\n", cmdline);

  {
    ULONGLONG child_start = GetTickCount64();
    exit_code = run_child(abs_exe, cmdline, &sc);
    fprintf(stderr, "appcontainer: child exited with code %d (%.3f s)\n",
            exit_code, (GetTickCount64() - child_start) / 1000.0);
  }

  /* Tear down the sandbox: revoke all grants so we don't leave
   * stale SIDs on directories, the NUL device, or the firewall. */
  {
    ULONGLONG t1 = GetTickCount64();
    modify_dir_and_parents(sid, cwd, 0);
    modify_dir_and_parents(sid, tmpdir, 0);
    modify_nul_access(sid, 0);
    modify_loopback_exemption(sid, 0);
    fprintf(stderr, "appcontainer: total teardown revokes: %.3f s\n",
            (GetTickCount64() - t1) / 1000.0);
  }

  if (net_sid)
    LocalFree(net_sid);
  FreeSid(sid);
  DeleteAppContainerProfile(profile_name);
  fprintf(stderr, "appcontainer: elapsed %.3f s\n",
          (GetTickCount64() - start_time) / 1000.0);
  return exit_code;
}
