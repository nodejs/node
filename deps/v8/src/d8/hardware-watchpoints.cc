// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8/hardware-watchpoints.h"

#ifdef V8_ENABLE_HARDWARE_WATCHPOINT_SUPPORT

#include <sys/mman.h>
#undef MAP_TYPE  // Conflicts with MAP_TYPE in Torque-generated instance-types.h
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <atomic>

#include "src/api/api-inl.h"
#include "src/base/platform/yield-processor.h"
#include "src/diagnostics/disasm.h"
#include "src/execution/isolate.h"
#include "src/objects/js-objects-inl.h"
#include "src/sandbox/testing.h"
#include "src/utils/utils.h"

namespace v8 {

namespace {

// The hardware provides four watchpoints, set via the DR0 to DR3 registers.
static constexpr int kNumWatchpoints = 4;

struct WatchpointInfo {
  uintptr_t address = 0;
  // TODO(clemensb): Add length of the field (and pass that to DR7).
  // TODO(clemensb): Add information about how to manipulate read values.
};

struct HardwareWatchpointSupport {
  // Controls whether tracing output should be printed.
  bool tracing_enabled = false;
  // The PID of the d8 process; only used in the debugger process.
  int d8_pid = -1;

  // Initialized based on `v8_flags.random_seed`, used for random manipulation
  // of read values.
  base::RandomNumberGenerator rng{0};

  // This part is allocated in shared space and used to communicate between the
  // two processes ("d8" and "debugger").
  struct SharedPart {
    // Written by the "d8" process to update watchpoints; read by the "debugger"
    // process which also resets `has_changed_watchpoints`.
    std::atomic<bool> has_changed_watchpoints{false};
    WatchpointInfo watchpoints[kNumWatchpoints];

