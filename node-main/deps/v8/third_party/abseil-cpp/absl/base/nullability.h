// Copyright 2023 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: nullability.h
// -----------------------------------------------------------------------------
//
// This header file defines a set of annotations for designating the expected
// nullability of pointers. These annotations allow you to designate pointers in
// one of three classification states:
//
//  * "Non-null" (for pointers annotated `absl_nonnull`), indicating that it is
//    invalid for the given pointer to ever be null.
//  * "Nullable" (for pointers annotated `absl_nullable`), indicating that it is
//    valid for the given pointer to be null.
//  * "Unknown" (for pointers annotated `absl_nullability_unknown`), indicating
//    that the given pointer has not yet been classified as either nullable or
//    non-null. This is the default state of unannotated pointers.
//
// NOTE: Unannotated pointers implicitly bear the annotation
// `absl_nullability_unknown`; you should rarely, if ever, see this annotation
// used in the codebase explicitly.
//
// -----------------------------------------------------------------------------
// Nullability and Contracts
// -----------------------------------------------------------------------------
//
// These nullability annotations allow you to more clearly specify contracts on
// software components by narrowing the *preconditions*, *postconditions*, and
// *invariants* of pointer state(s) in any given interface. It then depends on
// context who is responsible for fulfilling the annotation's requirements.
//
// For example, a function may receive a pointer argument. Designating that
// pointer argument as "non-null" tightens the precondition of the contract of
// that function. It is then the responsibility of anyone calling such a
// function to ensure that the passed pointer is not null.
//
// Similarly, a function may have a pointer as a return value. Designating that
// return value as "non-null" tightens the postcondition of the contract of that
// function. In this case, however, it is the responsibility of the function
// itself to ensure that the returned pointer is not null.
//
// Clearly defining these contracts allows providers (and consumers) of such
// pointers to have more confidence in their null state. If a function declares
// a return value as "non-null", for example, the caller should not need to
// check whether the returned value is `nullptr`; it can simply assume the
// pointer is valid.
//
// Of course most interfaces already have expectations on the nullability state
// of pointers, and these expectations are, in effect, a contract; often,
// however, those contracts are either poorly or partially specified, assumed,
// or misunderstood. These nullability annotations are designed to allow you to
// formalize those contracts within the codebase.
//
// -----------------------------------------------------------------------------
// Annotation Syntax
// -----------------------------------------------------------------------------
//
// The annotations should be positioned as a qualifier for the pointer type. For
// example, the position of `const` when declaring a const pointer (not a
// pointer to a const type) is the position you should also use for these
// annotations.
//
// Example:
//
// // A const non-null pointer to an `Employee`.
// Employee* absl_nonnull const e;
//
// // A non-null pointer to a const `Employee`.
// const Employee* absl_nonnull e;
//
// // A non-null pointer to a const nullable pointer to an `Employee`.
// Employee* absl_nullable const* absl_nonnull e = nullptr;
//
// // A non-null function pointer.
// void (*absl_nonnull func)(int, double);
//
// // A non-null array of `Employee`s as a parameter.
// void func(Employee employees[absl_nonnull]);
//
// // A non-null std::unique_ptr to an `Employee`.
// // As with `const`, it is possible to place the annotation on either side of
// // a named type not ending in `*`, but placing it before the type it
// // describes is preferred, unless inconsistent with surrounding code.
// absl_nonnull std::unique_ptr<Employee> employee;
//
// // Invalid annotation usage – this attempts to declare a pointer to a
// // nullable `Employee`, which is meaningless.
// absl_nullable Employee* e;
//
// -----------------------------------------------------------------------------
// Using Nullability Annotations
// -----------------------------------------------------------------------------
//
// Each annotation acts as a form of documentation about the contract for the
// given pointer. Each annotation requires providers or consumers of these
// pointers across API boundaries to take appropriate steps when setting or
// using these pointers:
//
// * "Non-null" pointers should never be null. It is the responsibility of the
//   provider of this pointer to ensure that the pointer may never be set to
//   null. Consumers of such pointers can treat such pointers as non-null.
// * "Nullable" pointers may or may not be null. Consumers of such pointers
//   should precede any usage of that pointer (e.g. a dereference operation)
//   with a a `nullptr` check.
// * "Unknown" pointers may be either "non-null" or "nullable" but have not been
//   definitively determined to be in either classification state. Providers of
//   such pointers across API boundaries should determine --  over time -- to
//   annotate the pointer in either of the above two states. Consumers of such
//   pointers across an API boundary should continue to treat such pointers as
//   they currently do.
//
// Example:
//
// // PaySalary() requires the passed pointer to an `Employee` to be non-null.
// void PaySalary(Employee* absl_nonnull e) {
//   pay(e->salary);  // OK to dereference
// }
//
// // CompleteTransaction() guarantees the returned pointer to an `Account` to
// // be non-null.
// Account* absl_nonnull balance CompleteTransaction(double fee) {
// ...
// }
//
// // Note that specifying a nullability annotation does not prevent someone
// // from violating the contract:
//
// Employee* absl_nullable find(Map& employees, std::string_view name);
//
// void g(Map& employees) {
//   Employee *e = find(employees, "Pat");
//   // `e` can now be null.
//   PaySalary(e); // Violates contract, but compiles!
// }
//
// Nullability annotations, in other words, are useful for defining and
// narrowing contracts; *enforcement* of those contracts depends on use and any
// additional (static or dynamic analysis) tooling.
//
// NOTE: The "unknown" annotation state indicates that a pointer's contract has
// not yet been positively identified. The unknown state therefore acts as a
// form of documentation of your technical debt, and a codebase that adopts
// nullability annotations should aspire to annotate every pointer as either
// "non-null" or "nullable".
//
// -----------------------------------------------------------------------------
// Applicability of Nullability Annotations
// -----------------------------------------------------------------------------
//
// By default, nullability annotations are applicable to raw and smart
// pointers. User-defined types can indicate compatibility with nullability
// annotations by adding the ABSL_NULLABILITY_COMPATIBLE attribute.
//
// // Example:
// struct ABSL_NULLABILITY_COMPATIBLE MyPtr {
//   ...
// };
//
// Note: Compilers that don't support the `nullability_on_classes` feature will
// allow nullability annotations to be applied to any type, not just ones
// marked with `ABSL_NULLABILITY_COMPATIBLE`.
//
// DISCLAIMER:
// ===========================================================================
// These nullability annotations are primarily a human readable signal about the
// intended contract of the pointer. They are not *types* and do not currently
// provide any correctness guarantees. For example, a pointer annotated as
// `absl_nonnull` is *not guaranteed* to be non-null, and the compiler won't
// alert or prevent assignment of a `T* absl_nullable` to a `T* absl_nonnull`.
// ===========================================================================
#ifndef ABSL_BASE_NULLABILITY_H_
#define ABSL_BASE_NULLABILITY_H_

