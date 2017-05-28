// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_BUILTIN_REDUCER_H_
#define V8_COMPILER_JS_BUILTIN_REDUCER_H_

#include "src/base/compiler-specific.h"
#include "src/base/flags.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;
class Factory;

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
struct FieldAccess;
class JSGraph;
class JSOperatorBuilder;
class SimplifiedOperatorBuilder;
class TypeCache;

class V8_EXPORT_PRIVATE JSBuiltinReducer final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  // Flags that control the mode of operation.
  enum Flag {
    kNoFlags = 0u,
    kDeoptimizationEnabled = 1u << 0,
  };
  typedef base::Flags<Flag> Flags;

  JSBuiltinReducer(Editor* editor, JSGraph* jsgraph, Flags flags,
                   CompilationDependencies* dependencies,
                   Handle<Context> native_context);
  ~JSBuiltinReducer() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceArrayIterator(Node* node, IterationKind kind);
  Reduction ReduceTypedArrayIterator(Node* node, IterationKind kind);
  Reduction ReduceArrayIterator(Handle<Map> receiver_map, Node* node,
                                IterationKind kind,
                                ArrayIteratorKind iter_kind);
  Reduction ReduceArrayIteratorNext(Node* node);
  Reduction ReduceFastArrayIteratorNext(Handle<Map> iterator_map, Node* node,
                                        IterationKind kind);
  Reduction ReduceTypedArrayIteratorNext(Handle<Map> iterator_map, Node* node,
                                         IterationKind kind);
  Reduction ReduceArrayIsArray(Node* node);
  Reduction ReduceArrayPop(Node* node);
  Reduction ReduceArrayPush(Node* node);
  Reduction ReduceArrayShift(Node* node);
  Reduction ReduceDateNow(Node* node);
  Reduction ReduceDateGetTime(Node* node);
  Reduction ReduceGlobalIsFinite(Node* node);
  Reduction ReduceGlobalIsNaN(Node* node);
  Reduction ReduceMathAbs(Node* node);
  Reduction ReduceMathAcos(Node* node);
  Reduction ReduceMathAcosh(Node* node);
  Reduction ReduceMathAsin(Node* node);
  Reduction ReduceMathAsinh(Node* node);
  Reduction ReduceMathAtan(Node* node);
  Reduction ReduceMathAtanh(Node* node);
  Reduction ReduceMathAtan2(Node* node);
  Reduction ReduceMathCbrt(Node* node);
  Reduction ReduceMathCeil(Node* node);
  Reduction ReduceMathClz32(Node* node);
  Reduction ReduceMathCos(Node* node);
  Reduction ReduceMathCosh(Node* node);
  Reduction ReduceMathExp(Node* node);
  Reduction ReduceMathExpm1(Node* node);
  Reduction ReduceMathFloor(Node* node);
  Reduction ReduceMathFround(Node* node);
  Reduction ReduceMathImul(Node* node);
  Reduction ReduceMathLog(Node* node);
  Reduction ReduceMathLog1p(Node* node);
  Reduction ReduceMathLog10(Node* node);
  Reduction ReduceMathLog2(Node* node);
  Reduction ReduceMathMax(Node* node);
  Reduction ReduceMathMin(Node* node);
  Reduction ReduceMathPow(Node* node);
  Reduction ReduceMathRound(Node* node);
  Reduction ReduceMathSign(Node* node);
  Reduction ReduceMathSin(Node* node);
  Reduction ReduceMathSinh(Node* node);
  Reduction ReduceMathSqrt(Node* node);
  Reduction ReduceMathTan(Node* node);
  Reduction ReduceMathTanh(Node* node);
  Reduction ReduceMathTrunc(Node* node);
  Reduction ReduceNumberIsFinite(Node* node);
  Reduction ReduceNumberIsInteger(Node* node);
  Reduction ReduceNumberIsNaN(Node* node);
  Reduction ReduceNumberIsSafeInteger(Node* node);
  Reduction ReduceNumberParseInt(Node* node);
  Reduction ReduceObjectCreate(Node* node);
  Reduction ReduceStringCharAt(Node* node);
  Reduction ReduceStringCharCodeAt(Node* node);
  Reduction ReduceStringConcat(Node* node);
  Reduction ReduceStringFromCharCode(Node* node);
  Reduction ReduceStringIndexOf(Node* node);
  Reduction ReduceStringIterator(Node* node);
  Reduction ReduceStringIteratorNext(Node* node);
  Reduction ReduceArrayBufferViewAccessor(Node* node,
                                          InstanceType instance_type,
                                          FieldAccess const& access);

  Node* ToNumber(Node* value);
  Node* ToUint32(Node* value);

  Flags flags() const { return flags_; }
  Graph* graph() const;
  Factory* factory() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  Handle<Context> native_context() const { return native_context_; }
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;
  JSOperatorBuilder* javascript() const;
  CompilationDependencies* dependencies() const { return dependencies_; }

  CompilationDependencies* const dependencies_;
  Flags const flags_;
  JSGraph* const jsgraph_;
  Handle<Context> const native_context_;
  TypeCache const& type_cache_;
};

DEFINE_OPERATORS_FOR_FLAGS(JSBuiltinReducer::Flags)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_BUILTIN_REDUCER_H_
