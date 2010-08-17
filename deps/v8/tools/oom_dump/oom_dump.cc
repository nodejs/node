// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

#include <google_breakpad/processor/minidump.h>
#include <processor/logging.h>

#define ENABLE_DEBUGGER_SUPPORT

#include <v8.h>

namespace {

using google_breakpad::Minidump;
using google_breakpad::MinidumpContext;
using google_breakpad::MinidumpThread;
using google_breakpad::MinidumpThreadList;
using google_breakpad::MinidumpException;
using google_breakpad::MinidumpMemoryRegion;

const char* InstanceTypeToString(int type) {
  static char const* names[v8::internal::LAST_TYPE] = {0};
  if (names[v8::internal::STRING_TYPE] == NULL) {
    using namespace v8::internal;
#define SET(type) names[type] = #type;
    INSTANCE_TYPE_LIST(SET)
#undef SET
  }
  return names[type];
}


u_int32_t ReadPointedValue(MinidumpMemoryRegion* region,
                           u_int64_t base,
                           int offset) {
  u_int32_t ptr = 0;
  CHECK(region->GetMemoryAtAddress(base + 4 * offset, &ptr));
  u_int32_t value = 0;
  CHECK(region->GetMemoryAtAddress(ptr, &value));
  return value;
}


void ReadArray(MinidumpMemoryRegion* region,
               u_int64_t array_ptr,
               int size,
               int* output) {
  for (int i = 0; i < size; i++) {
    u_int32_t value;
    CHECK(region->GetMemoryAtAddress(array_ptr + 4 * i, &value));
    output[i] = value;
  }
}


u_int32_t ReadArrayFrom(MinidumpMemoryRegion* region,
                        u_int64_t base,
                        int offset,
                        int size,
                        int* output) {
  u_int32_t ptr = 0;
  CHECK(region->GetMemoryAtAddress(base + 4 * offset, &ptr));
  ReadArray(region, ptr, size, output);
}


double toM(int size) {
  return size / (1024. * 1024.);
}


class IndirectSorter {
 public:
  explicit IndirectSorter(int* a) : a_(a) { }

  bool operator() (int i0, int i1) {
    return a_[i0] > a_[i1];
  }

