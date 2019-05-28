
// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FUNCTION_KIND_H_
#define V8_FUNCTION_KIND_H_

#include "src/utils.h"

namespace v8 {
namespace internal {

enum FunctionKind : uint8_t {
  // BEGIN constructable functions
  kNormalFunction,
  kModule,
  // BEGIN class constructors
  // BEGIN base constructors
  kBaseConstructor,
  // BEGIN default constructors
  kDefaultBaseConstructor,
  // END base constructors
  // BEGIN derived cosntructors
  kDefaultDerivedConstructor,
  // END default constructors
  kDerivedConstructor,
  // END derived costructors
  // END class cosntructors
  // END constructable functions.
  // BEGIN accessors
  kGetterFunction,
  kSetterFunction,
  // END accessors
  // BEGIN arrow functions
  kArrowFunction,
  // BEGIN async functions
  kAsyncArrowFunction,
  // END arrow functions
  kAsyncFunction,
  // BEGIN concise methods 1
  kAsyncConciseMethod,
  // BEGIN generators
  kAsyncConciseGeneratorMethod,
  // END concise methods 1
  kAsyncGeneratorFunction,
  // END async functions
  kGeneratorFunction,
  // BEGIN concise methods 2
  kConciseGeneratorMethod,
  // END generators
  kConciseMethod,
  kClassMembersInitializerFunction,
  // END concise methods 2

  kLastFunctionKind = kClassMembersInitializerFunction,
};

inline bool IsArrowFunction(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kArrowFunction,
                   FunctionKind::kAsyncArrowFunction);
}

inline bool IsModule(FunctionKind kind) {
  return kind == FunctionKind::kModule;
}

inline bool IsAsyncGeneratorFunction(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kAsyncConciseGeneratorMethod,
                   FunctionKind::kAsyncGeneratorFunction);
}

inline bool IsGeneratorFunction(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kAsyncConciseGeneratorMethod,
                   FunctionKind::kConciseGeneratorMethod);
}

inline bool IsAsyncFunction(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kAsyncArrowFunction,
                   FunctionKind::kAsyncGeneratorFunction);
}

inline bool IsResumableFunction(FunctionKind kind) {
  return IsGeneratorFunction(kind) || IsAsyncFunction(kind) || IsModule(kind);
}

inline bool IsConciseMethod(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kAsyncConciseMethod,
                   FunctionKind::kAsyncConciseGeneratorMethod) ||
         IsInRange(kind, FunctionKind::kConciseGeneratorMethod,
                   FunctionKind::kClassMembersInitializerFunction);
}

inline bool IsStrictFunctionWithoutPrototype(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kGetterFunction,
                   FunctionKind::kAsyncArrowFunction) ||
         IsInRange(kind, FunctionKind::kAsyncConciseMethod,
                   FunctionKind::kAsyncConciseGeneratorMethod) ||
         IsInRange(kind, FunctionKind::kConciseGeneratorMethod,
                   FunctionKind::kClassMembersInitializerFunction);
}

inline bool IsGetterFunction(FunctionKind kind) {
  return kind == FunctionKind::kGetterFunction;
}

inline bool IsSetterFunction(FunctionKind kind) {
  return kind == FunctionKind::kSetterFunction;
}

inline bool IsAccessorFunction(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kGetterFunction,
                   FunctionKind::kSetterFunction);
}

inline bool IsDefaultConstructor(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kDefaultBaseConstructor,
                   FunctionKind::kDefaultDerivedConstructor);
}

inline bool IsBaseConstructor(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kBaseConstructor,
                   FunctionKind::kDefaultBaseConstructor);
}

inline bool IsDerivedConstructor(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kDefaultDerivedConstructor,
                   FunctionKind::kDerivedConstructor);
}

inline bool IsClassConstructor(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kBaseConstructor,
                   FunctionKind::kDerivedConstructor);
}

inline bool IsClassMembersInitializerFunction(FunctionKind kind) {
  return kind == FunctionKind::kClassMembersInitializerFunction;
}

inline bool IsConstructable(FunctionKind kind) {
  return IsInRange(kind, FunctionKind::kNormalFunction,
                   FunctionKind::kDerivedConstructor);
}

inline const char* FunctionKind2String(FunctionKind kind) {
  switch (kind) {
    case FunctionKind::kNormalFunction:
      return "NormalFunction";
    case FunctionKind::kArrowFunction:
      return "ArrowFunction";
    case FunctionKind::kGeneratorFunction:
      return "GeneratorFunction";
    case FunctionKind::kConciseMethod:
      return "ConciseMethod";
    case FunctionKind::kDerivedConstructor:
      return "DerivedConstructor";
    case FunctionKind::kBaseConstructor:
      return "BaseConstructor";
    case FunctionKind::kGetterFunction:
      return "GetterFunction";
    case FunctionKind::kSetterFunction:
      return "SetterFunction";
    case FunctionKind::kAsyncFunction:
      return "AsyncFunction";
    case FunctionKind::kModule:
      return "Module";
    case FunctionKind::kClassMembersInitializerFunction:
      return "ClassMembersInitializerFunction";
    case FunctionKind::kDefaultBaseConstructor:
      return "DefaultBaseConstructor";
    case FunctionKind::kDefaultDerivedConstructor:
      return "DefaultDerivedConstructor";
    case FunctionKind::kAsyncArrowFunction:
      return "AsyncArrowFunction";
    case FunctionKind::kAsyncConciseMethod:
      return "AsyncConciseMethod";
    case FunctionKind::kConciseGeneratorMethod:
      return "ConciseGeneratorMethod";
    case FunctionKind::kAsyncConciseGeneratorMethod:
      return "AsyncConciseGeneratorMethod";
    case FunctionKind::kAsyncGeneratorFunction:
      return "AsyncGeneratorFunction";
  }
  UNREACHABLE();
}

inline std::ostream& operator<<(std::ostream& os, FunctionKind kind) {
  return os << FunctionKind2String(kind);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_FUNCTION_KIND_H_
