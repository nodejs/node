// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_GENERATORS_TYPING_GENERATORS_H_
#define V8_EXPERIMENTAL_REDLINE_GENERATORS_TYPING_GENERATORS_H_

#include <cstdint>
#include <functional>
#include <string_view>
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

// A function for the JIT natives, and how the warm-up calls call it. The
// call of a plain function is `name(...)`. The call of a method of a
// visible object or class is `receiver.method(...)`. The call of a getter
// or a setter is `name.call(receiver, ...)`. A class gets `new`. Thus
// `this`, the home object and the brand checks are valid in the compiled
// code.
struct JitTarget {
  std::string_view name;      // the function that the natives get
  std::string_view receiver;  // `this` of the calls, empty for a plain call
  std::string_view method;    // the key of a method of `receiver`
  bool construct = false;     // the calls are `new name(...)`
  bool is_generator = false;  // a new generator function: calls need next()
  int max_args = -1;          // most arguments of a call, -1: no limit

  // A call of the function with `args`.
  fast::Expression* Call(fast::Tree* tree,
                         std::vector<fast::Expression*> args) const;
};

// Picks a method, getter or setter of a visible object or class, or a
// visible class constructor, from the program text. Appends to `out` the
// declaration `const jitN = ...` of the function if it is not a variable.
// Returns false if no visible variable has one.
bool ReceiverJitTarget(GenContext& ctx, std::vector<fast::Statement*>* out,
                       JitTarget* target);

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
