/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#include <assert.h>
#include <direct.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "uv.h"
#include "internal.h"

#include <iphlpapi.h>
#include <psapi.h>
#include <tlhelp32.h>


/*
 * Max title length; the only thing MSDN tells us about the maximum length
 * of the console title is that it is smaller than 64K. However in practice
 * it is much smaller, and there is no way to figure out what the exact length
 * of the title is or can be, at least not on XP. To make it even more
 * annoying, GetConsoleTitle failes when the buffer to be read into is bigger
 * than the actual maximum length. So we make a conservative guess here;
 * just don't put the novel you're writing in the title, unless the plot
 * survives truncation.
 */
#define MAX_TITLE_LENGTH 8192

/* The number of nanoseconds in one second. */
#undef NANOSEC
#define NANOSEC 1000000000


/* Cached copy of the process title, plus a mutex guarding it. */
static char *process_title;
static CRITICAL_SECTION process_title_lock;

/* The tick frequency of the high-resolution clock. */
static uint64_t hrtime_frequency_ = 0;


/*
 * One-time intialization code for functionality defined in util.c.
 */
void uv__util_init() {
  /* Initialize process title access mutex. */
  InitializeCriticalSection(&process_title_lock);

  /* Retrieve high-resolution timer frequency. */
  if (!QueryPerformanceFrequency((LARGE_INTEGER*) &hrtime_frequency_))
    hrtime_frequency_ = 0;
}


int uv_utf16_to_utf8(const wchar_t* utf16Buffer, size_t utf16Size,
    char* utf8Buffer, size_t utf8Size) {
  return WideCharToMultiByte(CP_UTF8,
                             0,
                             utf16Buffer,
                             utf16Size,
                             utf8Buffer,
                             utf8Size,
                             NULL,
                             NULL);
}


int uv_utf8_to_utf16(const char* utf8Buffer, wchar_t* utf16Buffer,
    size_t utf16Size) {
  return MultiByteToWideChar(CP_UTF8,
                             0,
                             utf8Buffer,
                             -1,
                             utf16Buffer,
                             utf16Size);
}


int uv_exepath(char* buffer, size_t* size_ptr) {
  int utf8_len, utf16_buffer_len, utf16_len;
  WCHAR* utf16_buffer;

  if (buffer == NULL || size_ptr == NULL || *size_ptr == 0) {
    return -1;
  }

  if (*size_ptr > 32768) {
    /* Windows paths can never be longer than this. */
    utf16_buffer_len = 32768;
  } else {
    utf16_buffer_len = (int) *size_ptr;
  }

  utf16_buffer = (wchar_t*) malloc(sizeof(WCHAR) * utf16_buffer_len);
  if (!utf16_buffer) {
    return -1;
  }

  /* Get the path as UTF-16. */
  utf16_len = GetModuleFileNameW(NULL, utf16_buffer, utf16_buffer_len);
  if (utf16_len <= 0) {
    goto error;
  }

  /* utf16_len contains the length, *not* including the terminating null. */
  utf16_buffer[utf16_len] = L'\0';

  /* Convert to UTF-8 */
  utf8_len = WideCharToMultiByte(CP_UTF8,
                                 0,
                                 utf16_buffer,
                                 -1,
                                 buffer,
                                 *size_ptr > INT_MAX ? INT_MAX : (int) *size_ptr,
                                 NULL,
                                 NULL);
  if (utf8_len == 0) {
    goto error;
  }

  free(utf16_buffer);

  /* utf8_len *does* include the terminating null at this point, but the */
  /* returned size shouldn't. */
  *size_ptr = utf8_len - 1;
  return 0;

 error:
  free(utf16_buffer);
  return -1;
}


uv_err_t uv_cwd(char* buffer, size_t size) {
  DWORD utf16_len;
  WCHAR utf16_buffer[MAX_PATH + 1];
  int r;

  if (buffer == NULL || size == 0) {
    return uv__new_artificial_error(UV_EINVAL);
  }

  utf16_len = GetCurrentDirectoryW(MAX_PATH, utf16_buffer);
  if (utf16_len == 0) {
    return uv__new_sys_error(GetLastError());
  }

  /* utf16_len contains the length, *not* including the terminating null. */
  utf16_buffer[utf16_len] = L'\0';

  /* The returned directory should not have a trailing slash, unless it */
  /* points at a drive root, like c:\. Remove it if needed.*/
  if (utf16_buffer[utf16_len - 1] == L'\\' &&
      !(utf16_len == 3 && utf16_buffer[1] == L':')) {
    utf16_len--;
    utf16_buffer[utf16_len] = L'\0';
  }

  /* Convert to UTF-8 */
  r = WideCharToMultiByte(CP_UTF8,
                          0,
                          utf16_buffer,
                          -1,
                          buffer,
                          size > INT_MAX ? INT_MAX : (int) size,
                          NULL,
                          NULL);
  if (r == 0) {
    return uv__new_sys_error(GetLastError());
  }

  return uv_ok_;
}


