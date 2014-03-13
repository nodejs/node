// Copyright 2013 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_A64

#if defined(USE_SIMULATOR)

#include "a64/debugger-a64.h"

namespace v8 {
namespace internal {


void Debugger::VisitException(Instruction* instr) {
  switch (instr->Mask(ExceptionMask)) {
    case HLT: {
      if (instr->ImmException() == kImmExceptionIsDebug) {
        // Read the arguments encoded inline in the instruction stream.
        uint32_t code;
        uint32_t parameters;
        char const * message;

        ASSERT(sizeof(*pc_) == 1);
        memcpy(&code, pc_ + kDebugCodeOffset, sizeof(code));
        memcpy(&parameters, pc_ + kDebugParamsOffset, sizeof(parameters));
        message = reinterpret_cast<char const *>(pc_ + kDebugMessageOffset);

        if (message[0] == '\0') {
          fprintf(stream_, "Debugger hit %" PRIu32 ".\n", code);
        } else {
          fprintf(stream_, "Debugger hit %" PRIu32 ": %s\n", code, message);
        }

        // Other options.
        switch (parameters & kDebuggerTracingDirectivesMask) {
          case TRACE_ENABLE:
            set_log_parameters(log_parameters() | parameters);
            break;
          case TRACE_DISABLE:
            set_log_parameters(log_parameters() & ~parameters);
            break;
          case TRACE_OVERRIDE:
            set_log_parameters(parameters);
            break;
          default:
            // We don't support a one-shot LOG_DISASM.
            ASSERT((parameters & LOG_DISASM) == 0);
            // Don't print information that is already being traced.
            parameters &= ~log_parameters();
            // Print the requested information.
            if (parameters & LOG_SYS_REGS) PrintSystemRegisters(true);
            if (parameters & LOG_REGS) PrintRegisters(true);
            if (parameters & LOG_FP_REGS) PrintFPRegisters(true);
        }

        // Check if the debugger should break.
        if (parameters & BREAK) OS::DebugBreak();

        // The stop parameters are inlined in the code. Skip them:
        //  - Skip to the end of the message string.
        pc_ += kDebugMessageOffset + strlen(message) + 1;
        //  - Advance to the next aligned location.
        pc_ = AlignUp(pc_, kInstructionSize);
        //  - Verify that the unreachable marker is present.
        ASSERT(reinterpret_cast<Instruction*>(pc_)->Mask(ExceptionMask) == HLT);
        ASSERT(reinterpret_cast<Instruction*>(pc_)->ImmException() ==
               kImmExceptionIsUnreachable);
        //  - Skip past the unreachable marker.
        pc_ += kInstructionSize;
        pc_modified_ = true;
      } else {
        Simulator::VisitException(instr);
      }
      break;
    }

    default:
      UNIMPLEMENTED();
  }
}


} }  // namespace v8::internal

#endif  // USE_SIMULATOR

#endif  // V8_TARGET_ARCH_A64
