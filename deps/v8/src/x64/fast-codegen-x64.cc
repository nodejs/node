// Copyright 2010 the V8 project authors. All rights reserved.
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
#include "fast-codegen.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

Register FastCodeGenerator::accumulator0() { return rax; }
Register FastCodeGenerator::accumulator1() { return rdx; }
Register FastCodeGenerator::scratch0() { return rcx; }
Register FastCodeGenerator::scratch1() { return rdi; }
Register FastCodeGenerator::receiver_reg() { return rbx; }
Register FastCodeGenerator::context_reg() { return rsi; }


void FastCodeGenerator::EmitLoadReceiver() {
  // Offset 2 is due to return address and saved frame pointer.
  int index = 2 + scope()->num_parameters();
  __ movq(receiver_reg(), Operand(rbp, index * kPointerSize));
}


void FastCodeGenerator::EmitGlobalVariableLoad(Handle<Object> cell) {
  ASSERT(!destination().is(no_reg));
  ASSERT(cell->IsJSGlobalPropertyCell());

  __ Move(destination(), cell);
  __ movq(destination(),
          FieldOperand(destination(), JSGlobalPropertyCell::kValueOffset));
  if (FLAG_debug_code) {
    __ Cmp(destination(), Factory::the_hole_value());
    __ Check(not_equal, "DontDelete cells can't contain the hole");
  }

  // The loaded value is not known to be a smi.
  clear_as_smi(destination());
}


void FastCodeGenerator::EmitThisPropertyStore(Handle<String> name) {
  LookupResult lookup;
  info()->receiver()->Lookup(*name, &lookup);

  ASSERT(lookup.holder() == *info()->receiver());
  ASSERT(lookup.type() == FIELD);
  Handle<Map> map(Handle<HeapObject>::cast(info()->receiver())->map());
  int index = lookup.GetFieldIndex() - map->inobject_properties();
  int offset = index * kPointerSize;

  // We will emit the write barrier unless the stored value is statically
  // known to be a smi.
  bool needs_write_barrier = !is_smi(accumulator0());

  // Perform the store.  Negative offsets are inobject properties.
  if (offset < 0) {
    offset += map->instance_size();
    __ movq(FieldOperand(receiver_reg(), offset), accumulator0());
    if (needs_write_barrier) {
      // Preserve receiver from write barrier.
      __ movq(scratch0(), receiver_reg());
    }
  } else {
    offset += FixedArray::kHeaderSize;
    __ movq(scratch0(),
            FieldOperand(receiver_reg(), JSObject::kPropertiesOffset));
    __ movq(FieldOperand(scratch0(), offset), accumulator0());
  }

  if (needs_write_barrier) {
    if (destination().is(no_reg)) {
      // After RecordWrite accumulator0 is only accidently a smi, but it is
      // already marked as not known to be one.
      __ RecordWrite(scratch0(), offset, accumulator0(), scratch1());
    } else {
      // Copy the value to the other accumulator to preserve a copy from the
      // write barrier. One of the accumulators is available as a scratch
      // register.  Neither is a smi.
      __ movq(accumulator1(), accumulator0());
      clear_as_smi(accumulator1());
      Register value_scratch = other_accumulator(destination());
      __ RecordWrite(scratch0(), offset, value_scratch, scratch1());
    }
  } else if (destination().is(accumulator1())) {
    __ movq(accumulator1(), accumulator0());
    // Is a smi because we do not need the write barrier.
    set_as_smi(accumulator1());
  }
}


void FastCodeGenerator::EmitThisPropertyLoad(Handle<String> name) {
  ASSERT(!destination().is(no_reg));
  LookupResult lookup;
  info()->receiver()->Lookup(*name, &lookup);

  ASSERT(lookup.holder() == *info()->receiver());
  ASSERT(lookup.type() == FIELD);
  Handle<Map> map(Handle<HeapObject>::cast(info()->receiver())->map());
  int index = lookup.GetFieldIndex() - map->inobject_properties();
  int offset = index * kPointerSize;

  // Perform the load.  Negative offsets are inobject properties.
  if (offset < 0) {
    offset += map->instance_size();
    __ movq(destination(), FieldOperand(receiver_reg(), offset));
  } else {
    offset += FixedArray::kHeaderSize;
    __ movq(scratch0(),
            FieldOperand(receiver_reg(), JSObject::kPropertiesOffset));
    __ movq(destination(), FieldOperand(scratch0(), offset));
  }

  // The loaded value is not known to be a smi.
  clear_as_smi(destination());
}


void FastCodeGenerator::EmitBitOr() {
  if (is_smi(accumulator0()) && is_smi(accumulator1())) {
    // If both operands are known to be a smi then there is no need to check
    // the operands or result.
    if (destination().is(no_reg)) {
      __ or_(accumulator1(), accumulator0());
    } else {
      // Leave the result in the destination register.  Bitwise or is
      // commutative.
      __ or_(destination(), other_accumulator(destination()));
    }
  } else if (destination().is(no_reg)) {
    // Result is not needed but do not clobber the operands in case of
    // bailout.
    __ movq(scratch0(), accumulator1());
    __ or_(scratch0(), accumulator0());
    __ JumpIfNotSmi(scratch0(), bailout());
  } else {
    // Preserve the destination operand in a scratch register in case of
    // bailout.
    __ movq(scratch0(), destination());
    __ or_(destination(), other_accumulator(destination()));
    __ JumpIfNotSmi(destination(), bailout());
  }


  // If we didn't bailout, the result (in fact, both inputs too) is known to
  // be a smi.
  set_as_smi(accumulator0());
  set_as_smi(accumulator1());
}


void FastCodeGenerator::Generate(CompilationInfo* compilation_info) {
  ASSERT(info_ == NULL);
  info_ = compilation_info;

  // Save the caller's frame pointer and set up our own.
  Comment prologue_cmnt(masm(), ";; Prologue");
  __ push(rbp);
  __ movq(rbp, rsp);
  __ push(rsi);  // Context.
  __ push(rdi);  // Closure.
  // Note that we keep a live register reference to esi (context) at this
  // point.

  // Receiver (this) is allocated to a fixed register.
  if (info()->has_this_properties()) {
    Comment cmnt(masm(), ";; MapCheck(this)");
    if (FLAG_print_ir) {
      PrintF("MapCheck(this)\n");
    }
    ASSERT(info()->has_receiver() && info()->receiver()->IsHeapObject());
    Handle<HeapObject> object = Handle<HeapObject>::cast(info()->receiver());
    Handle<Map> map(object->map());
    EmitLoadReceiver();
    __ CheckMap(receiver_reg(), map, bailout(), false);
  }

  // If there is a global variable access check if the global object is the
  // same as at lazy-compilation time.
  if (info()->has_globals()) {
    Comment cmnt(masm(), ";; MapCheck(GLOBAL)");
    if (FLAG_print_ir) {
      PrintF("MapCheck(GLOBAL)\n");
    }
    ASSERT(info()->has_global_object());
    Handle<Map> map(info()->global_object()->map());
    __ movq(scratch0(), CodeGenerator::GlobalObject());
    __ CheckMap(scratch0(), map, bailout(), true);
  }

  VisitStatements(info()->function()->body());

  Comment return_cmnt(masm(), ";; Return(<undefined>)");
  if (FLAG_print_ir) {
    PrintF("Return(<undefined>)\n");
  }
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
  __ movq(rsp, rbp);
  __ pop(rbp);
  __ ret((scope()->num_parameters() + 1) * kPointerSize);

  __ bind(&bailout_);
}


#undef __


} }  // namespace v8::internal
