// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECT_LIST_MACROS_H_
#define V8_OBJECTS_OBJECT_LIST_MACROS_H_

namespace v8 {
namespace internal {

class AbstractCode;
class AccessorPair;
class AccessCheckInfo;
class AllocationSite;
class ByteArray;
class CachedTemplateObject;
class Cell;
class ClosureFeedbackCellArray;
class ConsString;
class DependentCode;
class ElementsAccessor;
class EnumCache;
class FixedArrayBase;
class FixedDoubleArray;
class FreeSpace;
class FunctionLiteral;
class FunctionTemplateInfo;
class JSAsyncGeneratorObject;
class JSGlobalProxy;
class SourceTextModule;
class JSPromise;
class JSProxy;
class JSProxyRevocableResult;
class KeyAccumulator;
class LayoutDescriptor;
class LookupIterator;
class FieldType;
class Module;
class SourceTextModuleInfoEntry;
class HeapNumber;
class ObjectHashTable;
class ObjectTemplateInfo;
class ObjectVisitor;
class OSROptimizedCodeCache;
class PreparseData;
class PropertyArray;
class PropertyCell;
class PropertyDescriptor;
class PrototypeInfo;
class ReadOnlyRoots;
class RegExpMatchInfo;
class RootVisitor;
class SafepointEntry;
class ScriptContextTable;
class SharedFunctionInfo;
class StringStream;
class Symbol;
class SyntheticModule;
class FeedbackCell;
class FeedbackMetadata;
class FeedbackVector;
class UncompiledData;
class TemplateInfo;
class TransitionArray;
class TemplateList;
class WasmInstanceObject;
class WasmMemoryObject;
template <typename T>
class ZoneForwardList;

#define OBJECT_TYPE_LIST(V) \
  V(LayoutDescriptor)       \
  V(Primitive)              \
  V(Number)                 \
  V(Numeric)

#define HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V) \
  V(AbstractCode)                              \
  V(AccessCheckNeeded)                         \
  V(AllocationSite)                            \
  V(ArrayList)                                 \
  V(BigInt)                                    \
  V(BigIntBase)                                \
  V(BigIntWrapper)                             \
  V(ObjectBoilerplateDescription)              \
  V(Boolean)                                   \
  V(BooleanWrapper)                            \
  V(ByteArray)                                 \
  V(BytecodeArray)                             \
  V(CallHandlerInfo)                           \
  V(Callable)                                  \
  V(Cell)                                      \
  V(ClassBoilerplate)                          \
  V(Code)                                      \
  V(CodeDataContainer)                         \
  V(CompilationCacheTable)                     \
  V(ConsString)                                \
  V(Constructor)                               \
  V(Context)                                   \
  V(CoverageInfo)                              \
  V(ClosureFeedbackCellArray)                  \
  V(DataHandler)                               \
  V(DeoptimizationData)                        \
  V(DependentCode)                             \
  V(DescriptorArray)                           \
  V(EmbedderDataArray)                         \
  V(EphemeronHashTable)                        \
  V(ExternalOneByteString)                     \
  V(ExternalString)                            \
  V(ExternalTwoByteString)                     \
  V(FeedbackCell)                              \
  V(FeedbackMetadata)                          \
  V(FeedbackVector)                            \
  V(Filler)                                    \
  V(FixedArray)                                \
  V(FixedArrayBase)                            \
  V(FixedArrayExact)                           \
  V(FixedDoubleArray)                          \
  V(Foreign)                                   \
  V(FrameArray)                                \
  V(FreeSpace)                                 \
  V(Function)                                  \
  V(GlobalDictionary)                          \
  V(HandlerTable)                              \
  V(HeapNumber)                                \
  V(InternalizedString)                        \
  V(JSArgumentsObject)                         \
  V(JSArray)                                   \
  V(JSArrayBuffer)                             \
  V(JSArrayBufferView)                         \
  V(JSArrayIterator)                           \
  V(JSAsyncFromSyncIterator)                   \
  V(JSAsyncFunctionObject)                     \
  V(JSAsyncGeneratorObject)                    \
  V(JSBoundFunction)                           \
  V(JSCollection)                              \
  V(JSCollectionIterator)                      \
  V(JSContextExtensionObject)                  \
  V(JSCustomElementsObject)                    \
  V(JSDataView)                                \
  V(JSDate)                                    \
  V(JSError)                                   \
  V(JSFinalizationGroup)                       \
  V(JSFinalizationGroupCleanupIterator)        \
  V(JSFunction)                                \
  V(JSFunctionOrBoundFunction)                 \
  V(JSGeneratorObject)                         \
  V(JSGlobalObject)                            \
  V(JSGlobalProxy)                             \
  V(JSMap)                                     \
  V(JSMapIterator)                             \
  V(JSMessageObject)                           \
  V(JSModuleNamespace)                         \
  V(JSObject)                                  \
  V(JSPrimitiveWrapper)                        \
  V(JSPromise)                                 \
  V(JSProxy)                                   \
  V(JSReceiver)                                \
  V(JSRegExp)                                  \
  V(JSRegExpStringIterator)                    \
  V(JSSet)                                     \
  V(JSSetIterator)                             \
  V(JSSpecialObject)                           \
  V(JSStringIterator)                          \
  V(JSTypedArray)                              \
  V(JSWeakCollection)                          \
  V(JSWeakRef)                                 \
  V(JSWeakMap)                                 \
  V(JSWeakSet)                                 \
  V(LoadHandler)                               \
  V(Map)                                       \
  V(MapCache)                                  \
  V(Module)                                    \
  V(Microtask)                                 \
  V(Name)                                      \
  V(NameDictionary)                            \
  V(NativeContext)                             \
  V(NormalizedMapCache)                        \
  V(NumberDictionary)                          \
  V(NumberWrapper)                             \
  V(ObjectHashSet)                             \
  V(ObjectHashTable)                           \
  V(Oddball)                                   \
  V(OrderedHashMap)                            \
  V(OrderedHashSet)                            \
  V(OrderedNameDictionary)                     \
  V(OSROptimizedCodeCache)                     \
  V(PreparseData)                              \
  V(PrimitiveHeapObject)                       \
  V(PromiseReactionJobTask)                    \
  V(PropertyArray)                             \
  V(PropertyCell)                              \
  V(RegExpMatchInfo)                           \
  V(ScopeInfo)                                 \
  V(ScriptContextTable)                        \
  V(ScriptWrapper)                             \
  V(SeqOneByteString)                          \
  V(SeqString)                                 \
  V(SeqTwoByteString)                          \
  V(SharedFunctionInfo)                        \
  V(SimpleNumberDictionary)                    \
  V(SlicedString)                              \
  V(SloppyArgumentsElements)                   \
  V(SmallOrderedHashMap)                       \
  V(SmallOrderedHashSet)                       \
  V(SmallOrderedNameDictionary)                \
  V(SourceTextModule)                          \
  V(SourceTextModuleInfo)                      \
  V(StoreHandler)                              \
  V(String)                                    \
  V(StringSet)                                 \
  V(StringTable)                               \
  V(StringWrapper)                             \
  V(Struct)                                    \
  V(Symbol)                                    \
  V(SymbolWrapper)                             \
  V(SyntheticModule)                           \
  V(TemplateInfo)                              \
  V(TemplateList)                              \
  V(ThinString)                                \
  V(TransitionArray)                           \
  V(UncompiledData)                            \
  V(UncompiledDataWithPreparseData)            \
  V(UncompiledDataWithoutPreparseData)         \
  V(Undetectable)                              \
  V(UniqueName)                                \
  V(WasmExceptionObject)                       \
  V(WasmExceptionPackage)                      \
  V(WasmGlobalObject)                          \
  V(WasmInstanceObject)                        \
  V(WasmMemoryObject)                          \
  V(WasmModuleObject)                          \
  V(WasmTableObject)                           \
  V(WeakFixedArray)                            \
  V(WeakArrayList)                             \
  V(WeakCell)

#ifdef V8_INTL_SUPPORT
#define HEAP_OBJECT_ORDINARY_TYPE_LIST(V) \
  HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V)  \
  V(JSV8BreakIterator)                    \
  V(JSCollator)                           \
  V(JSDateTimeFormat)                     \
  V(JSDisplayNames)                       \
  V(JSListFormat)                         \
  V(JSLocale)                             \
  V(JSNumberFormat)                       \
  V(JSPluralRules)                        \
  V(JSRelativeTimeFormat)                 \
  V(JSSegmentIterator)                    \
  V(JSSegmenter)
#else
#define HEAP_OBJECT_ORDINARY_TYPE_LIST(V) HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V)
#endif  // V8_INTL_SUPPORT

#define HEAP_OBJECT_TEMPLATE_TYPE_LIST(V) V(HashTable)

// Logical sub-types of heap objects that don't correspond to a C++ class but
// represent some specialization in terms of additional constraints.
#define HEAP_OBJECT_SPECIALIZED_TYPE_LIST(V) \
  V(AwaitContext)                            \
  V(BlockContext)                            \
  V(CallableApiObject)                       \
  V(CallableJSProxy)                         \
  V(CatchContext)                            \
  V(DebugEvaluateContext)                    \
  V(EvalContext)                             \
  V(FreeSpaceOrFiller)                       \
  V(FunctionContext)                         \
  V(JSApiObject)                             \
  V(JSMapKeyIterator)                        \
  V(JSMapKeyValueIterator)                   \
  V(JSMapValueIterator)                      \
  V(JSSetKeyValueIterator)                   \
  V(JSSetValueIterator)                      \
  V(JSSpecialApiObject)                      \
  V(ModuleContext)                           \
  V(NonNullForeign)                          \
  V(ScriptContext)                           \
  V(WithContext)

#define HEAP_OBJECT_TYPE_LIST(V)    \
  HEAP_OBJECT_ORDINARY_TYPE_LIST(V) \
  HEAP_OBJECT_TEMPLATE_TYPE_LIST(V) \
  HEAP_OBJECT_SPECIALIZED_TYPE_LIST(V)

#define ODDBALL_LIST(V)                 \
  V(Undefined, undefined_value)         \
  V(Null, null_value)                   \
  V(TheHole, the_hole_value)            \
  V(Exception, exception)               \
  V(Uninitialized, uninitialized_value) \
  V(True, true_value)                   \
  V(False, false_value)                 \
  V(ArgumentsMarker, arguments_marker)  \
  V(OptimizedOut, optimized_out)        \
  V(StaleRegister, stale_register)

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_OBJECT_LIST_MACROS_H_
