// Copyright 2009 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "codegen-inl.h"
#include "ic-inl.h"
#include "runtime.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {


void KeyedLoadIC::ClearInlinedVersion(Address address) {
  UNIMPLEMENTED();
}

void KeyedLoadIC::Generate(MacroAssembler* masm,
                           ExternalReference const& f) {
  masm->int3();  // UNIMPLEMENTED.
}

void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

bool KeyedLoadIC::PatchInlinedLoad(Address address, Object* map) {
  UNIMPLEMENTED();
  return false;
}

Object* KeyedLoadStubCompiler::CompileLoadArrayLength(String* name) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadCallback(String* name,
                                                   JSObject* object,
                                                   JSObject* holder,
                                                   AccessorInfo* callback) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadConstant(String* name,
                                                   JSObject* object,
                                                   JSObject* holder,
                                                   Object* callback) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadField(String* name,
                                                JSObject* object,
                                                JSObject* holder,
                                                int index) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadInterceptor(JSObject* object,
                                                      JSObject* holder,
                                                      String* name) {
  UNIMPLEMENTED();
  return NULL;
}

Object* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  UNIMPLEMENTED();
  return NULL;
}

void KeyedStoreIC::Generate(MacroAssembler* masm, ExternalReference const& f) {
  masm->int3();  // UNIMPLEMENTED.
}

void KeyedStoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

Object* KeyedStoreStubCompiler::CompileStoreField(JSObject* object,
                                                  int index,
                                                  Map* transition,
                                                  String* name) {
  UNIMPLEMENTED();
  return NULL;
}

void LoadIC::ClearInlinedVersion(Address address) {
  UNIMPLEMENTED();
}

void LoadIC::Generate(MacroAssembler* masm, ExternalReference const& f) {
  masm->int3();  // UNIMPLEMENTED.
}

void LoadIC::GenerateArrayLength(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void LoadIC::GenerateFunctionPrototype(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void LoadIC::GenerateMiss(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void LoadIC::GenerateNormal(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void LoadIC::GenerateStringLength(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

bool LoadIC::PatchInlinedLoad(Address address, Object* map, int index) {
  UNIMPLEMENTED();
  return false;
}

void StoreIC::Generate(MacroAssembler* masm, ExternalReference const& f) {
  masm->int3();  // UNIMPLEMENTED.
}

void StoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  masm->int3();  // UNIMPLEMENTED.
}

} }  // namespace v8::internal