#include "absl/base/config.h"

// ABSL_POINTERS_DEFAULT_NONNULL
//
// This macro specifies that all unannotated pointer types within the given
// file are designated as nonnull (instead of the default "unknown"). This macro
// exists as a standalone statement and applies default nonnull behavior to all
// subsequent pointers; as a result, place this macro as the first non-comment,
// non-`#include` line in a file.
//
// Example:
//
//     #include "absl/base/nullability.h"
//
//     ABSL_POINTERS_DEFAULT_NONNULL
//
//     void FillMessage(Message *m);                  // implicitly non-null
//     T* absl_nullable GetNullablePtr();           // explicitly nullable
//     T* absl_nullability_unknown GetUnknownPtr();  // explicitly unknown
//
// The macro can be safely used in header files – it will not affect any files
// that include it.
//
// In files with the macro, plain `T*` syntax means `T* absl_nonnull`, and the
// exceptions (`absl_nullable` and `absl_nullability_unknown`) must be marked
// explicitly. The same holds, correspondingly, for smart pointer types.
//
// For comparison, without the macro, all unannotated pointers would default to
// unknown, and otherwise require explicit annotations to change this behavior:
//
//     #include "absl/base/nullability.h"
//
//     void FillMessage(Message* absl_nonnull m);  // explicitly non-null
//     T* absl_nullable GetNullablePtr();          // explicitly nullable
//     T* GetUnknownPtr();                           // implicitly unknown
//
// No-op except for being a human readable signal.
#define ABSL_POINTERS_DEFAULT_NONNULL

