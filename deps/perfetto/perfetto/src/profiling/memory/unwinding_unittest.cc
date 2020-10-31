/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/profiling/memory/unwinding.h"

#include <cxxabi.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unwindstack/RegsGetLocal.h>

#include "perfetto/ext/base/scoped_file.h"
#include "src/profiling/common/unwind_support.h"
#include "src/profiling/memory/client.h"
#include "src/profiling/memory/wire_protocol.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace profiling {
namespace {

TEST(UnwindingTest, StackOverlayMemoryOverlay) {
  base::ScopedFile proc_mem(base::OpenFile("/proc/self/mem", O_RDONLY));
  ASSERT_TRUE(proc_mem);
  uint8_t fake_stack[1] = {120};
  std::shared_ptr<FDMemory> mem(
      std::make_shared<FDMemory>(std::move(proc_mem)));
  StackOverlayMemory memory(mem, 0u, fake_stack, 1);
  uint8_t buf[1] = {};
  ASSERT_EQ(memory.Read(0u, buf, 1), 1u);
  ASSERT_EQ(buf[0], 120);
}

TEST(UnwindingTest, StackOverlayMemoryNonOverlay) {
  uint8_t value = 52;

  base::ScopedFile proc_mem(base::OpenFile("/proc/self/mem", O_RDONLY));
  ASSERT_TRUE(proc_mem);
  uint8_t fake_stack[1] = {120};
  std::shared_ptr<FDMemory> mem(
      std::make_shared<FDMemory>(std::move(proc_mem)));
  StackOverlayMemory memory(mem, 0u, fake_stack, 1);
  uint8_t buf[1] = {1};
  ASSERT_EQ(memory.Read(reinterpret_cast<uint64_t>(&value), buf, 1), 1u);
  ASSERT_EQ(buf[0], value);
}

TEST(UnwindingTest, FDMapsParse) {
  base::ScopedFile proc_maps(base::OpenFile("/proc/self/maps", O_RDONLY));
  ASSERT_TRUE(proc_maps);
  FDMaps maps(std::move(proc_maps));
  ASSERT_TRUE(maps.Parse());
  unwindstack::MapInfo* map_info =
      maps.Find(reinterpret_cast<uint64_t>(&proc_maps));
  ASSERT_NE(map_info, nullptr);
  ASSERT_EQ(map_info->name, "[stack]");
}

void __attribute__((noinline)) AssertFunctionOffset() {
  constexpr auto kMaxFunctionSize = 1000u;
  // Need to zero-initialize to make MSAN happy. MSAN does not see the writes
  // from AsmGetRegs (as it is in assembly) and complains otherwise.
  char reg_data[kMaxRegisterDataSize] = {};
  unwindstack::AsmGetRegs(reg_data);
  auto regs = CreateRegsFromRawData(unwindstack::Regs::CurrentArch(), reg_data);
  ASSERT_GT(regs->pc(), reinterpret_cast<uint64_t>(&AssertFunctionOffset));
  ASSERT_LT(regs->pc() - reinterpret_cast<uint64_t>(&AssertFunctionOffset),
            kMaxFunctionSize);
}

TEST(UnwindingTest, TestFunctionOffset) {
  AssertFunctionOffset();
}

// This is needed because ASAN thinks copying the whole stack is a buffer
// underrun.
void __attribute__((noinline))
UnsafeMemcpy(void* dst, const void* src, size_t n)
    __attribute__((no_sanitize("address", "hwaddress", "memory"))) {
  const uint8_t* from = reinterpret_cast<const uint8_t*>(src);
  uint8_t* to = reinterpret_cast<uint8_t*>(dst);
  for (size_t i = 0; i < n; ++i)
    to[i] = from[i];
}

struct RecordMemory {
  std::unique_ptr<uint8_t[]> payload;
  std::unique_ptr<AllocMetadata> metadata;
};

RecordMemory __attribute__((noinline)) GetRecord(WireMessage* msg) {
  std::unique_ptr<AllocMetadata> metadata(new AllocMetadata);
  *metadata = {};

  const char* stackbase = GetThreadStackBase();
  const char* stacktop = reinterpret_cast<char*>(__builtin_frame_address(0));
  // Need to zero-initialize to make MSAN happy. MSAN does not see the writes
  // from AsmGetRegs (as it is in assembly) and complains otherwise.
  memset(metadata->register_data, 0, sizeof(metadata->register_data));
  unwindstack::AsmGetRegs(metadata->register_data);

  if (stackbase < stacktop) {
    PERFETTO_FATAL("Stacktop >= stackbase.");
    return {nullptr, nullptr};
  }
  size_t stack_size = static_cast<size_t>(stackbase - stacktop);

  metadata->alloc_size = 10;
  metadata->alloc_address = 0x10;
  metadata->stack_pointer = reinterpret_cast<uint64_t>(stacktop);
  metadata->stack_pointer_offset = sizeof(AllocMetadata);
  metadata->arch = unwindstack::Regs::CurrentArch();
  metadata->sequence_number = 1;

  std::unique_ptr<uint8_t[]> payload(new uint8_t[stack_size]);
  UnsafeMemcpy(&payload[0], stacktop, stack_size);

  *msg = {};
  msg->alloc_header = metadata.get();
  msg->payload = reinterpret_cast<char*>(payload.get());
  msg->payload_size = static_cast<size_t>(stack_size);
  return {std::move(payload), std::move(metadata)};
}

TEST(UnwindingTest, DoUnwind) {
  base::ScopedFile proc_maps(base::OpenFile("/proc/self/maps", O_RDONLY));
  base::ScopedFile proc_mem(base::OpenFile("/proc/self/mem", O_RDONLY));
  GlobalCallstackTrie callsites;
  UnwindingMetadata metadata(std::move(proc_maps), std::move(proc_mem));
  WireMessage msg;
  auto record = GetRecord(&msg);
  AllocRecord out;
  ASSERT_TRUE(DoUnwind(&msg, &metadata, &out));
  ASSERT_GT(out.frames.size(), 0u);
  int st;
  std::unique_ptr<char, base::FreeDeleter> demangled(abi::__cxa_demangle(
      out.frames[0].frame.function_name.c_str(), nullptr, nullptr, &st));
  ASSERT_EQ(st, 0) << "mangled: " << demangled.get()
                   << ", frames: " << out.frames.size();
  ASSERT_STREQ(demangled.get(),
               "perfetto::profiling::(anonymous "
               "namespace)::GetRecord(perfetto::profiling::WireMessage*)");
}

TEST(UnwindingTest, DoUnwindReparse) {
  base::ScopedFile proc_maps(base::OpenFile("/proc/self/maps", O_RDONLY));
  base::ScopedFile proc_mem(base::OpenFile("/proc/self/mem", O_RDONLY));
  GlobalCallstackTrie callsites;
  UnwindingMetadata metadata(std::move(proc_maps), std::move(proc_mem));
  // Force reparse in DoUnwind.
  metadata.fd_maps.Reset();
  WireMessage msg;
  auto record = GetRecord(&msg);
  AllocRecord out;
  ASSERT_TRUE(DoUnwind(&msg, &metadata, &out));
  ASSERT_GT(out.frames.size(), 0u);
  int st;
  std::unique_ptr<char, base::FreeDeleter> demangled(abi::__cxa_demangle(
      out.frames[0].frame.function_name.c_str(), nullptr, nullptr, &st));
  ASSERT_EQ(st, 0) << "mangled: " << demangled.get()
                   << ", frames: " << out.frames.size();
  ASSERT_STREQ(demangled.get(),
               "perfetto::profiling::(anonymous "
               "namespace)::GetRecord(perfetto::profiling::WireMessage*)");
}

}  // namespace
}  // namespace profiling
}  // namespace perfetto
