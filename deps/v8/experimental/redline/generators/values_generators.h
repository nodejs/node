// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_GENERATORS_VALUES_GENERATORS_H_
#define V8_EXPERIMENTAL_REDLINE_GENERATORS_VALUES_GENERATORS_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "experimental/redline/fast/ast.h"
#include "experimental/redline/generators/generator.h"

// Generators for operand kinds that Fuzzilli makes and Redline did not:
// builtin instances, constructor functions, iterables, disposables,
// resizable buffers, special heap values and GC natives. Several of them
// are value declarations that cannot throw, for the Prefix mutator.
namespace redline {

class CodeGen;

// The signature of a generator function (the GEN macro of generators.cc).
using ValueGeneratorFn = bool (*)(GenContext& ctx, CodeGen& cg,
                                  std::vector<fast::Statement*>* out);

// The `add` helper of GeneratorSet::CreateDefault().
using AddGeneratorFn =
    std::function<void(const char* name, const char* description, double weight,
                       uint32_t requirements, ValueGeneratorFn fn)>;

// Registers the generators of values_generators.cc.
void AddValueGenerators(const AddGeneratorFn& add);

// The value declarations of values_generators.cc that cannot throw. The
// Prefix mutator uses them in addition to its own list.
const std::vector<std::string>& CannotThrowValueGenerators();

// The body of the UsingDecl and AwaitUsingDecl generators. It is a block
// with a `using` (or `await using`) declaration and generated code. The
// value is a disposable value, null or undefined.
bool GenUsingBlock(GenContext& ctx, CodeGen& cg,
                   std::vector<fast::Statement*>* out, bool is_await);

// For CodeGen::Value(). With a small probability, it returns a call of a
// native that makes a special value of `type`. Examples are a hole NaN, a
// thin string and a Wasm struct. In all other cases, it returns nullptr.
fast::Expression* MaybeSpecialValue(CodeGen& cg, JsType type);

// For the NativeCall generator: the arguments of the natives that need a
// fixed argument shape (a long string and an integer offset for
// %ConstructSlicedString). Returns false for the other natives.
bool NativeArgumentsFor(CodeGen& cg, std::string_view name,
                        std::vector<fast::Expression*>* args);

// For JitOptimize and LazyDeopt: sometimes inserts a GC or allocation
// native at a random position of the body of the function to optimize.
void MaybeInsertGcNative(GenContext& ctx, CodeGen& cg,
                         std::vector<fast::Statement*>* body);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_GENERATORS_VALUES_GENERATORS_H_