#if defined(__clang__) && !defined(__OBJC__) && \
    ABSL_HAVE_FEATURE(nullability_on_classes)
// absl_nonnull (default with `ABSL_POINTERS_DEFAULT_NONNULL`)
//
// The indicated pointer is never null. It is the responsibility of the provider
// of this pointer across an API boundary to ensure that the pointer is never
// set to null. Consumers of this pointer across an API boundary may safely
// dereference the pointer.
//
// Example:
//
// // `employee` is designated as not null.
// void PaySalary(Employee* absl_nonnull employee) {
//   pay(*employee);  // OK to dereference
// }
#define absl_nonnull _Nonnull

// absl_nullable
//
// The indicated pointer may, by design, be either null or non-null. Consumers
// of this pointer across an API boundary should perform a `nullptr` check
// before performing any operation using the pointer.
//
// Example:
//
// // `employee` may  be null.
// void PaySalary(Employee* absl_nullable employee) {
//   if (employee != nullptr) {
//     Pay(*employee);  // OK to dereference
//   }
// }
#define absl_nullable _Nullable

// absl_nullability_unknown  (default without `ABSL_POINTERS_DEFAULT_NONNULL`)
//
// The indicated pointer has not yet been determined to be definitively
// "non-null" or "nullable." Providers of such pointers across API boundaries
// should, over time, annotate such pointers as either "non-null" or "nullable."
// Consumers of these pointers across an API boundary should treat such pointers
// with the same caution they treat currently unannotated pointers. Most
// existing code will have "unknown"  pointers, which should eventually be
// migrated into one of the above two nullability states: `absl_nonnull` or
//  `absl_nullable`.
//
// NOTE: For files that do not specify `ABSL_POINTERS_DEFAULT_NONNULL`,
// because this annotation is the global default state, unannotated pointers are
// are assumed to have "unknown" semantics. This assumption is designed to
// minimize churn and reduce clutter within the codebase.
//
// Example:
//
// // `employee`s nullability state is unknown.
// void PaySalary(Employee* absl_nullability_unknown employee) {
//   Pay(*employee); // Potentially dangerous. API provider should investigate.
// }
//
// Note that a pointer without an annotation, by default, is assumed to have the
// annotation `NullabilityUnknown`.
//
// // `employee`s nullability state is unknown.
// void PaySalary(Employee* employee) {
//   Pay(*employee); // Potentially dangerous. API provider should investigate.
// }
#define absl_nullability_unknown _Null_unspecified
#else
// No-op for non-Clang compilers or Objective-C.
#define absl_nonnull
// No-op for non-Clang compilers or Objective-C.
#define absl_nullable
// No-op for non-Clang compilers or Objective-C.
#define absl_nullability_unknown
#endif

// ABSL_NULLABILITY_COMPATIBLE
//
// Indicates that a class is compatible with nullability annotations.
//
// For example:
//
// struct ABSL_NULLABILITY_COMPATIBLE MyPtr {
//   ...
// };
//
// Note: Compilers that don't support the `nullability_on_classes` feature will
// allow nullability annotations to be applied to any type, not just ones marked
// with `ABSL_NULLABILITY_COMPATIBLE`.
#if ABSL_HAVE_FEATURE(nullability_on_classes)
#define ABSL_NULLABILITY_COMPATIBLE _Nullable
#else
#define ABSL_NULLABILITY_COMPATIBLE
#endif

#endif  // ABSL_BASE_NULLABILITY_H_
