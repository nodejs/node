/**
 * Copyright (c) 2013-2015, Christopher Jeffrey, Peter Sunde (MIT License)
 * Copyright (c) 2016, Daniel Imms (MIT License).
 * Copyright (c) 2018, Microsoft Corporation (MIT License).
 *
 * pty.cc:
 *   This file is responsible for starting processes
 *   with pseudo-terminal file descriptors.
 */

#include <iostream>
#include <nan.h>
#include <Shlwapi.h> // PathCombine, PathIsRelative
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <winpty.h>

#include "path_util.h"

/**
* Misc
*/
extern "C" void init(v8::Local<v8::Object>);

#define WINPTY_DBG_VARIABLE TEXT("WINPTYDBG")

/**
* winpty
*/
static std::vector<winpty_t *> ptyHandles;
static volatile LONG ptyCounter;

/**
* Helpers
*/

static winpty_t *get_pipe_handle(int handle) {
  for (size_t i = 0; i < ptyHandles.size(); ++i) {
    winpty_t *ptyHandle = ptyHandles[i];
    int current = (int)winpty_agent_process(ptyHandle);
    if (current == handle) {
      return ptyHandle;
    }
  }
  return nullptr;
}

static bool remove_pipe_handle(int handle) {
  for (size_t i = 0; i < ptyHandles.size(); ++i) {
    winpty_t *ptyHandle = ptyHandles[i];
    if ((int)winpty_agent_process(ptyHandle) == handle) {
      winpty_free(ptyHandle);
      ptyHandles.erase(ptyHandles.begin() + i);
      ptyHandle = nullptr;
      return true;
    }
  }
  return false;
}

void throw_winpty_error(const char *generalMsg, winpty_error_ptr_t error_ptr) {
  std::stringstream why;
  std::wstring msg(winpty_error_msg(error_ptr));
  std::string msg_(msg.begin(), msg.end());
  why << generalMsg << ": " << msg_;
  Nan::ThrowError(why.str().c_str());
  winpty_error_free(error_ptr);
}

static NAN_METHOD(PtyGetExitCode) {
  Nan::HandleScope scope;

  if (info.Length() != 1 ||
      !info[0]->IsNumber()) {
    Nan::ThrowError("Usage: pty.getExitCode(pidHandle)");
    return;
  }

  DWORD exitCode = 0;
  GetExitCodeProcess((HANDLE)info[0]->IntegerValue(Nan::GetCurrentContext()).FromJust(), &exitCode);

  info.GetReturnValue().Set(Nan::New<v8::Number>(exitCode));
}

static NAN_METHOD(PtyGetProcessList) {
  Nan::HandleScope scope;

  if (info.Length() != 1 ||
      !info[0]->IsNumber()) {
    Nan::ThrowError("Usage: pty.getProcessList(pid)");
    return;
  }

  int pid = info[0]->Int32Value(Nan::GetCurrentContext()).FromJust();

  winpty_t *pc = get_pipe_handle(pid);
  if (pc == nullptr) {
    info.GetReturnValue().Set(Nan::New<v8::Array>(0));
    return;
  }
  int processList[64];
  const int processCount = 64;
  int actualCount = winpty_get_console_process_list(pc, processList, processCount, nullptr);

  v8::Local<v8::Array> result = Nan::New<v8::Array>(actualCount);
  for (uint32_t i = 0; i < actualCount; i++) {
    Nan::Set(result, i, Nan::New<v8::Number>(processList[i]));
  }
  info.GetReturnValue().Set(result);
}

