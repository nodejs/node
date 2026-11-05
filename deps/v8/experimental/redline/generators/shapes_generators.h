// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_GENERATORS_SHAPES_GENERATORS_H_
#define V8_EXPERIMENTAL_REDLINE_GENERATORS_SHAPES_GENERATORS_H_

#include <cstdint>
#include <vector>

#include "experimental/redline/generators/codegen.h"
#include "experimental/redline/generators/generator.h"

namespace redline {

// One generator of shapes_generators.cc or template_generators.cc.
// GeneratorSet::CreateDefault() registers these entries after the generators
// of generators.cc.
struct ShapesGenerator {
  const char* name;
  const char* description;
  double weight;
  uint32_t requirements;
  bool (*fn)(GenContext& ctx, CodeGen& cg, std::vector<fast::Statement*>* out);
};

// Generators for program shapes that Fuzzilli makes and the other generators
// do not make. They make imitations of values, literal factories, number
// chains, control flow shapes, well-known symbol hooks and iterator chains.
std::vector<ShapesGenerator> ShapesGenerators();

// Fuzzilli's program templates (V8CommonProfile.swift, ProgramTemplates.swift)
// as generators with a low weight.
std::vector<ShapesGenerator> TemplateGenerators();

// An expression that evaluates to an iterable of any kind. It can be a
// visible array, string, Map, Set or typed array, or a string literal. It
// can also be a new Map or Set, an iterator of an array, or a generator
// object. Spreads and `yield*` use it.
fast::Expression* AnyIterable(CodeGen& cg);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_GENERATORS_SHAPES_GENERATORS_H_
