// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/target.h"

#include <inttypes.h>
#include "src/base/platform/time.h"
#include "src/debug/wasm/gdb-server/gdb-remote-util.h"
#include "src/debug/wasm/gdb-server/gdb-server.h"
#include "src/debug/wasm/gdb-server/packet.h"
#include "src/debug/wasm/gdb-server/session.h"
#include "src/debug/wasm/gdb-server/transport.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

static const int kThreadId = 1;

Target::Target(GdbServer* gdb_server)
    : gdb_server_(gdb_server),
      status_(Status::Running),
      cur_signal_(0),
      session_(nullptr) {}

void Target::InitQueryPropertyMap() {
  // Request LLDB to send packets up to 4000 bytes for bulk transfers.
  query_properties_["Supported"] =
      "PacketSize=4000;vContSupported-;qXfer:libraries:read+;";

  query_properties_["Attached"] = "1";

  // There is only one register, named 'pc', in this architecture
  query_properties_["RegisterInfo0"] =
      "name:pc;alt-name:pc;bitsize:64;offset:0;encoding:uint;format:hex;set:"
      "General Purpose Registers;gcc:16;dwarf:16;generic:pc;";
  query_properties_["RegisterInfo1"] = "E45";

  // ProcessInfo for wasm32
  query_properties_["ProcessInfo"] =
      "pid:1;ppid:1;uid:1;gid:1;euid:1;egid:1;name:6c6c6462;triple:" +
      Mem2Hex("wasm32-unknown-unknown-wasm") + ";ptrsize:4;";
  query_properties_["Symbol"] = "OK";

  // Current thread info
  char buff[16];
  snprintf(buff, sizeof(buff), "QC%x", kThreadId);
  query_properties_["C"] = buff;
}

void Target::Terminate() {
  // Executed in the Isolate thread.
  status_ = Status::Terminated;
}

void Target::Run(Session* session) {
  // Executed in the GdbServer thread.
  session_ = session;
  do {
    WaitForDebugEvent();
    ProcessCommands();
  } while (!IsTerminated() && session_->IsConnected());
  session_ = nullptr;
}

void Target::WaitForDebugEvent() {
  // Executed in the GdbServer thread.

  if (status_ != Status::Terminated) {
    // Wait for either:
    //   * the thread to fault (or single-step)
    //   * an interrupt from LLDB
    session_->WaitForDebugStubEvent();
  }
}

void Target::ProcessCommands() {
  // GDB-remote messages are processed in the GDBServer thread.

  if (IsTerminated()) {
    return;
  }

  // Now we are ready to process commands.
  // Loop through packets until we process a continue packet or a detach.
  Packet recv, reply;
  while (session_->IsConnected()) {
    if (!session_->GetPacket(&recv)) {
      continue;
    }

    reply.Clear();
    ProcessPacketResult result = ProcessPacket(&recv, &reply);
    switch (result) {
      case ProcessPacketResult::Paused:
        session_->SendPacket(&reply);
        break;

      case ProcessPacketResult::Continue:
        // If this is a continue type command, break out of this loop.
        return;

      case ProcessPacketResult::Detach:
        session_->SendPacket(&reply);
        session_->Disconnect();
        cur_signal_ = 0;  // // Reset the signal value
        return;

      case ProcessPacketResult::Kill:
        session_->SendPacket(&reply);
        exit(-9);

      default:
        UNREACHABLE();
    }
  }
}