static NAN_METHOD(PtyStartProcess) {
  Nan::HandleScope scope;

  if (info.Length() != 7 ||
      !info[0]->IsString() ||
      !info[1]->IsString() ||
      !info[2]->IsArray() ||
      !info[3]->IsString() ||
      !info[4]->IsNumber() ||
      !info[5]->IsNumber() ||
      !info[6]->IsBoolean()) {
    Nan::ThrowError("Usage: pty.startProcess(file, cmdline, env, cwd, cols, rows, debug)");
    return;
  }

  std::stringstream why;

  const wchar_t *filename = path_util::to_wstring(Nan::Utf8String(info[0]));
  const wchar_t *cmdline = path_util::to_wstring(Nan::Utf8String(info[1]));
  const wchar_t *cwd = path_util::to_wstring(Nan::Utf8String(info[3]));

  // create environment block
  std::wstring env;
  const v8::Local<v8::Array> envValues = v8::Local<v8::Array>::Cast(info[2]);
  if (!envValues.IsEmpty()) {

    std::wstringstream envBlock;

    for(uint32_t i = 0; i < envValues->Length(); i++) {
      std::wstring envValue(path_util::to_wstring(Nan::Utf8String(Nan::Get(envValues, i).ToLocalChecked())));
      envBlock << envValue << L'\0';
    }

    env = envBlock.str();
  }

  // use environment 'Path' variable to determine location of
  // the relative path that we have recieved (e.g cmd.exe)
  std::wstring shellpath;
  if (::PathIsRelativeW(filename)) {
    shellpath = path_util::get_shell_path(filename);
  } else {
    shellpath = filename;
  }

  std::string shellpath_(shellpath.begin(), shellpath.end());

  if (shellpath.empty() || !path_util::file_exists(shellpath)) {
    why << "File not found: " << shellpath_;
    Nan::ThrowError(why.str().c_str());
    goto cleanup;
  }

  int cols = info[4]->Int32Value(Nan::GetCurrentContext()).FromJust();
  int rows = info[5]->Int32Value(Nan::GetCurrentContext()).FromJust();
  bool debug = Nan::To<bool>(info[6]).FromJust();

  // Enable/disable debugging
  SetEnvironmentVariable(WINPTY_DBG_VARIABLE, debug ? "1" : NULL); // NULL = deletes variable

  // Create winpty config
  winpty_error_ptr_t error_ptr = nullptr;
  winpty_config_t* winpty_config = winpty_config_new(0, &error_ptr);
  if (winpty_config == nullptr) {
    throw_winpty_error("Error creating WinPTY config", error_ptr);
    goto cleanup;
  }
  winpty_error_free(error_ptr);

  // Set pty size on config
  winpty_config_set_initial_size(winpty_config, cols, rows);

  // Start the pty agent
  winpty_t *pc = winpty_open(winpty_config, &error_ptr);
  winpty_config_free(winpty_config);
  if (pc == nullptr) {
    throw_winpty_error("Error launching WinPTY agent", error_ptr);
    goto cleanup;
  }
  winpty_error_free(error_ptr);

  // Save pty struct for later use
  ptyHandles.insert(ptyHandles.end(), pc);

  // Create winpty spawn config
  winpty_spawn_config_t* config = winpty_spawn_config_new(WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN, shellpath.c_str(), cmdline, cwd, env.c_str(), &error_ptr);
  if (config == nullptr) {
    throw_winpty_error("Error creating WinPTY spawn config", error_ptr);
    goto cleanup;
  }
  winpty_error_free(error_ptr);

  // Spawn the new process
  HANDLE handle = nullptr;
  BOOL spawnSuccess = winpty_spawn(pc, config, &handle, nullptr, nullptr, &error_ptr);
  winpty_spawn_config_free(config);
  if (!spawnSuccess) {
    throw_winpty_error("Unable to start terminal process", error_ptr);
    goto cleanup;
  }
  winpty_error_free(error_ptr);

  // Set return values
  v8::Local<v8::Object> marshal = Nan::New<v8::Object>();
  Nan::Set(marshal, Nan::New<v8::String>("innerPid").ToLocalChecked(), Nan::New<v8::Number>((int)GetProcessId(handle)));
  Nan::Set(marshal, Nan::New<v8::String>("innerPidHandle").ToLocalChecked(), Nan::New<v8::Number>((int)handle));
  Nan::Set(marshal, Nan::New<v8::String>("pid").ToLocalChecked(), Nan::New<v8::Number>((int)winpty_agent_process(pc)));
  Nan::Set(marshal, Nan::New<v8::String>("pty").ToLocalChecked(), Nan::New<v8::Number>(InterlockedIncrement(&ptyCounter)));
  Nan::Set(marshal, Nan::New<v8::String>("fd").ToLocalChecked(), Nan::New<v8::Number>(-1));
  {
    LPCWSTR coninPipeName = winpty_conin_name(pc);
    std::wstring coninPipeNameWStr(coninPipeName);
    std::string coninPipeNameStr(coninPipeNameWStr.begin(), coninPipeNameWStr.end());
    Nan::Set(marshal, Nan::New<v8::String>("conin").ToLocalChecked(), Nan::New<v8::String>(coninPipeNameStr).ToLocalChecked());
    LPCWSTR conoutPipeName = winpty_conout_name(pc);
    std::wstring conoutPipeNameWStr(conoutPipeName);
    std::string conoutPipeNameStr(conoutPipeNameWStr.begin(), conoutPipeNameWStr.end());
    Nan::Set(marshal, Nan::New<v8::String>("conout").ToLocalChecked(), Nan::New<v8::String>(conoutPipeNameStr).ToLocalChecked());
  }
  info.GetReturnValue().Set(marshal);

  goto cleanup;

cleanup:
  delete filename;
  delete cmdline;
  delete cwd;
}

