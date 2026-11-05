// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_KNOWN_KEYS_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_KNOWN_KEYS_H_

#include <string_view>
#include <vector>

#include "experimental/redline/core/builder.h"

// The property keys that the program text gives a value: the keys of an
// object literal, the members of a class and the stores `x.p = v`. The
// Builder derives BindingInfo::keys with these functions, and generators
// record the keys of the values that they declare.
namespace redline {

// The name of a property key that is not computed or that is a literal: an
// identifier, a string or an integer. Empty for other keys, for example
// private names. Interns the name in `tree`.
std::string_view KeyName(fast::Tree* tree, const fast::Expression* key,
                         bool computed);
// Adds `key` to `keys`. A key that is already there takes the type of the
// later fact: the type of the last store is the type of the property.
void AddKnownKey(std::vector<KnownKey>* keys, const KnownKey& key);
// Adds the keys of an object literal.
void AddObjectLiteralKeys(fast::Tree* tree,
                          const fast::ObjectExpression* object,
                          std::vector<KnownKey>* keys);
// Adds the public keys of a class: static keys, and the keys of its
// instances (on_instance), with the `this.x = v` stores of its constructor.
void AddClassKeys(fast::Tree* tree, const fast::ClassBody* body,
                  std::vector<KnownKey>* keys);
// Adds to `keys` the keys of the instances of a class. These are the keys
// of `class_keys` that are on_instance. The copies are not on_instance.
void AddInstanceKeys(const std::vector<KnownKey>& class_keys,
                     std::vector<KnownKey>* keys);
// The keys of the value of `binding`, derived from the program text. They
// come from its initializer (an object literal, a class or `new C(...)` of
// a known class) and from the stores `binding.p = v`.
void DeriveKnownKeys(Builder& builder, const fast::Binding* binding,
                     std::vector<KnownKey>* keys);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_KNOWN_KEYS_H_