Target::ProcessPacketResult Target::ProcessPacket(Packet* pkt_in,
                                                  Packet* pkt_out) {
  ErrorCode err = ErrorCode::None;

  // Clear the outbound message.
  pkt_out->Clear();

  // Set the sequence number, if present.
  int32_t seq = -1;
  if (pkt_in->GetSequence(&seq)) {
    pkt_out->SetSequence(seq);
  }

  // A GDB-remote packet begins with an upper- or lower-case letter, which
  // generally represents a single command.
  // The letters 'q' and 'Q' introduce a "General query packets" and are used
  // to extend the protocol with custom commands.
  // The format of GDB-remote commands is documented here:
  // https://sourceware.org/gdb/onlinedocs/gdb/Overview.html#Overview.
  char cmd;
  pkt_in->GetRawChar(&cmd);

  switch (cmd) {
    // Queries the reason the target halted.
    // IN : $?
    // OUT: A Stop-reply packet
    case '?':
      SetStopReply(pkt_out);
      break;

    // Resumes execution
    // IN : $c
    // OUT: A Stop-reply packet is sent later, when the execution halts.
    case 'c':
      // TODO(paolosev) - Not implemented yet
      err = ErrorCode::Failed;
      break;

    // Detaches the debugger from this target
    // IN : $D
    // OUT: $OK
    case 'D':
      TRACE_GDB_REMOTE("Requested Detach.\n");
      pkt_out->AddString("OK");
      return ProcessPacketResult::Detach;

    // Read general registers (We only support register 'pc' that contains
    // the current instruction pointer).
    // IN : $g
    // OUT: $xx...xx
    case 'g': {
      uint64_t pc = GetCurrentPc();
      pkt_out->AddBlock(&pc, sizeof(pc));
      break;
    }

    // Write general registers - NOT SUPPORTED
    // IN : $Gxx..xx
    // OUT: $ (empty string)
    case 'G': {
      break;
    }

    // Set thread for subsequent operations. For Wasm targets, we currently
    // assume that there is only one thread with id = kThreadId (= 1).
    // IN : $H(c/g)(-1,0,xxxx)
    // OUT: $OK
    case 'H': {
      // Type of the operation (‘m’, ‘M’, ‘g’, ‘G’, ...)
      char operation;
      if (!pkt_in->GetRawChar(&operation)) {
        err = ErrorCode::BadFormat;
        break;
      }

      uint64_t thread_id;
      if (!pkt_in->GetNumberSep(&thread_id, 0)) {
        err = ErrorCode::BadFormat;
        break;
      }

      // Ignore, only one thread supported for now.
      pkt_out->AddString("OK");
      break;
    }

    // Kills the debuggee.
    // IN : $k
    // OUT: $OK
    case 'k':
      TRACE_GDB_REMOTE("Requested Kill.\n");
      pkt_out->AddString("OK");
      return ProcessPacketResult::Kill;

    // Reads {llll} addressable memory units starting at address {aaaa}.
    // IN : $maaaa,llll
    // OUT: $xx..xx
    case 'm': {
      uint64_t address;
      if (!pkt_in->GetNumberSep(&address, 0)) {
        err = ErrorCode::BadFormat;
        break;
      }
      wasm_addr_t wasm_addr(address);

      uint64_t len;
      if (!pkt_in->GetNumberSep(&len, 0)) {
        err = ErrorCode::BadFormat;
        break;
      }

      if (len > Transport::kBufSize / 2) {
        err = ErrorCode::BadArgs;
        break;
      }

      uint32_t length = static_cast<uint32_t>(len);
      uint8_t buff[Transport::kBufSize];
      if (wasm_addr.ModuleId() > 0) {
        uint32_t read =
            gdb_server_->GetWasmModuleBytes(wasm_addr, buff, length);
        if (read > 0) {
          pkt_out->AddBlock(buff, read);
        } else {
          err = ErrorCode::Failed;
        }
      } else {
        err = ErrorCode::BadArgs;
      }
      break;
    }

    // Writes {llll} addressable memory units starting at address {aaaa}.
    // IN : $Maaaa,llll:xx..xx
    // OUT: $OK
    case 'M': {
      // Writing to memory not supported for Wasm.
      err = ErrorCode::Failed;
      break;
    }

    // pN: Reads the value of register N.
    // IN : $pxx
    // OUT: $xx..xx
    case 'p': {
      uint64_t pc = GetCurrentPc();
      pkt_out->AddBlock(&pc, sizeof(pc));
    } break;

    case 'q': {
      err = ProcessQueryPacket(pkt_in, pkt_out);
      break;
    }

    // Single step
    // IN : $s
    // OUT: A Stop-reply packet is sent later, when the execution halts.
    case 's': {
      // TODO(paolosev) - Not implemented yet
      err = ErrorCode::Failed;
      break;
    }

    // Find out if the thread 'id' is alive.
    // IN : $T
    // OUT: $OK if alive, $Enn if thread is dead.
    case 'T': {
      uint64_t id;
      if (!pkt_in->GetNumberSep(&id, 0)) {
        err = ErrorCode::BadFormat;
        break;
      }
      if (id != kThreadId) {
        err = ErrorCode::BadArgs;
        break;
      }
      pkt_out->AddString("OK");
      break;
    }

    // Z: Adds a breakpoint
    // IN : $Ztype,addr,kind
    // OUT: $OK (success) or $Enn (error)
    case 'Z': {
      // Only software breakpoints are supported.
      uint64_t breakpoint_type;  // 0: sw breakpoint,
                                 // 1: hw breakpoint,
                                 // 2: watchpoint
      uint64_t breakpoint_address;
      uint64_t breakpoint_kind;  // Ignored for Wasm.
      if (!pkt_in->GetNumberSep(&breakpoint_type, 0) || breakpoint_type != 0 ||
          !pkt_in->GetNumberSep(&breakpoint_address, 0) ||
          !pkt_in->GetNumberSep(&breakpoint_kind, 0)) {
        err = ErrorCode::BadFormat;
        break;
      }

      wasm_addr_t wasm_breakpoint_addr(breakpoint_address);
      if (!gdb_server_->AddBreakpoint(wasm_breakpoint_addr.ModuleId(),
                                      wasm_breakpoint_addr.Offset())) {
        err = ErrorCode::Failed;
        break;
      }

      pkt_out->AddString("OK");
      break;
    }

    // z: Removes a breakpoint
    // IN : $ztype,addr,kind
    // OUT: $OK (success) or $Enn (error)
    case 'z': {
      uint64_t breakpoint_type;
      uint64_t breakpoint_address;
      uint64_t breakpoint_kind;
      if (!pkt_in->GetNumberSep(&breakpoint_type, 0) || breakpoint_type != 0 ||
          !pkt_in->GetNumberSep(&breakpoint_address, 0) ||
          !pkt_in->GetNumberSep(&breakpoint_kind, 0)) {
        err = ErrorCode::BadFormat;
        break;
      }

      wasm_addr_t wasm_breakpoint_addr(breakpoint_address);
      if (!gdb_server_->RemoveBreakpoint(wasm_breakpoint_addr.ModuleId(),
                                         wasm_breakpoint_addr.Offset())) {
        err = ErrorCode::Failed;
        break;
      }

      pkt_out->AddString("OK");
      break;
    }

    // If the command is not recognized, ignore it by sending an empty reply.
    default: {
      std::string str;
      pkt_in->GetString(&str);
      TRACE_GDB_REMOTE("Unknown command: %s\n", pkt_in->GetPayload());
    }
  }

  // If there is an error, return the error code instead of a payload
  if (err != ErrorCode::None) {
    pkt_out->Clear();
    pkt_out->AddRawChar('E');
    pkt_out->AddWord8(static_cast<uint8_t>(err));
  }
  return ProcessPacketResult::Paused;
}