uv_err_t uv_chdir(const char* dir) {
  uv_err_t err;
  wchar_t* utf16Buffer = NULL;
  size_t utf16Size;

  if (!dir) {
    err.code = UV_EINVAL;
    goto done;
  }

  utf16Size = uv_utf8_to_utf16(dir, NULL, 0);
  if (!utf16Size) {
    err = uv__new_sys_error(GetLastError());
    goto done;
  }

  utf16Buffer = (wchar_t*)malloc(sizeof(wchar_t) * utf16Size);
  if (!utf16Buffer) {
    err.code = UV_ENOMEM;
    goto done;
  }

  if (!uv_utf8_to_utf16(dir, utf16Buffer, utf16Size)) {
    err = uv__new_sys_error(GetLastError());
    goto done;
  }

  if (_wchdir(utf16Buffer) == -1) {
    err = uv__new_sys_error(_doserrno);
    goto done;
  }

  err = uv_ok_;

done:
  if (utf16Buffer) {
    free(utf16Buffer);
  }

  return err;
}


void uv_loadavg(double avg[3]) {
  /* Can't be implemented */
  avg[0] = avg[1] = avg[2] = 0;
}


uint64_t uv_get_free_memory(void) {
  MEMORYSTATUSEX memory_status;
  memory_status.dwLength = sizeof(memory_status);

  if(!GlobalMemoryStatusEx(&memory_status))
  {
     return -1;
  }

  return (uint64_t)memory_status.ullAvailPhys;
}


uint64_t uv_get_total_memory(void) {
  MEMORYSTATUSEX memory_status;
  memory_status.dwLength = sizeof(memory_status);

  if(!GlobalMemoryStatusEx(&memory_status))
  {
    return -1;
  }

  return (uint64_t)memory_status.ullTotalPhys;
}


int uv_parent_pid() {
  int parent_pid = -1;
  HANDLE handle;
  PROCESSENTRY32 pe;
  int current_pid = GetCurrentProcessId();

  pe.dwSize = sizeof(PROCESSENTRY32);
  handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

  if (Process32First(handle, &pe)) {
    do {
      if (pe.th32ProcessID == current_pid) {
        parent_pid = pe.th32ParentProcessID;
        break;
      }
    } while( Process32Next(handle, &pe));
  }

  CloseHandle(handle);
  return parent_pid;
}


char** uv_setup_args(int argc, char** argv) {
  return argv;
}


uv_err_t uv_set_process_title(const char* title) {
  uv_err_t err;
  int length;
  wchar_t* title_w = NULL;

  uv__once_init();

  /* Find out how big the buffer for the wide-char title must be */
  length = uv_utf8_to_utf16(title, NULL, 0);
  if (!length) {
    err = uv__new_sys_error(GetLastError());
    goto done;
  }

  /* Convert to wide-char string */
  title_w = (wchar_t*)malloc(sizeof(wchar_t) * length);
  if (!title_w) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  length = uv_utf8_to_utf16(title, title_w, length);
  if (!length) {
    err = uv__new_sys_error(GetLastError());
    goto done;
  };

  /* If the title must be truncated insert a \0 terminator there */
  if (length > MAX_TITLE_LENGTH) {
    title_w[MAX_TITLE_LENGTH - 1] = L'\0';
  }

  if (!SetConsoleTitleW(title_w)) {
    err = uv__new_sys_error(GetLastError());
    goto done;
  }

  EnterCriticalSection(&process_title_lock);
  free(process_title);
  process_title = strdup(title);
  LeaveCriticalSection(&process_title_lock);

  err = uv_ok_;

done:
  free(title_w);
  return err;
}


static int uv__get_process_title() {
  wchar_t title_w[MAX_TITLE_LENGTH];
  int length;

  if (!GetConsoleTitleW(title_w, sizeof(title_w) / sizeof(WCHAR))) {
    return -1;
  }

  /* Find out what the size of the buffer is that we need */
  length = uv_utf16_to_utf8(title_w, -1, NULL, 0);
  if (!length) {
    return -1;
  }

  assert(!process_title);
  process_title = (char*)malloc(length);
  if (!process_title) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  /* Do utf16 -> utf8 conversion here */
  if (!uv_utf16_to_utf8(title_w, -1, process_title, length)) {
    free(process_title);
    return -1;
  }

  return 0;
}


