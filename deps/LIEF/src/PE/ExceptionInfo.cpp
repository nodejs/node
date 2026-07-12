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
#include "LIEF/PE/ExceptionInfo.hpp"
#include "LIEF/PE/exceptions_info/RuntimeFunctionX64.hpp"
#include "LIEF/PE/exceptions_info/RuntimeFunctionAArch64.hpp"

#include "LIEF/BinaryStream/BinaryStream.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"

namespace LIEF::PE {

std::unique_ptr<ExceptionInfo>
  ExceptionInfo::parse(Parser& ctx, BinaryStream& strm, Header::MACHINE_TYPES arch)
{
  uint64_t pos = strm.pos();
  switch (arch) {
    case Header::MACHINE_TYPES::AMD64:
      {
        if (auto F = RuntimeFunctionX64::parse(ctx, strm)) {
          F->offset(pos);
          return F;
        }
        return nullptr;
      }
    case Header::MACHINE_TYPES::ARM64:
      {
        if (auto F = RuntimeFunctionAArch64::parse(ctx, strm)) {
          F->offset(pos);
          return F;
        }
        return nullptr;
      }

    default:
      return nullptr;
  }
  return nullptr;
}

std::unique_ptr<ExceptionInfo> ExceptionInfo::parse(Parser& ctx, BinaryStream& strm) {
  const Header::MACHINE_TYPES arch = ctx.bin().header().machine();
  return parse(ctx, strm, arch);
}

}
