// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_SCOPEINFO_H_
#define V8_SCOPEINFO_H_

#include "variables.h"

namespace v8 { namespace internal {

// Scope information represents information about a functions's
// scopes (currently only one, because we don't do any inlining)
// and the allocation of the scope's variables. Scope information
// is stored in a compressed form with Code objects and is used
// at runtime (stack dumps, deoptimization, etc.).
//
// Historical note: In other VMs built by this team, ScopeInfo was
// usually called DebugInfo since the information was used (among
// other things) for on-demand debugging (Self, Smalltalk). However,
// DebugInfo seems misleading, since this information is primarily used
// in debugging-unrelated contexts.

// Forward defined as
// template <class Allocator = FreeStoreAllocationPolicy> class ScopeInfo;
template<class Allocator>
class ScopeInfo BASE_EMBEDDED {
 public:
  // Create a ScopeInfo instance from a scope.
  explicit ScopeInfo(Scope* scope);

  // Create a ScopeInfo instance from a Code object.
  explicit ScopeInfo(Code* code);

  // Write the ScopeInfo data into a Code object, and returns the
  // amount of space that was needed. If no Code object is provided
  // (NULL handle), Serialize() only returns the amount of space needed.
  //
  // This operations requires that the Code object has the correct amount
  // of space for the ScopeInfo data; otherwise the operation fails (fatal
  // error). Any existing scope info in the Code object is simply overwritten.
  int Serialize(Code* code);

  // Garbage collection support for scope info embedded in Code objects.
  // This code is in ScopeInfo because only here we should have to know
  // about the encoding.
  static void IterateScopeInfo(Code* code, ObjectVisitor* v);


  // --------------------------------------------------------------------------
  // Lookup

  Handle<String> function_name() const  { return function_name_; }

  Handle<String> parameter_name(int i) const  { return parameters_[i]; }
  int number_of_parameters() const  { return parameters_.length(); }

  Handle<String> stack_slot_name(int i) const  { return stack_slots_[i]; }
  int number_of_stack_slots() const  { return stack_slots_.length(); }

  Handle<String> context_slot_name(int i) const {
    return context_slots_[i - Context::MIN_CONTEXT_SLOTS];
  }
  int number_of_context_slots() const {
    int l = context_slots_.length();
    return l == 0 ? 0 : l + Context::MIN_CONTEXT_SLOTS;
  }

  Handle<String> LocalName(int i) const;
  int NumberOfLocals() const;

  // --------------------------------------------------------------------------
  // The following functions provide quick access to scope info details
  // for runtime routines w/o the need to explicitly create a ScopeInfo
  // object.
  //
  // ScopeInfo is the only class which should have to know about the
  // encoding of it's information in a Code object, which is why these
  // functions are in this class.

  // Does this scope call eval.
  static bool CallsEval(Code* code);

  // Return the number of stack slots for code.
  static int NumberOfStackSlots(Code* code);

  // Return the number of context slots for code.
  static int NumberOfContextSlots(Code* code);

  // Lookup support for scope info embedded in Code objects. Returns
  // the stack slot index for a given slot name if the slot is
  // present; otherwise returns a value < 0. The name must be a symbol
  // (canonicalized).
  static int StackSlotIndex(Code* code, String* name);

  // Lookup support for scope info embedded in Code objects. Returns the
  // context slot index for a given slot name if the slot is present; otherwise
  // returns a value < 0. The name must be a symbol (canonicalized).
  // If the slot is present and mode != NULL, sets *mode to the corresponding
  // mode for that variable.
  static int ContextSlotIndex(Code* code, String* name, Variable::Mode* mode);

  // Lookup support for scope info embedded in Code objects. Returns the
  // parameter index for a given parameter name if the parameter is present;
  // otherwise returns a value < 0. The name must be a symbol (canonicalized).
  static int ParameterIndex(Code* code, String* name);

  // Lookup support for scope info embedded in Code objects. Returns the
  // function context slot index if the function name is present (named
  // function expressions, only), otherwise returns a value < 0. The name
  // must be a symbol (canonicalized).
  static int FunctionContextSlotIndex(Code* code, String* name);

  // --------------------------------------------------------------------------
  // Debugging support

#ifdef DEBUG
  void Print();
#endif

 private:
  Handle<String> function_name_;
  bool calls_eval_;
  List<Handle<String>, Allocator > parameters_;
  List<Handle<String>, Allocator > stack_slots_;
  List<Handle<String>, Allocator > context_slots_;
  List<Variable::Mode, Allocator > context_modes_;
};

} }  // namespace v8::internal

#endif  // V8_SCOPEINFO_H_