 private:
  int* a_;
};

void DumpHeapStats(const char *minidump_file) {
  Minidump minidump(minidump_file);
  CHECK(minidump.Read());

  MinidumpException *exception = minidump.GetException();
  CHECK(exception);

  MinidumpContext* crash_context = exception->GetContext();
  CHECK(crash_context);

  u_int32_t exception_thread_id = 0;
  CHECK(exception->GetThreadID(&exception_thread_id));

  MinidumpThreadList* thread_list = minidump.GetThreadList();
  CHECK(thread_list);

  MinidumpThread* exception_thread =
      thread_list->GetThreadByID(exception_thread_id);
  CHECK(exception_thread);

  const MDRawContextX86* contextX86 = crash_context->GetContextX86();
  CHECK(contextX86);

  const u_int32_t esp = contextX86->esp;

  MinidumpMemoryRegion* memory_region = exception_thread->GetMemory();
  CHECK(memory_region);

  const u_int64_t last = memory_region->GetBase() + memory_region->GetSize();

  u_int64_t heap_stats_addr = 0;
  for (u_int64_t addr = esp; addr < last; addr += 4) {
    u_int32_t value = 0;
    CHECK(memory_region->GetMemoryAtAddress(addr, &value));
    if (value >= esp && value < last) {
      u_int32_t value2 = 0;
      CHECK(memory_region->GetMemoryAtAddress(value, &value2));
      if (value2 == 0xdecade00) {
        heap_stats_addr = addr;
        break;
      }
    }
  }
  CHECK(heap_stats_addr);

  // Read heap stats.

#define READ_FIELD(offset) \
  ReadPointedValue(memory_region, heap_stats_addr, offset)

  CHECK(READ_FIELD(0) == 0xdecade00);
  CHECK(READ_FIELD(23) == 0xdecade01);

  const int new_space_size = READ_FIELD(1);
  const int new_space_capacity = READ_FIELD(2);
  const int old_pointer_space_size = READ_FIELD(3);
  const int old_pointer_space_capacity = READ_FIELD(4);
  const int old_data_space_size = READ_FIELD(5);
  const int old_data_space_capacity = READ_FIELD(6);
  const int code_space_size = READ_FIELD(7);
  const int code_space_capacity = READ_FIELD(8);
  const int map_space_size = READ_FIELD(9);
  const int map_space_capacity = READ_FIELD(10);
  const int cell_space_size = READ_FIELD(11);
  const int cell_space_capacity = READ_FIELD(12);
  const int lo_space_size = READ_FIELD(13);
  const int global_handle_count = READ_FIELD(14);
  const int weak_global_handle_count = READ_FIELD(15);
  const int pending_global_handle_count = READ_FIELD(16);
  const int near_death_global_handle_count = READ_FIELD(17);
  const int destroyed_global_handle_count = READ_FIELD(18);
  const int memory_allocator_size = READ_FIELD(19);
  const int memory_allocator_capacity = READ_FIELD(20);
#undef READ_FIELD

  int objects_per_type[v8::internal::LAST_TYPE + 1] = {0};
  ReadArrayFrom(memory_region, heap_stats_addr, 21,
                v8::internal::LAST_TYPE + 1, objects_per_type);

  int size_per_type[v8::internal::LAST_TYPE + 1] = {0};
  ReadArrayFrom(memory_region, heap_stats_addr, 22, v8::internal::LAST_TYPE + 1,
                size_per_type);

  int js_global_objects =
      objects_per_type[v8::internal::JS_GLOBAL_OBJECT_TYPE];
  int js_builtins_objects =
      objects_per_type[v8::internal::JS_BUILTINS_OBJECT_TYPE];
  int js_global_proxies =
      objects_per_type[v8::internal::JS_GLOBAL_PROXY_TYPE];

  int indices[v8::internal::LAST_TYPE + 1];
  for (int i = 0; i <= v8::internal::LAST_TYPE; i++) {
    indices[i] = i;
  }

  std::stable_sort(indices, indices + sizeof(indices)/sizeof(indices[0]),
                  IndirectSorter(size_per_type));

  int total_size = 0;
  for (int i = 0; i <= v8::internal::LAST_TYPE; i++) {
    total_size += size_per_type[i];
  }

  // Print heap stats.

  printf("exception thread ID: %d (%x)\n",
         exception_thread_id, exception_thread_id);
  printf("heap stats address: %p\n", (void*)heap_stats_addr);
#define PRINT_INT_STAT(stat) \
    printf("\t%-25s\t% 10d\n", #stat ":", stat);
#define PRINT_MB_STAT(stat) \
    printf("\t%-25s\t% 10.3f MB\n", #stat ":", toM(stat));
  PRINT_MB_STAT(new_space_size);
  PRINT_MB_STAT(new_space_capacity);
  PRINT_MB_STAT(old_pointer_space_size);
  PRINT_MB_STAT(old_pointer_space_capacity);
  PRINT_MB_STAT(old_data_space_size);
  PRINT_MB_STAT(old_data_space_capacity);
  PRINT_MB_STAT(code_space_size);
  PRINT_MB_STAT(code_space_capacity);
  PRINT_MB_STAT(map_space_size);
  PRINT_MB_STAT(map_space_capacity);
  PRINT_MB_STAT(cell_space_size);
  PRINT_MB_STAT(cell_space_capacity);
  PRINT_MB_STAT(lo_space_size);
  PRINT_INT_STAT(global_handle_count);
  PRINT_INT_STAT(weak_global_handle_count);
  PRINT_INT_STAT(pending_global_handle_count);
  PRINT_INT_STAT(near_death_global_handle_count);
  PRINT_INT_STAT(destroyed_global_handle_count);
  PRINT_MB_STAT(memory_allocator_size);
  PRINT_MB_STAT(memory_allocator_capacity);
#undef PRINT_STAT

  printf("\n");

  printf(
      "\tJS_GLOBAL_OBJECT_TYPE/JS_BUILTINS_OBJECT_TYPE/JS_GLOBAL_PROXY_TYPE: "
      "%d/%d/%d\n\n",
      js_global_objects, js_builtins_objects, js_global_proxies);

  int running_size = 0;
  for (int i = 0; i <= v8::internal::LAST_TYPE; i++) {
    int type = indices[i];
    const char* name = InstanceTypeToString(type);
    if (name == NULL) {
      // Unknown instance type.  Check that there is no objects of that type.
      CHECK(objects_per_type[type] == 0);
      CHECK(size_per_type[type] == 0);
      continue;
    }
    int size = size_per_type[type];
    running_size += size;
    printf("\t%-37s% 9d% 11.3f MB% 10.3f%%% 10.3f%%\n",
           name, objects_per_type[type], toM(size),
           100.*size/total_size, 100.*running_size/total_size);
  }
  printf("\t%-37s% 9d% 11.3f MB% 10.3f%%% 10.3f%%\n",
         "total", 0, toM(total_size), 100., 100.);
}

}  // namespace

int main(int argc, char **argv) {
  BPLOG_INIT(&argc, &argv);

  if (argc != 2) {
    fprintf(stderr, "usage: %s <minidump>\n", argv[0]);
    return 1;
  }

  DumpHeapStats(argv[1]);

  return 0;
}