    // For coordinating initial setup between the two processes.
    std::atomic<int> initialization_step{0};
  }* shared = nullptr;
} g_support;

// Print trace output if `--trace-in-sandbox-manipulation-via-watchpoints` was
// specified.
// Note that the processes cannot really be debugged via gdb because they use
// the same kernel API that gdb would use (ptrace). Hence we need a lot of
// tracing to debug non-working watchpoints.
#define TRACE(...) \
  if (g_support.tracing_enabled) i::PrintF(__VA_ARGS__);

void SetNewWatchpoints() {
  WatchpointInfo* watchpoints = g_support.shared->watchpoints;
  static_assert(kNumWatchpoints == 4);
  TRACE("[debugger] Updating watchpoints to 0x%lx / 0x%lx / 0x%lx / 0x%lx\n",
        watchpoints[0].address, watchpoints[1].address, watchpoints[2].address,
        watchpoints[3].address);

  for (int i = 0; i < kNumWatchpoints; ++i) {
    // Write DR0-DR3 (watchpoint registers).
    CHECK_EQ(0, ptrace(PTRACE_POKEUSER, g_support.d8_pid,
                       offsetof(struct user, u_debugreg[i]),
                       watchpoints[i].address));
  }

  // Write DR7 (control register).
  uint64_t dr7 = 0;
  for (int i = 0; i < kNumWatchpoints; ++i) {
    if (!watchpoints[i].address) continue;  // watchpoint unused / disabled.
    dr7 |= 0x1 << (2 * i) |                 // Li=1 (activate watchpoint in DRi)
           (0b11 << (16 + 4 * i)) |         // RWi=r/w (0x11)
           (0b11 << (18 + 4 * i));          // LENi=4 (0x11) (cover 4 bytes)
  }
  CHECK_EQ(0, ptrace(PTRACE_POKEUSER, g_support.d8_pid,
                     offsetof(struct user, u_debugreg[7]), dr7));
}

// Returns a pointer to the field in `regs` which was read before `regs.rip`.
// This is used when a watchpoint is hit to figure out if it was a read or
// write, and where the result of the read was stored.
// On a write, this returns `nullptr`.
unsigned long long*  // NOLINT(runtime/int)
DisassembleReadResultRegister(struct user_regs_struct& regs) {
  // Set up the disassembler, ignore unknown / illegal bytes (we might start
  // decoding in the middle of an instruction).
  disasm::NameConverter disasm_name_converter;
  disasm::Disassembler disasm{
      disasm_name_converter,
      disasm::Disassembler::kContinueOnUnimplementedOpcode};

  // Read bytes around `rip`. Include the range [-16, 15], but align to whole
  // words.
  uint8_t bytes_around_rip[40];             // Five whole words.
  size_t rip_offset = 16 + (regs.rip & 7);  // Somewhere in [16, 23].
  for (size_t offset = 0; offset < arraysize(bytes_around_rip);
       offset += i::kSystemPointerSize) {
    errno = 0;
    uint64_t data = ptrace(PTRACE_PEEKTEXT, g_support.d8_pid,
                           regs.rip - rip_offset + offset, 0);
    CHECK_EQ(0, errno);

    static_assert(sizeof(data) == i::kSystemPointerSize);
    memcpy(bytes_around_rip + offset, &data, i::kSystemPointerSize);
  }

  // Buffer for disassembled instruction.
  char buffer[64];
  // Try disassembling at increasing offsets. Eventually this should synchronize
  // with the instruction stream and find the instruction ending at `rip`.
  for (size_t offset = 0; offset < rip_offset; ++offset) {
    while (offset < rip_offset) {
      offset += disasm.InstructionDecode(base::VectorOf(buffer),
                                         bytes_around_rip + offset);
    }
    if (offset > rip_offset) continue;  // Overshot `rip`.
    // Found the instruction!
    // The format is of the form: "4d637607     REX.W movsxlq r14,[r14+0x7]"
    TRACE("[debugger] Executed instruction was: %s\n", buffer);
    // Figure out if this was a read or a write.
    char* mov_pos = buffer;
    // Skip over the hex representation.
    while (isxdigit(*mov_pos)) ++mov_pos;
    // Skip over spaces.
    while (*mov_pos == ' ') ++mov_pos;
    // Skip over REX prefix.
    if (memcmp(mov_pos, "REX.W ", 6) == 0) mov_pos += 6;
    // TODO(clemensb): Implement more instructions if necessary.
    if (memcmp(mov_pos, "mov", 3) != 0) FATAL("Not a mov: %s\n", buffer);
    // Find the position of the comma to figure out if the memory operand is
    // on the LHS (read) or RHS (write).
    char* space_pos = strchr(mov_pos + 3, ' ');
    CHECK_NOT_NULL(space_pos);
    char* comma_pos = strchr(space_pos + 1, ',');
    CHECK_NOT_NULL(comma_pos);
    bool mem_op_on_lhs = space_pos[1] == '[';
    bool mem_op_on_rhs = comma_pos[1] == '[';
    // Be extra careful to interpret the disassembly correctly.
    CHECK_EQ(mem_op_on_lhs, comma_pos[-1] == ']');
    CHECK_EQ(mem_op_on_rhs, space_pos[strlen(space_pos) - 1] == ']');
    CHECK_EQ(mem_op_on_lhs, !mem_op_on_rhs);
    if (mem_op_on_lhs) return nullptr;  // This is a write.
#define FIND_REG_NAME(name) \
  if (memcmp(space_pos + 1, #name, strlen(#name)) == 0) return &regs.name;
    // TODO(clemensb): Also handle double registers.
    GENERAL_REGISTERS(FIND_REG_NAME)
#undef FIND_REG_NAME

    FATAL("Could not read register name: %s", buffer);
  }

  FATAL("Could not disassemble the instruction preceeding RIP");
}

// Executed in the "debugger" process: Check if a watchpoint was hit and handle
// it.
// Return true if a watchpoint was hit, false otherwise.
bool HandleWatchpoint(struct user_regs_struct& regs) {
  errno = 0;
  uint64_t dr6 = ptrace(PTRACE_PEEKUSER, g_support.d8_pid,
                        offsetof(struct user, u_debugreg[6]), 0);
  CHECK_EQ(0, errno);
  // Check if a watchpoint was hit. Return false if not.
  if ((dr6 & 0xf) == 0) return false;

  int hit_watchpoint = base::bits::CountTrailingZeros(dr6 & 0xf);
  // Exactly one watchpoint (DR0-DR4) must have been hit.
  CHECK_EQ(0x1 << hit_watchpoint, dr6 & 0xf);

  // Reset first four bits (B0-B4) in DR6.
  CHECK_EQ(0, ptrace(PTRACE_POKEUSER, g_support.d8_pid,
                     offsetof(struct user, u_debugreg[6]), dr6 & ~0xf));

  // Disassemble the instruction that read the value (the instruction which ends
  // at the current RIP).
  unsigned long long*  // NOLINT(runtime/int)
      result_reg = DisassembleReadResultRegister(regs);
  // It it was a write instruction, do nothing (return).
  if (!result_reg) {
    TRACE("[debugger] Ignoring write.\n");
    return true;
  }

  uint64_t read_value = *result_reg;

  // TODO(clemensb): Add API for specifying how to manipulate the value.
  // TODO(clemensb): Make sure to only produce values that the specific
  // instruction would have produced.
  // TODO(clemensb): Look into strategies used by AFL:
  // https://lcamtuf.blogspot.com/2014/08/binary-fuzzing-strategies-what-works.html.
  // TODO(clemensb): Unify with strategies used by Samuel's trap-based fuzzer
  // (https://crbug.com/361277236).
  uint64_t new_value = read_value;
  switch (g_support.rng.NextInt(4)) {
    case 0:
      new_value += 10;
      break;
    case 1:
      new_value -= 10;
      break;
    case 2:
      // Random bit flip.
      new_value ^= uint64_t{1} << g_support.rng.NextInt(64);
      break;
    case 3:
      new_value = g_support.rng.NextInt64();
      break;
  }
  TRACE("[debugger] Changing value in result register from %" PRIu64 "/%" PRIx64
        " to %" PRIu64 "/%" PRIx64 ".\n",
        read_value, read_value, new_value, new_value);
  *result_reg = new_value;

  CHECK_EQ(0, ptrace(PTRACE_SETREGS, g_support.d8_pid, 0, &regs));
  return true;
}

// This implements the main logic of the debugger; it waits until the `d8`
// process stops, then reacts by
// - stopping itself in case `d8` exited regularly or via a crash,
// - setting a new hardware watchpoint if requested, or
// - reacting to a hit watchpoint.
void WaitForD8ToStopThenReact() {
  TRACE("[debugger] waiting for d8 to stop.\n");
  int status;
  waitpid(g_support.d8_pid, &status, 0);

  if (WIFEXITED(status)) {
    TRACE("[debugger] d8 exited with status %d.\n", WEXITSTATUS(status));
    base::OS::ExitProcess(0);
  }

  if (WIFSIGNALED(status)) {
    TRACE("[debugger] d8 crashed/killed by signal %d (%s).\n", WTERMSIG(status),
          strsignal(WTERMSIG(status)));
    base::OS::ExitProcess(0);
  }

  CHECK(WIFSTOPPED(status));

  struct user_regs_struct regs;
  CHECK_EQ(0, ptrace(PTRACE_GETREGS, g_support.d8_pid, 0, &regs));
  int signal = WSTOPSIG(status);
  TRACE("[debugger] d8 stopped at 0x%llx (sig %d, %s).\n", regs.rip, signal,
        strsignal(signal));

  // Check if a watchpoint has been hit; if so, handle it and return.
  if (signal == SIGTRAP && HandleWatchpoint(regs)) return;

  if (WSTOPSIG(status) == SIGSTOP) {
    // D8 requested an update on watchpoints -> execute them.
    CHECK(g_support.shared->has_changed_watchpoints.exchange(false));
    SetNewWatchpoints();
    return;
  }

  if (signal == SIGSEGV || signal == SIGABRT || signal == SIGILL ||
      signal == SIGTRAP) {
    TRACE("[debugger] d8 crashed (%s)\n", strsignal(signal));
    base::OS::ExitProcess(0);
  }

  // If there are other reasons for the d8 process to stop we should handle
  // them.
  UNREACHABLE();
}

}  // namespace

void SetupForHardwareWatchpoints(bool enable_tracing) {
  static std::atomic<bool> already_setup = false;
  CHECK(already_setup.exchange(true, std::memory_order_relaxed) == false);

  g_support.tracing_enabled = enable_tracing;

  // Set up shared memory for communication between the two processes.
  void* shared_mem =
      mmap(NULL, sizeof(HardwareWatchpointSupport), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (shared_mem == MAP_FAILED) {
    FATAL("mmap failed: %s", strerror(errno));
  }
  g_support.shared = new (shared_mem) HardwareWatchpointSupport::SharedPart();

  // Initialize the global RNG based on `--random-seed` or a randomly chosen
  // seed.
  g_support.rng.SetSeed(i::v8_flags.random_seed
                            ? i::v8_flags.random_seed
                            : base::RandomNumberGenerator{}.initial_seed());

  // Now fork the process; the parent will be the regular d8 process, the child
  // is the "debugger" which attaches to the parent via ptrace.
  pid_t d8_pid = getpid();
  pid_t debugger_pid = fork();
  if (debugger_pid == -1) {
    FATAL("fork failed: %s", strerror(errno));
  }

  if (debugger_pid != 0) {
    TRACE("[d8] I am the parent (d8) process!\n");

    // Allow the child (debugger) to ptrace me.
    if (prctl(PR_SET_PTRACER, debugger_pid, 0, 0, 0) == -1) {
      FATAL("PR_SET_PTRACER failed: %s", strerror(errno));
    }

    TRACE("[d8] signalling to debugger that they have permission to ptrace.\n");
    g_support.shared->initialization_step.store(1);

    TRACE("[d8] waiting for debugger to attach.\n");
    while (g_support.shared->initialization_step.load() != 2) {
      YIELD_PROCESSOR;
    }
    TRACE("[d8] debugger is attached; continuing d8 execution.\n");
    // Continue standard d8 execution.
    return;
  }

  TRACE("[debugger] I am the child (debugger) process!\n");
  g_support.d8_pid = d8_pid;

  // Dump the RNG seed for reproduction.
  TRACE("[debugger] RNG seed: %" PRIi64 "\n", g_support.rng.initial_seed());

  TRACE("[debugger] waiting for d8 process to signal readyness for attach.\n");
  while (g_support.shared->initialization_step.load() != 1) {
    YIELD_PROCESSOR;
  }

  TRACE("[debugger] attaching to d8 process.\n");
  if (ptrace(PTRACE_ATTACH, d8_pid, 0, 0) == -1) {
    FATAL("PTRACE_ATTACH failed: %s", strerror(errno));
  }

  TRACE("[debugger] waiting for d8 process to stop.\n");

  int status;
  waitpid(d8_pid, &status, 0);

  TRACE("[debugger] d8 stopped with status %d.\n", status);
  CHECK(WIFSTOPPED(status));

  // Kill the tracee (d8) if the tracer (debugger) is killed.
  TRACE("[debugger] setting ptrace options.\n");
  if (ptrace(PTRACE_SETOPTIONS, d8_pid, 0, PTRACE_O_EXITKILL) == -1) {
    FATAL("PTRACE_SETOPTIONS failed: %s", strerror(errno));
  }
  // Signal to d8 that it can start executing the actual JS code.
  g_support.shared->initialization_step.store(2);

  while (true) {
    TRACE("[debugger] continuing d8\n");

    if (ptrace(PTRACE_CONT, d8_pid, NULL, NULL) == -1) {
      FATAL("PTRACE_CONT failed: %s", strerror(errno));
    }

    // This will eventually end the process, once the "d8" process has exited.
    WaitForD8ToStopThenReact();
  }
}

// Called from d8 on exit to reset all watchpoints.
void ResetAllHardwareWatchpoints() {
  WatchpointInfo* watchpoints = g_support.shared->watchpoints;
  bool has_hardware_watchpoint = false;
  for (int i = 0; i < kNumWatchpoints; ++i) {
    has_hardware_watchpoint |= watchpoints[i].address != 0;
  }
  if (!has_hardware_watchpoint) {
    TRACE("[d8] no hardware watchpoints to reset.\n");
    return;
  }

  TRACE("[d8] resetting all hardware watchpoints.\n");
  for (int i = 0; i < kNumWatchpoints; ++i) watchpoints[i].address = 0;
  CHECK(!g_support.shared->has_changed_watchpoints.exchange(true));
  // Signal the debugger process; this pauses execution until that process
  // continues us.
  raise(SIGSTOP);
  TRACE("[d8] continuing execution.\n");
}

namespace {
void ThrowTypeError(v8::Isolate* isolate, std::string_view message) {
  isolate->ThrowException(v8::Exception::TypeError(
      v8::String::NewFromUtf8(isolate, message.data(), NewStringType::kNormal,
                              static_cast<int>(message.size()))
          .ToLocalChecked()));
}
}  // namespace

void SetHardwareWatchpointCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  Isolate* isolate = info.GetIsolate();
  Local<v8::Context> context = isolate->GetCurrentContext();

  if (info.Length() < 2) {
    ThrowTypeError(isolate, "Pass two arguments: object and offset");
    return;
  }

  i::DirectHandle<i::HeapObject> object;
  if (!i::TryCast(Utils::OpenDirectHandle(*info[0]), &object)) {
    ThrowTypeError(isolate, "First argument must be an object");
    return;
  }

  int32_t offset;
  if (!info[1]->IsInt32() || !info[1]->Int32Value(context).To(&offset)) {
    v8::String::Utf8Value field_name(isolate, info[1]);
    if (!*field_name) {
      ThrowTypeError(isolate, "Second argument must be an integer or a string");
      return;
    }

    i::InstanceType instance_type = object->map()->instance_type();
    if (std::optional<int> offset_from_name = i::SandboxTesting::GetFieldOffset(
            isolate, instance_type, *field_name)) {
      offset = offset_from_name.value();
    } else {
      DCHECK(isolate->HasPendingException());
      return;
    }
  }

  int object_size = object->Size();
  DCHECK_EQ(0, object_size % i::kTaggedSize);
  if (offset < 0 || offset >= object_size) {
    std::ostringstream error;
    error << "Second argument (offset=" << offset << ") is "
          << "out of bounds of the given object of size " << object_size;
    ThrowTypeError(isolate, error.view());
    return;
  }
  if ((offset % i::kTaggedSize) != 0) {
    std::ostringstream error;
    error << "Second argument (offset=" << offset << ") is "
          << "not tagged-size-aligned";
    ThrowTypeError(isolate, error.view());
    return;
  }

  // TODO(clemensb): Make sure that the object stays where it is in memory,
  // potentially by moving it to old space and keeping a global reference to it.

  i::Address watch_addr = object->address() + offset;
  uint32_t current_content = *reinterpret_cast<uint32_t*>(watch_addr);

  int watchpoint_idx = 0;
  while (g_support.shared->watchpoints[watchpoint_idx].address) {
    if (++watchpoint_idx == kNumWatchpoints) {
      FATAL("Maximum number of watchpoints (%d) exceeded", kNumWatchpoints);
    }
  }
  g_support.shared->watchpoints[watchpoint_idx].address = watch_addr;

  TRACE(
      "[d8] sending watch request (#%d/%d) for addr 0x%lx (current content: "
      "%u/0x%x)\n",
      watchpoint_idx, kNumWatchpoints, watch_addr, current_content,
      current_content);
  CHECK(!g_support.shared->has_changed_watchpoints.exchange(true));
  // Signal the debugger process; this pauses execution until that process
  // continues us.
  raise(SIGSTOP);
  TRACE("[d8] continuing execution.\n");
}

}  // namespace v8

#endif  // V8_ENABLE_HARDWARE_WATCHPOINT_SUPPORT
