// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/encoded-c-signature.h"

#include "include/v8-fast-api-calls.h"
#include "src/base/bits.h"
#include "src/base/logging.h"

namespace v8 {
namespace internal {

int EncodedCSignature::FPParameterCount() const {
  CHECK(IsValid());
  return base::bits::CountPopulation(bitfield_ & ~(1 << kReturnIndex));
}

EncodedCSignature::EncodedCSignature(const CFunctionInfo* signature) {
  parameter_count_ = static_cast<int>(signature->ArgumentCount());
  for (int i = 0; i < parameter_count_; ++i) {
    if (CTypeInfo::IsFloatingPointType(signature->ArgumentInfo(i).GetType())) {
      SetFloat(i);
    }
  }
  // The struct holding the options of the CFunction (e.g. callback) is not
  // included in the number of regular parameters, so we add it manually here.
  if (signature->HasOptions()) {
    parameter_count_++;
  }
  if (CTypeInfo::IsFloatingPointType(signature->ReturnInfo().GetType())) {
    if (signature->ReturnInfo().GetType() == CTypeInfo::Type::kFloat64) {
      SetReturnFloat64();
    } else {
      SetReturnFloat32();
    }
  }
}

}  // namespace internal
}  // namespace v8
