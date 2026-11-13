// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_GENERATORS_API_GENERATORS_H_
#define V8_EXPERIMENTAL_REDLINE_GENERATORS_API_GENERATORS_H_

#include <cstdint>
#include <vector>

#include "experimental/redline/generators/codegen.h"
#include "experimental/redline/generators/generator.h"

namespace redline {

// One generator of api_generators.cc. GeneratorSet::CreateDefault() registers
// these entries after the generators of generators.cc.
struct ApiGenerator {
  const char* name;
  const char* description;
  double weight;
  uint32_t requirements;
  bool (*fn)(GenContext& ctx, CodeGen& cg, std::vector<fast::Statement*>* out);
};

// Generators for builtin APIs that need more than one typed call: promise
// combinators and async iteration, and the host objects of d8.test.
std::vector<ApiGenerator> ApiGenerators();

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_GENERATORS_API_GENERATORS_H_
