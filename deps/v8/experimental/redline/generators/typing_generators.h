// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_GENERATORS_TYPING_GENERATORS_H_
#define V8_EXPERIMENTAL_REDLINE_GENERATORS_TYPING_GENERATORS_H_

#include <cstdint>
#include <functional>
#include <vector>

#include "experimental/redline/generators/codegen.h"

namespace redline {

// The argument lists for the calls of one function in a JIT warm-up, after
// Fuzzilli's chooseArgumentLists() (V8CommonProfile.swift:155-194). The
// first call gets the first list. A later call reuses an earlier list with
// a given probability, else it gets a new list. There are at most
// `max_lists` lists. A reused list is a copy of the same expressions, so
// that the function sees the same types again.
class ArgumentLists {
 public:
  using Maker = std::function<std::vector<fast::Expression*>()>;
  ArgumentLists(CodeGen& cg, Maker make, int max_lists);

  // The arguments for the next call.
  std::vector<fast::Expression*> Next(double reuse_chance);
  // Arguments of other types than the first list, for a call that makes
  // the optimized code deoptimize. Empty if the first list is empty.
  std::vector<fast::Expression*> OtherTypes();

 private:
  std::vector<fast::Expression*> Copy(
      const std::vector<fast::Expression*>& list);

  CodeGen& cg_;
  Maker make_;
  size_t max_lists_;
  std::vector<std::vector<fast::Expression*>> lists_;
};

// The type of an argument expression: the type of the visible variable
// that it names, or the guess for the expression.
JsType ArgumentType(const GenContext& ctx, const fast::Expression* argument);

// An iterable of any kind. It is a visible array, string, Map, Set or typed
// array, or an iterator over one of them. It can also be a new Map or Set,
// a generator object, or an object with a [Symbol.iterator] method.
fast::Expression* IterableSource(GenContext& ctx, CodeGen& cg);

// The function type of the generators of typing_generators.cc.
using TypingGeneratorFn = bool (*)(GenContext& ctx, CodeGen& cg,
                                   std::vector<fast::Statement*>* out);

// Adds the generators of typing_generators.cc with `add` (name,
// description, weight, requirements, function).
void RegisterTypingGenerators(
    const std::function<void(const char*, const char*, double, uint32_t,
                             TypingGeneratorFn)>& add);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_GENERATORS_TYPING_GENERATORS_H_