uv_err_t uv_get_process_title(char* buffer, size_t size) {
  uv__once_init();

  EnterCriticalSection(&process_title_lock);
  /*
   * If the process_title was never read before nor explicitly set,
   * we must query it with getConsoleTitleW
   */
  if (!process_title && uv__get_process_title() == -1) {
    return uv__new_sys_error(GetLastError());
  }

  assert(process_title);
  strncpy(buffer, process_title, size);
  LeaveCriticalSection(&process_title_lock);

  return uv_ok_;
}


uint64_t uv_hrtime(void) {
  LARGE_INTEGER counter;

  uv__once_init();

  /* If the performance frequency is zero, there's no support. */
  if (!hrtime_frequency_) {
    /* uv__set_sys_error(loop, ERROR_NOT_SUPPORTED); */
    return 0;
  }

  if (!QueryPerformanceCounter(&counter)) {
    /* uv__set_sys_error(loop, GetLastError()); */
    return 0;
  }

  /* Because we have no guarantee about the order of magnitude of the */
  /* performance counter frequency, and there may not be much headroom to */
  /* multiply by NANOSEC without overflowing, we use 128-bit math instead. */
  return ((uint64_t) counter.LowPart * NANOSEC / hrtime_frequency_) +
         (((uint64_t) counter.HighPart * NANOSEC / hrtime_frequency_)
         << 32);
}


uv_err_t uv_resident_set_memory(size_t* rss) {
  HANDLE current_process;
  PROCESS_MEMORY_COUNTERS pmc;

  current_process = GetCurrentProcess();

  if (!GetProcessMemoryInfo(current_process, &pmc, sizeof(pmc))) {
    return uv__new_sys_error(GetLastError());
  }

  *rss = pmc.WorkingSetSize;

  return uv_ok_;
}


uv_err_t uv_uptime(double* uptime) {
  BYTE stack_buffer[4096];
  BYTE* malloced_buffer = NULL;
  BYTE* buffer = (BYTE*) stack_buffer;
  size_t buffer_size = sizeof(stack_buffer);
  DWORD data_size;

  PERF_DATA_BLOCK* data_block;
  PERF_OBJECT_TYPE* object_type;
  PERF_COUNTER_DEFINITION* counter_definition;

  DWORD i;

  for (;;) {
    LONG result;

    data_size = (DWORD) buffer_size;
    result = RegQueryValueExW(HKEY_PERFORMANCE_DATA,
                              L"2",
                              NULL,
                              NULL,
                              buffer,
                              &data_size);
    if (result == ERROR_SUCCESS) {
      break;
    } else if (result != ERROR_MORE_DATA) {
      *uptime = 0;
      return uv__new_sys_error(result);
    }

    free(malloced_buffer);

    buffer_size *= 2;
    /* Don't let the buffer grow infinitely. */
    if (buffer_size > 1 << 20) {
      goto internalError;
    }

    buffer = malloced_buffer = (BYTE*) malloc(buffer_size);
    if (malloced_buffer == NULL) {
      *uptime = 0;
      return uv__new_artificial_error(UV_ENOMEM);
    }
  }

  if (data_size < sizeof(*data_block))
    goto internalError;

  data_block = (PERF_DATA_BLOCK*) buffer;

  if (wmemcmp(data_block->Signature, L"PERF", 4) != 0)
    goto internalError;

  if (data_size < data_block->HeaderLength + sizeof(*object_type))
    goto internalError;

  object_type = (PERF_OBJECT_TYPE*) (buffer + data_block->HeaderLength);

  if (object_type->NumInstances != PERF_NO_INSTANCES)
    goto internalError;

  counter_definition = (PERF_COUNTER_DEFINITION*) (buffer +
      data_block->HeaderLength + object_type->HeaderLength);
  for (i = 0; i < object_type->NumCounters; i++) {
    if ((BYTE*) counter_definition + sizeof(*counter_definition) >
        buffer + data_size) {
      break;
    }

    if (counter_definition->CounterNameTitleIndex == 674 &&
        counter_definition->CounterSize == sizeof(uint64_t)) {
      if (counter_definition->CounterOffset + sizeof(uint64_t) > data_size ||
          !(counter_definition->CounterType & PERF_OBJECT_TIMER)) {
        goto internalError;
      } else {
        BYTE* address = (BYTE*) object_type + object_type->DefinitionLength +
                        counter_definition->CounterOffset;
        uint64_t value = *((uint64_t*) address);
        *uptime = (double) (object_type->PerfTime.QuadPart - value) /
                  (double) object_type->PerfFreq.QuadPart;
        free(malloced_buffer);
        return uv_ok_;
      }
    }

    counter_definition = (PERF_COUNTER_DEFINITION*)
        ((BYTE*) counter_definition + counter_definition->ByteLength);
  }

  /* If we get here, the uptime value was not found. */
  free(malloced_buffer);
  *uptime = 0;
  return uv__new_artificial_error(UV_ENOSYS);

 internalError:
  free(malloced_buffer);
  *uptime = 0;
  return uv__new_artificial_error(UV_EIO);
}