Target::ErrorCode Target::ProcessQueryPacket(const Packet* pkt_in,
                                             Packet* pkt_out) {
  const char* str = &pkt_in->GetPayload()[1];

  // Get first thread query
  // IN : $qfThreadInfo
  // OUT: $m<tid>
  //
  // Get next thread query
  // IN : $qsThreadInfo
  // OUT: $m<tid> or l to denote end of list.
  if (!strcmp(str, "fThreadInfo") || !strcmp(str, "sThreadInfo")) {
    if (str[0] == 'f') {
      pkt_out->AddString("m");
      pkt_out->AddNumberSep(kThreadId, 0);
    } else {
      pkt_out->AddString("l");
    }
    return ErrorCode::None;
  }

  // Get a list of loaded libraries
  // IN : $qXfer:libraries:read
  // OUT: an XML document which lists loaded libraries, with this format:
  // <library-list>
  //   <library name="foo.wasm">
  //     <section address="0x100000000"/>
  //   </library>
  //   <library name="bar.wasm">
  //     <section address="0x200000000"/>
  //   </library>
  // </library-list>
  // Note that LLDB must be compiled with libxml2 support to handle this packet.
  std::string tmp = "Xfer:libraries:read";
  if (!strncmp(str, tmp.data(), tmp.length())) {
    std::vector<GdbServer::WasmModuleInfo> modules =
        gdb_server_->GetLoadedModules();
    std::string result("l<library-list>");
    for (const auto& module : modules) {
      wasm_addr_t address(module.module_id, 0);
      char address_string[32];
      snprintf(address_string, sizeof(address_string), "%" PRIu64,
               static_cast<uint64_t>(address));
      result += "<library name=\"";
      result += module.module_name;
      result += "\"><section address=\"";
      result += address_string;
      result += "\"/></library>";
    }
    result += "</library-list>";
    pkt_out->AddString(result.c_str());
    return ErrorCode::None;
  }

  // Get the current call stack.
  // IN : $qWasmCallStack
  // OUT: $xx..xxyy..yyzz..zz (A sequence of uint64_t values represented as
  //                           consecutive 8-bytes blocks).
  std::vector<std::string> toks = StringSplit(str, ":;");
  if (toks[0] == "WasmCallStack") {
    std::vector<wasm_addr_t> callStackPCs = gdb_server_->GetWasmCallStack();
    pkt_out->AddBlock(
        callStackPCs.data(),
        static_cast<uint32_t>(sizeof(wasm_addr_t) * callStackPCs.size()));
    return ErrorCode::None;
  }

  // Get a Wasm global value in the Wasm module specified.
  // IN : $qWasmGlobal:moduleId;index
  // OUT: $xx..xx
  if (toks[0] == "WasmGlobal") {
    if (toks.size() == 3) {
      uint32_t module_id =
          static_cast<uint32_t>(strtol(toks[1].data(), nullptr, 10));
      uint32_t index =
          static_cast<uint32_t>(strtol(toks[2].data(), nullptr, 10));
      uint64_t value = 0;
      if (gdb_server_->GetWasmGlobal(module_id, index, &value)) {
        pkt_out->AddBlock(&value, sizeof(value));
        return ErrorCode::None;
      } else {
        return ErrorCode::Failed;
      }
    }
    return ErrorCode::BadFormat;
  }

  // Get a Wasm local value in the stack frame specified.
  // IN : $qWasmLocal:moduleId;frameIndex;index
  // OUT: $xx..xx
  if (toks[0] == "WasmLocal") {
    if (toks.size() == 4) {
      uint32_t module_id =
          static_cast<uint32_t>(strtol(toks[1].data(), nullptr, 10));
      uint32_t frame_index =
          static_cast<uint32_t>(strtol(toks[2].data(), nullptr, 10));
      uint32_t index =
          static_cast<uint32_t>(strtol(toks[3].data(), nullptr, 10));
      uint64_t value = 0;
      if (gdb_server_->GetWasmLocal(module_id, frame_index, index, &value)) {
        pkt_out->AddBlock(&value, sizeof(value));
        return ErrorCode::None;
      } else {
        return ErrorCode::Failed;
      }
    }
    return ErrorCode::BadFormat;
  }

  // Read Wasm memory.
  // IN : $qWasmMem:memId;addr;len
  // OUT: $xx..xx
  if (toks[0] == "WasmMem") {
    if (toks.size() == 4) {
      uint32_t module_id =
          static_cast<uint32_t>(strtol(toks[1].data(), nullptr, 10));
      uint32_t address =
          static_cast<uint32_t>(strtol(toks[2].data(), nullptr, 16));
      uint32_t length =
          static_cast<uint32_t>(strtol(toks[3].data(), nullptr, 16));
      if (length > Transport::kBufSize / 2) {
        return ErrorCode::BadArgs;
      }
      uint8_t buff[Transport::kBufSize];
      uint32_t read =
          gdb_server_->GetWasmMemory(module_id, address, buff, length);
      if (read > 0) {
        pkt_out->AddBlock(buff, read);
        return ErrorCode::None;
      } else {
        return ErrorCode::Failed;
      }
    }
    return ErrorCode::BadFormat;
  }

  // No match so far, check the property cache.
  QueryPropertyMap::const_iterator it = query_properties_.find(toks[0]);
  if (it != query_properties_.end()) {
    pkt_out->AddString(it->second.data());
  }
  // If not found, just send an empty response.
  return ErrorCode::None;
}

// A Stop-reply packet has the format:
//   Sxx
// or:
//   Txx<name1>:<value1>;...;<nameN>:<valueN>
// where 'xx' is a two-digit hex number that represents the stop signal
// and the <name>:<value> pairs are used to report additional information,
// like the thread id.
void Target::SetStopReply(Packet* pkt_out) const {
  pkt_out->AddRawChar('T');
  pkt_out->AddWord8(cur_signal_);

  // Adds 'thread-pcs:<pc1>,...,<pcN>;' A list of pc values for all threads that
  // currently exist in the process.
  char buff[64];
  snprintf(buff, sizeof(buff), "thread-pcs:%" PRIx64 ";",
           static_cast<uint64_t>(GetCurrentPc()));
  pkt_out->AddString(buff);

  // Adds 'thread:<tid>;' pair. Note that a terminating ';' is required.
  pkt_out->AddString("thread:");
  pkt_out->AddNumberSep(kThreadId, ';');
}

wasm_addr_t Target::GetCurrentPc() const { return wasm_addr_t(0); }

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