static NAN_METHOD(PtyResize) {
  Nan::HandleScope scope;

  if (info.Length() != 3 ||
      !info[0]->IsNumber() ||
      !info[1]->IsNumber() ||
      !info[2]->IsNumber()) {
    Nan::ThrowError("Usage: pty.resize(pid, cols, rows)");
    return;
  }

  int handle = info[0]->Int32Value(Nan::GetCurrentContext()).FromJust();
  int cols = info[1]->Int32Value(Nan::GetCurrentContext()).FromJust();
  int rows = info[2]->Int32Value(Nan::GetCurrentContext()).FromJust();

  winpty_t *pc = get_pipe_handle(handle);

  if (pc == nullptr) {
    Nan::ThrowError("The pty doesn't appear to exist");
    return;
  }
  BOOL success = winpty_set_size(pc, cols, rows, nullptr);
  if (!success) {
    Nan::ThrowError("The pty could not be resized");
    return;
  }

  return info.GetReturnValue().SetUndefined();
}

static NAN_METHOD(PtyKill) {
  Nan::HandleScope scope;

  if (info.Length() != 2 ||
      !info[0]->IsNumber() ||
      !info[1]->IsNumber()) {
    Nan::ThrowError("Usage: pty.kill(pid, innerPidHandle)");
    return;
  }

  int handle = info[0]->Int32Value(Nan::GetCurrentContext()).FromJust();
  HANDLE innerPidHandle = (HANDLE)info[1]->Int32Value(Nan::GetCurrentContext()).FromJust();

  winpty_t *pc = get_pipe_handle(handle);
  if (pc == nullptr) {
    Nan::ThrowError("Pty seems to have been killed already");
    return;
  }

  assert(remove_pipe_handle(handle));
  CloseHandle(innerPidHandle);

  return info.GetReturnValue().SetUndefined();
}

/**
* Init
*/

extern "C" void init(v8::Local<v8::Object> target) {
  Nan::HandleScope scope;
  Nan::SetMethod(target, "startProcess", PtyStartProcess);
  Nan::SetMethod(target, "resize", PtyResize);
  Nan::SetMethod(target, "kill", PtyKill);
  Nan::SetMethod(target, "getExitCode", PtyGetExitCode);
  Nan::SetMethod(target, "getProcessList", PtyGetProcessList);
};

NODE_MODULE(pty, init);