uv_err_t uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION* sppi;
  DWORD sppi_size;
  SYSTEM_INFO system_info;
  DWORD cpu_count, i, r;
  ULONG result_size;
  size_t size;
  uv_err_t err;
  uv_cpu_info_t* cpu_info;

  *cpu_infos = NULL;
  *count = 0;

  uv__once_init();

  GetSystemInfo(&system_info);
  cpu_count = system_info.dwNumberOfProcessors;

  size = cpu_count * sizeof(uv_cpu_info_t);
  *cpu_infos = (uv_cpu_info_t*) malloc(size);
  if (*cpu_infos == NULL) {
    err = uv__new_artificial_error(UV_ENOMEM);
    goto out;
  }
  memset(*cpu_infos, 0, size);

  sppi_size = sizeof(*sppi) * cpu_count;
  sppi = (SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION*) malloc(sppi_size);
  if (!sppi) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  r = pNtQuerySystemInformation(SystemProcessorPerformanceInformation,
                                sppi,
                                sppi_size,
                                &result_size);
  if (r != ERROR_SUCCESS || result_size != sppi_size) {
    err = uv__new_sys_error(GetLastError());
    goto out;
  }

  for (i = 0; i < cpu_count; i++) {
    WCHAR key_name[128];
    HKEY processor_key;
    DWORD cpu_speed;
    DWORD cpu_speed_size = sizeof(cpu_speed);
    WCHAR cpu_brand[256];
    DWORD cpu_brand_size = sizeof(cpu_brand);

    _snwprintf(key_name,
               ARRAY_SIZE(key_name),
               L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\%d",
               i);

    r = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      key_name,
                      0,
                      KEY_QUERY_VALUE,
                      &processor_key);
    if (r != ERROR_SUCCESS) {
      err = uv__new_sys_error(GetLastError());
      goto out;
    }

    if (RegQueryValueExW(processor_key,
                         L"~MHz",
                         NULL, NULL,
                         (BYTE*) &cpu_speed,
                         &cpu_speed_size) != ERROR_SUCCESS) {
      err = uv__new_sys_error(GetLastError());
      RegCloseKey(processor_key);
      goto out;
    }

    if (RegQueryValueExW(processor_key,
                         L"ProcessorNameString",
                         NULL, NULL,
                         (BYTE*) &cpu_brand,
                         &cpu_brand_size) != ERROR_SUCCESS) {
      err = uv__new_sys_error(GetLastError());
      RegCloseKey(processor_key);
      goto out;
    }

    RegCloseKey(processor_key);

    cpu_info = &(*cpu_infos)[i];
    cpu_info->speed = cpu_speed;
    cpu_info->cpu_times.user = sppi[i].UserTime.QuadPart / 10000;
    cpu_info->cpu_times.sys = (sppi[i].KernelTime.QuadPart -
        sppi[i].IdleTime.QuadPart) / 10000;
    cpu_info->cpu_times.idle = sppi[i].IdleTime.QuadPart / 10000;
    cpu_info->cpu_times.irq = sppi[i].InterruptTime.QuadPart / 10000;
    cpu_info->cpu_times.nice = 0;

    size = uv_utf16_to_utf8(cpu_brand,
                            cpu_brand_size / sizeof(WCHAR),
                            NULL,
                            0);
    if (size == 0) {
      err = uv__new_sys_error(GetLastError());
      goto out;
    }

    /* Allocate 1 extra byte for the null terminator. */
    cpu_info->model = (char*) malloc(size + 1);
    if (cpu_info->model == NULL) {
      err = uv__new_artificial_error(UV_ENOMEM);
      goto out;
    }

    if (uv_utf16_to_utf8(cpu_brand,
                         cpu_brand_size / sizeof(WCHAR),
                         cpu_info->model,
                         size) == 0) {
      err = uv__new_sys_error(GetLastError());
      goto out;
    }

    /* Ensure that cpu_info->model is null terminated. */
    cpu_info->model[size] = '\0';

    (*count)++;
  }

  err = uv_ok_;

 out:
  if (sppi) {
    free(sppi);
  }

  if (err.code != UV_OK &&
      *cpu_infos != NULL) {
    int i;

    for (i = 0; i < *count; i++) {
      /* This is safe because the cpu_infos memory area is zeroed out */
      /* immediately after allocating it. */
      free((*cpu_infos)[i].model);
    }
    free(*cpu_infos);

    *cpu_infos = NULL;
    *count = 0;
  }

  return err;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(cpu_infos[i].model);
  }

  free(cpu_infos);
}


