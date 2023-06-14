// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/bytecode-array.h"

#include <iomanip>

#include "src/codegen/handler-table.h"
#include "src/codegen/source-position-table.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-decoder.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

void BytecodeArray::PrintJson(std::ostream& os) {
  DisallowGarbageCollection no_gc;

  Address base_address = GetFirstBytecodeAddress();
  BytecodeArray handle_storage = *this;
  Handle<BytecodeArray> handle(reinterpret_cast<Address*>(&handle_storage));
  interpreter::BytecodeArrayIterator iterator(handle);
  bool first_data = true;

  os << "{\"data\": [";

  while (!iterator.done()) {
    if (!first_data) os << ", ";
    Address current_address = base_address + iterator.current_offset();
    first_data = false;

    os << "{\"offset\":" << iterator.current_offset() << ", \"disassembly\":\"";
    interpreter::BytecodeDecoder::Decode(
        os, reinterpret_cast<uint8_t*>(current_address), false);

    if (interpreter::Bytecodes::IsJump(iterator.current_bytecode())) {
      os << " (" << iterator.GetJumpTargetOffset() << ")";
    }

    if (interpreter::Bytecodes::IsSwitch(iterator.current_bytecode())) {
      os << " {";
      bool first_entry = true;
      for (interpreter::JumpTableTargetOffset entry :
           iterator.GetJumpTableTargetOffsets()) {
        if (!first_entry) os << ", ";
        first_entry = false;
        os << entry.target_offset;
      }
      os << "}";
    }

    os << "\"}";
    iterator.Advance();
  }

  os << "]";

  int constant_pool_lenght = constant_pool().length();
  if (constant_pool_lenght > 0) {
    os << ", \"constantPool\": [";
    for (int i = 0; i < constant_pool_lenght; i++) {
      Object object = constant_pool().get(i);
      if (i > 0) os << ", ";
      os << "\"" << object << "\"";
    }
    os << "]";
  }

  os << "}";
}

void BytecodeArray::Disassemble(std::ostream& os) {
  DisallowGarbageCollection no_gc;
  // Storage for backing the handle passed to the iterator. This handle won't be
  // updated by the gc, but that's ok because we've disallowed GCs anyway.
  BytecodeArray handle_storage = *this;
  Handle<BytecodeArray> handle(reinterpret_cast<Address*>(&handle_storage));
  Disassemble(handle, os);
}

// static
void BytecodeArray::Disassemble(Handle<BytecodeArray> handle,
                                std::ostream& os) {
  DisallowGarbageCollection no_gc;

  os << "Parameter count " << handle->parameter_count() << "\n";
  os << "Register count " << handle->register_count() << "\n";
  os << "Frame size " << handle->frame_size() << "\n";
  os << "Bytecode age: " << handle->bytecode_age() << "\n";

  Address base_address = handle->GetFirstBytecodeAddress();
  SourcePositionTableIterator source_positions(handle->SourcePositionTable());

  interpreter::BytecodeArrayIterator iterator(handle);
  while (!iterator.done()) {
    if (!source_positions.done() &&
        iterator.current_offset() == source_positions.code_offset()) {
      os << std::setw(5) << source_positions.source_position().ScriptOffset();
      os << (source_positions.is_statement() ? " S> " : " E> ");
      source_positions.Advance();
    } else {
      os << "         ";
    }
    Address current_address = base_address + iterator.current_offset();
    os << reinterpret_cast<const void*>(current_address) << " @ "
       << std::setw(4) << iterator.current_offset() << " : ";
    interpreter::BytecodeDecoder::Decode(
        os, reinterpret_cast<uint8_t*>(current_address));
    if (interpreter::Bytecodes::IsJump(iterator.current_bytecode())) {
      Address jump_target = base_address + iterator.GetJumpTargetOffset();
      os << " (" << reinterpret_cast<void*>(jump_target) << " @ "
         << iterator.GetJumpTargetOffset() << ")";
    }
    if (interpreter::Bytecodes::IsSwitch(iterator.current_bytecode())) {
      os << " {";
      bool first_entry = true;
      for (interpreter::JumpTableTargetOffset entry :
           iterator.GetJumpTableTargetOffsets()) {
        if (first_entry) {
          first_entry = false;
        } else {
          os << ",";
        }
        os << " " << entry.case_value << ": @" << entry.target_offset;
      }
      os << " }";
    }
    os << std::endl;
    iterator.Advance();
  }

  os << "Constant pool (size = " << handle->constant_pool().length() << ")\n";
#ifdef OBJECT_PRINT
  if (handle->constant_pool().length() > 0) {
    handle->constant_pool().Print(os);
  }
#endif

  os << "Handler Table (size = " << handle->handler_table().length() << ")\n";
#ifdef ENABLE_DISASSEMBLER
  if (handle->handler_table().length() > 0) {
    HandlerTable table(*handle);
    table.HandlerTableRangePrint(os);
  }
#endif

  ByteArray source_position_table = handle->SourcePositionTable();
  os << "Source Position Table (size = " << source_position_table.length()
     << ")\n";
#ifdef OBJECT_PRINT
  if (source_position_table.length() > 0) {
    os << Brief(source_position_table) << std::endl;
  }
#endif
}

void BytecodeArray::CopyBytecodesTo(BytecodeArray to) {
  BytecodeArray from = *this;
  DCHECK_EQ(from.length(), to.length());
  CopyBytes(reinterpret_cast<uint8_t*>(to.GetFirstBytecodeAddress()),
            reinterpret_cast<uint8_t*>(from.GetFirstBytecodeAddress()),
            from.length());
}

void BytecodeArray::MakeOlder(uint16_t increment) {
  if (v8_flags.flush_code_based_on_time) {
    DCHECK_NE(increment, 0);
    uint16_t current_age;
    uint16_t updated_age;

    do {
      current_age = bytecode_age();
      // When the age is 0, it was reset by the function prologue in
      // Ignition/Sparkplug. But that might have been some time after the last
      // full GC. So in this case we don't increment the value like we normally
      // would but just set the age to 1. All non-0 values can be incremented as
      // expected (we add the number of seconds since the last GC) as they were
      // definitely last executed before the last full GC.
      updated_age = current_age == 0 ? 1 : SaturateAdd(current_age, increment);
    } while (CompareExchangeBytecodeAge(current_age, updated_age) !=
             current_age);
  } else {
    uint16_t age = bytecode_age();
    if (age < v8_flags.bytecode_old_age) {
      CompareExchangeBytecodeAge(age, age + 1);
    }
    DCHECK_LE(bytecode_age(), v8_flags.bytecode_old_age);
  }
}

void BytecodeArray::EnsureOldForTesting() {
  uint16_t old_age = v8_flags.flush_code_based_on_time
                         ? UINT16_MAX
                         : v8_flags.bytecode_old_age;
  set_bytecode_age(old_age);
  DCHECK(IsOld());
}

bool BytecodeArray::IsOld() const {
  if (v8_flags.flush_code_based_on_time) {
    return bytecode_age() >= v8_flags.bytecode_old_time;
  } else {
    return bytecode_age() >= v8_flags.bytecode_old_age;
  }
}

}  // namespace internal
}  // namespace v8
