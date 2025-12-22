/* Copyright 2022 - 2025 R. Thomas
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
#ifndef LIEF_ASM_EBPF_REGISTER_H
#define LIEF_ASM_EBPF_REGISTER_H
namespace LIEF {
namespace assembly {
namespace ebpf {
enum class REG;
const char* get_register_name(REG r);

enum class REG {
  NoRegister = 0,
  R0 = 1,
  R1 = 2,
  R2 = 3,
  R3 = 4,
  R4 = 5,
  R5 = 6,
  R6 = 7,
  R7 = 8,
  R8 = 9,
  R9 = 10,
  R10 = 11,
  R11 = 12,
  W0 = 13,
  W1 = 14,
  W2 = 15,
  W3 = 16,
  W4 = 17,
  W5 = 18,
  W6 = 19,
  W7 = 20,
  W8 = 21,
  W9 = 22,
  W10 = 23,
  W11 = 24,
  NUM_TARGET_REGS = 25,
};

}
}
}
#endif