uv_err_t uv_interface_addresses(uv_interface_address_t** addresses,
    int* count) {
  unsigned long size = 0;
  IP_ADAPTER_ADDRESSES* adapter_addresses;
  IP_ADAPTER_ADDRESSES* adapter_address;
  uv_interface_address_t* address;
  struct sockaddr* sock_addr;
  int length;
  char* name;
  /* Use IP_ADAPTER_UNICAST_ADDRESS_XP to retain backwards compatibility */
  /* with Windows XP */
  IP_ADAPTER_UNICAST_ADDRESS_XP* unicast_address;

  if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &size)
      != ERROR_BUFFER_OVERFLOW) {
    return uv__new_sys_error(GetLastError());
  }

  adapter_addresses = (IP_ADAPTER_ADDRESSES*)malloc(size);
  if (!adapter_addresses) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, adapter_addresses, &size)
      != ERROR_SUCCESS) {
    return uv__new_sys_error(GetLastError());
  }

  /* Count the number of interfaces */
  *count = 0;

  for (adapter_address = adapter_addresses;
       adapter_address != NULL;
       adapter_address = adapter_address->Next) {
    unicast_address = (IP_ADAPTER_UNICAST_ADDRESS_XP*)
                      adapter_address->FirstUnicastAddress;
    while (unicast_address) {
      (*count)++;
      unicast_address = unicast_address->Next;
    }
  }

  *addresses = (uv_interface_address_t*)
    malloc(*count * sizeof(uv_interface_address_t));
  if (!(*addresses)) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  address = *addresses;

  for (adapter_address = adapter_addresses;
       adapter_address != NULL;
       adapter_address = adapter_address->Next) {
    name = NULL;
    unicast_address = (IP_ADAPTER_UNICAST_ADDRESS_XP*)
                      adapter_address->FirstUnicastAddress;

    while (unicast_address) {
      sock_addr = unicast_address->Address.lpSockaddr;
      if (sock_addr->sa_family == AF_INET6) {
        address->address.address6 = *((struct sockaddr_in6 *)sock_addr);
      } else {
        address->address.address4 = *((struct sockaddr_in *)sock_addr);
      }

      address->is_internal =
        adapter_address->IfType == IF_TYPE_SOFTWARE_LOOPBACK ? 1 : 0;

      if (!name) {
        /* Convert FriendlyName to utf8 */
        length = uv_utf16_to_utf8(adapter_address->FriendlyName, -1, NULL, 0);
        if (length) {
          name = (char*)malloc(length);
          if (!name) {
            uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
          }

          if (!uv_utf16_to_utf8(adapter_address->FriendlyName, -1, name,
              length)) {
            free(name);
            name = NULL;
          }
        }
      }

      assert(name);
      address->name = name;

      unicast_address = unicast_address->Next;
      address++;
    }
  }

  free(adapter_addresses);

  return uv_ok_;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
    int count) {
  int i;
  char* freed_name = NULL;

  for (i = 0; i < count; i++) {
    if (freed_name != addresses[i].name) {
      freed_name = addresses[i].name;
      free(freed_name);
    }
  }

  free(addresses);
}


void uv_filetime_to_time_t(FILETIME* file_time, time_t* stat_time) {
  FILETIME local_time;
  SYSTEMTIME system_time;
  struct tm time;

  if ((file_time->dwLowDateTime || file_time->dwHighDateTime) &&
      FileTimeToLocalFileTime(file_time, &local_time)         &&
      FileTimeToSystemTime(&local_time, &system_time)) {
    time.tm_year = system_time.wYear - 1900;
    time.tm_mon = system_time.wMonth - 1;
    time.tm_mday = system_time.wDay;
    time.tm_hour = system_time.wHour;
    time.tm_min = system_time.wMinute;
    time.tm_sec = system_time.wSecond;
    time.tm_isdst = -1;

    *stat_time = mktime(&time);
  } else {
    *stat_time = 0;
  }
}
