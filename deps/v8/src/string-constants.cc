// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/string-constants.h"

#include "src/base/functional.h"
#include "src/dtoa.h"
#include "src/objects.h"
#include "src/objects/string-inl.h"

namespace v8 {
namespace internal {

Handle<String> StringConstantBase::AllocateStringConstant(
    Isolate* isolate) const {
  if (!flattened_.is_null()) {
    return flattened_;
  }

  Handle<String> result;
  switch (kind()) {
    case StringConstantKind::kStringLiteral: {
      result = static_cast<const StringLiteral*>(this)->str();
      break;
    }
    case StringConstantKind::kNumberToStringConstant: {
      auto num_constant = static_cast<const NumberToStringConstant*>(this);
      Handle<Object> num_obj =
          isolate->factory()->NewNumber(num_constant->num());
      result = isolate->factory()->NumberToString(num_obj);
      break;
    }
    case StringConstantKind::kStringCons: {
      Handle<String> lhs =
          static_cast<const StringCons*>(this)->lhs()->AllocateStringConstant(
              isolate);
      Handle<String> rhs =
          static_cast<const StringCons*>(this)->rhs()->AllocateStringConstant(
              isolate);
      result = isolate->factory()->NewConsString(lhs, rhs).ToHandleChecked();
      break;
    }
  }

  // TODO(mslekova): Normally we'd want to flatten the string here
  // but that results in OOM for too long strings.
  Memoize(result);
  return flattened_;
}

bool StringConstantBase::operator==(const StringConstantBase& other) const {
  if (kind() != other.kind()) return false;

  switch (kind()) {
    case StringConstantKind::kStringLiteral: {
      return static_cast<const StringLiteral*>(this) ==
             static_cast<const StringLiteral*>(&other);
    }
    case StringConstantKind::kNumberToStringConstant: {
      return static_cast<const NumberToStringConstant*>(this) ==
             static_cast<const NumberToStringConstant*>(&other);
    }
    case StringConstantKind::kStringCons: {
      return static_cast<const StringCons*>(this) ==
             static_cast<const StringCons*>(&other);
    }
  }
  UNREACHABLE();
}

size_t hash_value(StringConstantBase const& base) {
  switch (base.kind()) {
    case StringConstantKind::kStringLiteral: {
      return hash_value(*static_cast<const StringLiteral*>(&base));
    }
    case StringConstantKind::kNumberToStringConstant: {
      return hash_value(*static_cast<const NumberToStringConstant*>(&base));
    }
    case StringConstantKind::kStringCons: {
      return hash_value(*static_cast<const StringCons*>(&base));
    }
  }
  UNREACHABLE();
}

bool operator==(StringLiteral const& lhs, StringLiteral const& rhs) {
  return lhs.str().address() == rhs.str().address();
}

bool operator!=(StringLiteral const& lhs, StringLiteral const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(StringLiteral const& p) {
  return base::hash_combine(p.str().address());
}

std::ostream& operator<<(std::ostream& os, StringLiteral const& p) {
  return os << Brief(*p.str());
}

bool operator==(NumberToStringConstant const& lhs,
                NumberToStringConstant const& rhs) {
  return lhs.num() == rhs.num();
}

bool operator!=(NumberToStringConstant const& lhs,
                NumberToStringConstant const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(NumberToStringConstant const& p) {
  return base::hash_combine(p.num());
}

std::ostream& operator<<(std::ostream& os, NumberToStringConstant const& p) {
  return os << p.num();
}

bool operator==(StringCons const& lhs, StringCons const& rhs) {
  // TODO(mslekova): Think if we can express this in a more readable manner
  return *(lhs.lhs()) == *(rhs.lhs()) && *(lhs.rhs()) == *(rhs.rhs());
}

bool operator!=(StringCons const& lhs, StringCons const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(StringCons const& p) {
  return base::hash_combine(*(p.lhs()), *(p.rhs()));
}

std::ostream& operator<<(std::ostream& os, const StringConstantBase* base) {
  os << "DelayedStringConstant: ";
  switch (base->kind()) {
    case StringConstantKind::kStringLiteral: {
      os << *static_cast<const StringLiteral*>(base);
      break;
    }
    case StringConstantKind::kNumberToStringConstant: {
      os << *static_cast<const NumberToStringConstant*>(base);
      break;
    }
    case StringConstantKind::kStringCons: {
      os << *static_cast<const StringCons*>(base);
      break;
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, StringCons const& p) {
  return os << p.lhs() << ", " << p.rhs();
}

size_t StringConstantBase::GetMaxStringConstantLength() const {
  switch (kind()) {
    case StringConstantKind::kStringLiteral: {
      return static_cast<const StringLiteral*>(this)
          ->GetMaxStringConstantLength();
    }
    case StringConstantKind::kNumberToStringConstant: {
      return static_cast<const NumberToStringConstant*>(this)
          ->GetMaxStringConstantLength();
    }
    case StringConstantKind::kStringCons: {
      return static_cast<const StringCons*>(this)->GetMaxStringConstantLength();
    }
  }
  UNREACHABLE();
}

size_t StringLiteral::GetMaxStringConstantLength() const { return length_; }

size_t NumberToStringConstant::GetMaxStringConstantLength() const {
  return kBase10MaximalLength + 1;
}

size_t StringCons::GetMaxStringConstantLength() const {
  return lhs()->GetMaxStringConstantLength() +
         rhs()->GetMaxStringConstantLength();
}

}  // namespace internal
}  // namespace v8
