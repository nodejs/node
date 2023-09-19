// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_STRING_CONSTANTS_H_
#define V8_CODEGEN_STRING_CONSTANTS_H_

#include "src/handles/handles.h"
#include "src/objects/string.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

enum class StringConstantKind {
  kStringLiteral,
  kNumberToStringConstant,
  kStringCons
};

class StringConstantBase : public ZoneObject {
 public:
  explicit StringConstantBase(StringConstantKind kind) : kind_(kind) {}

  StringConstantKind kind() const { return kind_; }
  Handle<String> AllocateStringConstant(Isolate* isolate) const;

  size_t GetMaxStringConstantLength() const;

  bool operator==(const StringConstantBase& other) const;

 private:
  void Memoize(Handle<String> flattened) const { flattened_ = flattened; }

  StringConstantKind kind_;
  mutable Handle<String> flattened_ = Handle<String>::null();
};

size_t hash_value(StringConstantBase const& base);

class StringLiteral final : public StringConstantBase {
 public:
  explicit StringLiteral(Handle<String> str, size_t length)
      : StringConstantBase(StringConstantKind::kStringLiteral),
        str_(str),
        length_(length) {}

  Handle<String> str() const { return str_; }

  size_t GetMaxStringConstantLength() const;

 private:
  Handle<String> str_;
  size_t length_;  // We store this separately to avoid accessing the heap.
};

bool operator==(StringLiteral const& lhs, StringLiteral const& rhs);
bool operator!=(StringLiteral const& lhs, StringLiteral const& rhs);

size_t hash_value(StringLiteral const& parameters);

std::ostream& operator<<(std::ostream& os, StringLiteral const& parameters);

class NumberToStringConstant final : public StringConstantBase {
 public:
  explicit NumberToStringConstant(double num)
      : StringConstantBase(StringConstantKind::kNumberToStringConstant),
        num_(num) {}

  double num() const { return num_; }

  size_t GetMaxStringConstantLength() const;

 private:
  double num_;
};

bool operator==(NumberToStringConstant const& lhs,
                NumberToStringConstant const& rhs);
bool operator!=(NumberToStringConstant const& lhs,
                NumberToStringConstant const& rhs);

size_t hash_value(NumberToStringConstant const& parameters);

std::ostream& operator<<(std::ostream& os,
                         NumberToStringConstant const& parameters);

class StringCons final : public StringConstantBase {
 public:
  explicit StringCons(const StringConstantBase* lhs,
                      const StringConstantBase* rhs)
      : StringConstantBase(StringConstantKind::kStringCons),
        lhs_(lhs),
        rhs_(rhs) {}

  const StringConstantBase* lhs() const { return lhs_; }
  const StringConstantBase* rhs() const { return rhs_; }

  size_t GetMaxStringConstantLength() const;

 private:
  const StringConstantBase* lhs_;
  const StringConstantBase* rhs_;
};

bool operator==(StringCons const& lhs, StringCons const& rhs);
bool operator!=(StringCons const& lhs, StringCons const& rhs);

size_t hash_value(StringCons const& parameters);

std::ostream& operator<<(std::ostream& os, StringCons const& parameters);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_STRING_CONSTANTS_H_
