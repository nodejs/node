// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_FUNCTION_BODY_DECODER_H_
#define V8_WASM_FUNCTION_BODY_DECODER_H_

#include <iterator>

#include "src/base/compiler-specific.h"
#include "src/base/iterator.h"
#include "src/globals.h"
#include "src/signature.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {

class BitVector;  // forward declaration

namespace compiler {  // external declarations from compiler.
class WasmGraphBuilder;
}

namespace wasm {

typedef compiler::WasmGraphBuilder TFBuilder;
struct WasmModule;  // forward declaration of module interface.

// A wrapper around the signature and bytes of a function.
struct FunctionBody {
  FunctionSig* sig;   // function signature
  const byte* base;   // base of the module bytes, for error reporting
  const byte* start;  // start of the function body
  const byte* end;    // end of the function body
};

static inline FunctionBody FunctionBodyForTesting(const byte* start,
                                                  const byte* end) {
  return {nullptr, start, start, end};
}

struct DecodeStruct {
  int unused;
};
typedef Result<DecodeStruct*> DecodeResult;
inline std::ostream& operator<<(std::ostream& os, const DecodeStruct& tree) {
  return os;
}

V8_EXPORT_PRIVATE DecodeResult VerifyWasmCode(AccountingAllocator* allocator,
                                              const wasm::WasmModule* module,
                                              FunctionBody& body);
DecodeResult BuildTFGraph(AccountingAllocator* allocator, TFBuilder* builder,
                          FunctionBody& body);
bool PrintRawWasmCode(AccountingAllocator* allocator, const FunctionBody& body,
                      const wasm::WasmModule* module);

// A simplified form of AST printing, e.g. from a debugger.
void PrintRawWasmCode(const byte* start, const byte* end);

inline DecodeResult VerifyWasmCode(AccountingAllocator* allocator,
                                   const WasmModule* module, FunctionSig* sig,
                                   const byte* start, const byte* end) {
  FunctionBody body = {sig, nullptr, start, end};
  return VerifyWasmCode(allocator, module, body);
}

inline DecodeResult BuildTFGraph(AccountingAllocator* allocator,
                                 TFBuilder* builder, FunctionSig* sig,
                                 const byte* start, const byte* end) {
  FunctionBody body = {sig, nullptr, start, end};
  return BuildTFGraph(allocator, builder, body);
}

struct BodyLocalDecls {
  // The size of the encoded declarations.
  uint32_t encoded_size;  // size of encoded declarations

  ZoneVector<ValueType> type_list;

  explicit BodyLocalDecls(Zone* zone) : encoded_size(0), type_list(zone) {}
};

V8_EXPORT_PRIVATE bool DecodeLocalDecls(BodyLocalDecls* decls,
                                        const byte* start, const byte* end);

V8_EXPORT_PRIVATE BitVector* AnalyzeLoopAssignmentForTesting(Zone* zone,
                                                             size_t num_locals,
                                                             const byte* start,
                                                             const byte* end);

// Computes the length of the opcode at the given address.
V8_EXPORT_PRIVATE unsigned OpcodeLength(const byte* pc, const byte* end);

// Computes the stack effect of the opcode at the given address.
// Returns <pop count, push count>.
// Be cautious with control opcodes: This function only covers their immediate,
// local stack effect (e.g. BrIf pops 1, Br pops 0). Those opcodes can have
// non-local stack effect though, which are not covered here.
std::pair<uint32_t, uint32_t> StackEffect(const WasmModule* module,
                                          FunctionSig* sig, const byte* pc,
                                          const byte* end);

// A simple forward iterator for bytecodes.
class V8_EXPORT_PRIVATE BytecodeIterator : public NON_EXPORTED_BASE(Decoder) {
  // Base class for both iterators defined below.
  class iterator_base {
   public:
    inline iterator_base& operator++() {
      DCHECK_LT(ptr_, end_);
      ptr_ += OpcodeLength(ptr_, end_);
      return *this;
    }
    inline bool operator==(const iterator_base& that) {
      return this->ptr_ == that.ptr_;
    }
    inline bool operator!=(const iterator_base& that) {
      return this->ptr_ != that.ptr_;
    }

   protected:
    const byte* ptr_;
    const byte* end_;
    iterator_base(const byte* ptr, const byte* end) : ptr_(ptr), end_(end) {}
  };

 public:
  // If one wants to iterate over the bytecode without looking at {pc_offset()}.
  class opcode_iterator
      : public iterator_base,
        public std::iterator<std::input_iterator_tag, WasmOpcode> {
   public:
    inline WasmOpcode operator*() {
      DCHECK_LT(ptr_, end_);
      return static_cast<WasmOpcode>(*ptr_);
    }

   private:
    friend class BytecodeIterator;
    opcode_iterator(const byte* ptr, const byte* end)
        : iterator_base(ptr, end) {}
  };
  // If one wants to iterate over the instruction offsets without looking at
  // opcodes.
  class offset_iterator
      : public iterator_base,
        public std::iterator<std::input_iterator_tag, uint32_t> {
   public:
    inline uint32_t operator*() {
      DCHECK_LT(ptr_, end_);
      return static_cast<uint32_t>(ptr_ - start_);
    }

   private:
    const byte* start_;
    friend class BytecodeIterator;
    offset_iterator(const byte* start, const byte* ptr, const byte* end)
        : iterator_base(ptr, end), start_(start) {}
  };

  // Create a new {BytecodeIterator}. If the {decls} pointer is non-null,
  // assume the bytecode starts with local declarations and decode them.
  // Otherwise, do not decode local decls.
  BytecodeIterator(const byte* start, const byte* end,
                   BodyLocalDecls* decls = nullptr);

  base::iterator_range<opcode_iterator> opcodes() {
    return base::iterator_range<opcode_iterator>(opcode_iterator(pc_, end_),
                                                 opcode_iterator(end_, end_));
  }

  base::iterator_range<offset_iterator> offsets() {
    return base::iterator_range<offset_iterator>(
        offset_iterator(start_, pc_, end_),
        offset_iterator(start_, end_, end_));
  }

  WasmOpcode current() {
    return static_cast<WasmOpcode>(read_u8<false>(pc_, "expected bytecode"));
  }

  void next() {
    if (pc_ < end_) {
      pc_ += OpcodeLength(pc_, end_);
      if (pc_ >= end_) pc_ = end_;
    }
  }

  bool has_next() { return pc_ < end_; }
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_FUNCTION_BODY_DECODER_H_
