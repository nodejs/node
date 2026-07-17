/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PE_UNWIND_CODE_AARCH64_H
#define LIEF_PE_UNWIND_CODE_AARCH64_H

namespace LIEF {

namespace PE {
/// This namespace wraps code related to PE-ARM64 unwinding code
namespace unwind_aarch64 {

enum class OPCODES {
  ALLOC_S,
  SAVE_R19R20_X,
  SAVE_FPLR,
  SAVE_FPLR_X,
  ALLOC_M,
  SAVE_REGP,
  SAVE_REGP_X,
  SAVE_REG,
  SAVE_REG_X,
  SAVE_LRPAIR,
  SAVE_FREGP,
  SAVE_FREGP_X,
  SAVE_FREG,
  SAVE_FREG_X,
  ALLOC_Z,
  ALLOC_L,
  SETFP,
  ADDFP,
  NOP,
  END,
  END_C,
  SAVE_NEXT,
  SAVE_ANY_REG, E7,
  TRAP_FRAME,
  MACHINE_FRAME,
  CONTEXT,
  EC_CONTEXT,
  CLEAR_UNWOUND_TO_CALL,
  PAC_SIGN_LR,
  SAVE_PREG,
  SAVE_ZREG,
};
}
}
}
#endif
