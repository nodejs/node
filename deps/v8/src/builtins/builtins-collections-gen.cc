// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/code-stub-assembler.h"
#include "src/factory-inl.h"
#include "src/objects/hash-table.h"

namespace v8 {
namespace internal {

using compiler::Node;
template <class T>
using TNode = compiler::TNode<T>;
template <class T>
using TVariable = compiler::TypedCodeAssemblerVariable<T>;

class BaseCollectionsAssembler : public CodeStubAssembler {
 public:
  explicit BaseCollectionsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  virtual ~BaseCollectionsAssembler() {}

 protected:
  enum Variant { kMap, kSet };

  // Adds an entry to a collection.  For Maps, properly handles extracting the
  // key and value from the entry (see LoadKeyValue()).
  TNode<Object> AddConstructorEntry(Variant variant, TNode<Context> context,
                                    TNode<Object> collection,
                                    TNode<Object> add_function,
                                    TNode<Object> key_value,
                                    Label* if_exception = nullptr,
                                    TVariable<Object>* var_exception = nullptr);

  // Adds constructor entries to a collection.  Choosing a fast path when
  // possible.
  void AddConstructorEntries(Variant variant, TNode<Context> context,
                             TNode<Context> native_context,
                             TNode<Object> collection,
                             TNode<Object> initial_entries,
                             TNode<BoolT> is_fast_jsarray);

  // Fast path for adding constructor entries.  Assumes the entries are a fast
  // JS array (see CodeStubAssembler::BranchIfFastJSArray()).
  void AddConstructorEntriesFromFastJSArray(Variant variant,
                                            TNode<Context> context,
                                            TNode<Object> collection,
                                            TNode<JSArray> fast_jsarray);

  // Adds constructor entries to a collection using the iterator protocol.
  void AddConstructorEntriesFromIterable(Variant variant,
                                         TNode<Context> context,
                                         TNode<Context> native_context,
                                         TNode<Object> collection,
                                         TNode<Object> iterable);

  // Constructs a collection instance. Choosing a fast path when possible.
  TNode<Object> AllocateJSCollection(TNode<Context> context,
                                     TNode<Context> native_context,
                                     int constructor_function_index,
                                     TNode<Object> new_target);

  // Fast path for constructing a collection instance if the constructor
  // function has not been modified.
  TNode<Object> AllocateJSCollectionFast(TNode<HeapObject> constructor);

  // Fallback for constructing a collection instance if the constructor function
  // has been modified.
  TNode<Object> AllocateJSCollectionSlow(TNode<Context> context,
                                         TNode<HeapObject> constructor,
                                         TNode<Object> new_target);

  // Allocates the backing store for a collection.
  virtual TNode<Object> AllocateTable(Variant variant, TNode<Context> context,
                                      TNode<IntPtrT> at_least_space_for) = 0;

  // Main entry point for a collection constructor builtin.
  void GenerateConstructor(Variant variant,
                           const int constructor_function_index,
                           Handle<String> constructor_function_name,
                           int collection_tableoffset);

  // Retrieves the collection function that adds an entry. `set` for Maps and
  // `add` for Sets.
  TNode<Object> GetAddFunction(Variant variant, TNode<Context> context,
                               TNode<Object> collection);

  // Estimates the number of entries the collection will have after adding the
  // entries passed in the constructor. AllocateTable() can use this to avoid
  // the time of growing/rehashing when adding the constructor entries.
  TNode<IntPtrT> EstimatedInitialSize(TNode<Object> initial_entries,
                                      TNode<BoolT> is_fast_jsarray);

  void GotoIfNotJSReceiver(Node* const obj, Label* if_not_receiver);

  // Loads an element from a fixed array.  If the element is the hole, returns
  // `undefined`.
  TNode<Object> LoadAndNormalizeFixedArrayElement(TNode<Object> elements,
                                                  TNode<IntPtrT> index);

  // Loads an element from a fixed double array.  If the element is the hole,
  // returns `undefined`.
  TNode<Object> LoadAndNormalizeFixedDoubleArrayElement(TNode<Object> elements,
                                                        TNode<IntPtrT> index);

  // Loads key and value variables with the first and second elements of an
  // array.  If the array lacks 2 elements, undefined is used.
  void LoadKeyValue(TNode<Context> context, TNode<Object> maybe_array,
                    TVariable<Object>* key, TVariable<Object>* value,
                    Label* if_exception = nullptr,
                    TVariable<Object>* var_exception = nullptr);
};

TNode<Object> BaseCollectionsAssembler::AddConstructorEntry(
    Variant variant, TNode<Context> context, TNode<Object> collection,
    TNode<Object> add_function, TNode<Object> key_value, Label* if_exception,
    TVariable<Object>* var_exception) {
  CSA_ASSERT(this, Word32BinaryNot(IsTheHole(key_value)));
  if (variant == kMap) {
    Label exit(this), if_notobject(this, Label::kDeferred);
    GotoIfNotJSReceiver(key_value, &if_notobject);

    TVARIABLE(Object, key);
    TVARIABLE(Object, value);
    LoadKeyValue(context, key_value, &key, &value, if_exception, var_exception);
    Node* key_n = key;
    Node* value_n = value;
    TNode<Object> add_call =
        UncheckedCast<Object>(CallJS(CodeFactory::Call(isolate()), context,
                                     add_function, collection, key_n, value_n));
    Goto(&exit);

    BIND(&if_notobject);
    {
      Node* ret = CallRuntime(
          Runtime::kThrowTypeError, context,
          SmiConstant(MessageTemplate::kIteratorValueNotAnObject), key_value);
      if (if_exception != nullptr) {
        DCHECK(var_exception != nullptr);
        GotoIfException(ret, if_exception, var_exception);
      }
      Unreachable();
    }
    BIND(&exit);
    return add_call;

  } else {  // variant == kSet
    DCHECK(variant == kSet);
    return UncheckedCast<Object>(CallJS(CodeFactory::Call(isolate()), context,
                                        add_function, collection, key_value));
  }
}

void BaseCollectionsAssembler::AddConstructorEntries(
    Variant variant, TNode<Context> context, TNode<Context> native_context,
    TNode<Object> collection, TNode<Object> initial_entries,
    TNode<BoolT> is_fast_jsarray) {
  Label exit(this), slow_loop(this, Label::kDeferred);
  GotoIf(IsNullOrUndefined(initial_entries), &exit);

  // TODO(mvstanton): Re-enable the fast path when a fix is found for
  // crbug.com/798026.
  {
    AddConstructorEntriesFromIterable(variant, context, native_context,
                                      collection, initial_entries);
    Goto(&exit);
  }
  BIND(&exit);
}

void BaseCollectionsAssembler::AddConstructorEntriesFromFastJSArray(
    Variant variant, TNode<Context> context, TNode<Object> collection,
    TNode<JSArray> fast_jsarray) {
  TNode<FixedArrayBase> elements = LoadElements(fast_jsarray);
  TNode<Int32T> elements_kind = LoadMapElementsKind(LoadMap(fast_jsarray));
  TNode<IntPtrT> length = SmiUntag(LoadFastJSArrayLength(fast_jsarray));
  TNode<Object> add_func = GetAddFunction(variant, context, collection);

  CSA_ASSERT(this, IsFastJSArray(fast_jsarray, context));
  CSA_ASSERT(this, IsFastElementsKind(elements_kind));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(length, IntPtrConstant(0)));

  Label exit(this), if_doubles(this), if_smiorobjects(this);
  Branch(IsFastSmiOrTaggedElementsKind(elements_kind), &if_smiorobjects,
         &if_doubles);
  BIND(&if_smiorobjects);
  {
    auto set_entry = [&](Node* index) {
      TNode<Object> element = LoadAndNormalizeFixedArrayElement(
          elements, UncheckedCast<IntPtrT>(index));
      AddConstructorEntry(variant, context, collection, add_func, element);
    };
    BuildFastLoop(IntPtrConstant(0), length, set_entry, 1,
                  ParameterMode::INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
    Goto(&exit);
  }
  BIND(&if_doubles);
  {
    // A Map constructor requires entries to be arrays (ex. [key, value]),
    // so a FixedDoubleArray can never succeed.
    if (variant == kMap) {
      TNode<Float64T> element =
          UncheckedCast<Float64T>(LoadFixedDoubleArrayElement(
              elements, IntPtrConstant(0), MachineType::Float64(), 0,
              INTPTR_PARAMETERS));
      ThrowTypeError(context, MessageTemplate::kIteratorValueNotAnObject,
                     AllocateHeapNumberWithValue(element));
    } else {
      auto set_entry = [&](Node* index) {
        TNode<Object> entry = LoadAndNormalizeFixedDoubleArrayElement(
            elements, UncheckedCast<IntPtrT>(index));
        AddConstructorEntry(kSet, context, collection, add_func, entry);
      };
      BuildFastLoop(IntPtrConstant(0), length, set_entry, 1,
                    ParameterMode::INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
      Goto(&exit);
    }
  }
  BIND(&exit);
}

void BaseCollectionsAssembler::AddConstructorEntriesFromIterable(
    Variant variant, TNode<Context> context, TNode<Context> native_context,
    TNode<Object> collection, TNode<Object> iterable) {
  Label exit(this), loop(this), if_exception(this, Label::kDeferred);
  CSA_ASSERT(this, Word32BinaryNot(IsNullOrUndefined(iterable)));

  TNode<Object> add_func = GetAddFunction(variant, context, collection);
  IteratorBuiltinsAssembler iterator_assembler(this->state());
  IteratorRecord iterator = iterator_assembler.GetIterator(context, iterable);

  CSA_ASSERT(this, Word32BinaryNot(IsUndefined(iterator.object)));

  TNode<Object> fast_iterator_result_map =
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
  TVARIABLE(Object, var_exception);

  Goto(&loop);
  BIND(&loop);
  {
    TNode<Object> next = CAST(iterator_assembler.IteratorStep(
        context, iterator, &exit, fast_iterator_result_map));
    TNode<Object> next_value = CAST(iterator_assembler.IteratorValue(
        context, next, fast_iterator_result_map));
    TNode<Object> add_result =
        AddConstructorEntry(variant, context, collection, add_func, next_value,
                            &if_exception, &var_exception);
    GotoIfException(add_result, &if_exception, &var_exception);
    Goto(&loop);
  }
  BIND(&if_exception);
  {
    iterator_assembler.IteratorCloseOnException(context, iterator,
                                                &var_exception);
  }
  BIND(&exit);
}

TNode<Object> BaseCollectionsAssembler::AllocateJSCollection(
    TNode<Context> context, TNode<Context> native_context,
    int constructor_function_index, TNode<Object> new_target) {
  TNode<HeapObject> constructor =
      CAST(LoadContextElement(native_context, constructor_function_index));
  TNode<BoolT> is_target_unmodified = WordEqual(constructor, new_target);

  return Select<Object>(is_target_unmodified,
                        [=] { return AllocateJSCollectionFast(constructor); },
                        [=] {
                          return AllocateJSCollectionSlow(context, constructor,
                                                          new_target);
                        },
                        MachineRepresentation::kTagged);
}

TNode<Object> BaseCollectionsAssembler::AllocateJSCollectionFast(
    TNode<HeapObject> constructor) {
  CSA_ASSERT(this, IsConstructorMap(LoadMap(constructor)));
  TNode<Object> initial_map =
      LoadObjectField(constructor, JSFunction::kPrototypeOrInitialMapOffset);
  return CAST(AllocateJSObjectFromMap(initial_map));
}

TNode<Object> BaseCollectionsAssembler::AllocateJSCollectionSlow(
    TNode<Context> context, TNode<HeapObject> constructor,
    TNode<Object> new_target) {
  ConstructorBuiltinsAssembler constructor_assembler(this->state());
  return CAST(constructor_assembler.EmitFastNewObject(context, constructor,
                                                      new_target));
}

void BaseCollectionsAssembler::GenerateConstructor(
    Variant variant, const int constructor_function_index,
    Handle<String> constructor_function_name, int collection_tableoffset) {
  const int kIterableArg = 0;
  CodeStubArguments args(
      this, ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount)));
  TNode<Object> iterable = args.GetOptionalArgumentValue(kIterableArg);
  TNode<Object> new_target = CAST(Parameter(BuiltinDescriptor::kNewTarget));
  TNode<Context> context = CAST(Parameter(BuiltinDescriptor::kContext));

  Label if_undefined(this, Label::kDeferred);
  GotoIf(IsUndefined(new_target), &if_undefined);

  TNode<BoolT> is_fast_jsarray = IsFastJSArray(iterable, context);
  TNode<IntPtrT> at_least_space_for =
      EstimatedInitialSize(iterable, is_fast_jsarray);
  TNode<Context> native_context = LoadNativeContext(context);
  TNode<Object> collection = AllocateJSCollection(
      context, native_context, constructor_function_index, new_target);
  TNode<Object> table = AllocateTable(variant, context, at_least_space_for);

  StoreObjectField(collection, collection_tableoffset, table);
  AddConstructorEntries(variant, context, native_context, collection, iterable,
                        is_fast_jsarray);
  Return(collection);

  BIND(&if_undefined);
  ThrowTypeError(context, MessageTemplate::kConstructorNotFunction,
                 HeapConstant(constructor_function_name));
}

TNode<Object> BaseCollectionsAssembler::GetAddFunction(
    Variant variant, TNode<Context> context, TNode<Object> collection) {
  // TODO(pwong): Consider calling the builtin directly when the prototype is
  // unmodified.  This will require tracking WeakMap/WeakSet prototypes on the
  // native context.
  Handle<String> add_func_name = variant == kMap
                                     ? isolate()->factory()->set_string()
                                     : isolate()->factory()->add_string();
  TNode<Object> add_func =
      CAST(GetProperty(context, collection, add_func_name));

  Label exit(this), if_notcallable(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(add_func), &if_notcallable);
  GotoIfNot(IsCallable(add_func), &if_notcallable);
  Goto(&exit);

  BIND(&if_notcallable);
  ThrowTypeError(context, MessageTemplate::kPropertyNotFunction, add_func,
                 HeapConstant(add_func_name), collection);

  BIND(&exit);
  return add_func;
}

TNode<IntPtrT> BaseCollectionsAssembler::EstimatedInitialSize(
    TNode<Object> initial_entries, TNode<BoolT> is_fast_jsarray) {
  return Select<IntPtrT>(
      is_fast_jsarray,
      [=] { return SmiUntag(LoadFastJSArrayLength(CAST(initial_entries))); },
      [=] { return IntPtrConstant(0); }, MachineType::PointerRepresentation());
}

void BaseCollectionsAssembler::GotoIfNotJSReceiver(Node* const obj,
                                                   Label* if_not_receiver) {
  GotoIf(TaggedIsSmi(obj), if_not_receiver);
  GotoIfNot(IsJSReceiver(obj), if_not_receiver);
}

TNode<Object> BaseCollectionsAssembler::LoadAndNormalizeFixedArrayElement(
    TNode<Object> elements, TNode<IntPtrT> index) {
  TNode<Object> element = CAST(LoadFixedArrayElement(elements, index));
  return Select<Object>(IsTheHole(element), [=] { return UndefinedConstant(); },
                        [=] { return element; },
                        MachineRepresentation::kTagged);
}

TNode<Object> BaseCollectionsAssembler::LoadAndNormalizeFixedDoubleArrayElement(
    TNode<Object> elements, TNode<IntPtrT> index) {
  TVARIABLE(Object, entry);
  Label if_hole(this, Label::kDeferred), next(this);
  TNode<Float64T> element = UncheckedCast<Float64T>(LoadFixedDoubleArrayElement(
      elements, index, MachineType::Float64(), 0, INTPTR_PARAMETERS, &if_hole));
  {  // not hole
    entry = AllocateHeapNumberWithValue(element);
    Goto(&next);
  }
  BIND(&if_hole);
  {
    entry = UndefinedConstant();
    Goto(&next);
  }
  BIND(&next);
  return entry;
}

void BaseCollectionsAssembler::LoadKeyValue(TNode<Context> context,
                                            TNode<Object> maybe_array,
                                            TVariable<Object>* key,
                                            TVariable<Object>* value,
                                            Label* if_exception,
                                            TVariable<Object>* var_exception) {
  CSA_ASSERT(this, Word32BinaryNot(IsTheHole(maybe_array)));

  Label exit(this), if_fast(this), if_slow(this, Label::kDeferred);
  BranchIfFastJSArray(maybe_array, context, &if_fast, &if_slow);
  BIND(&if_fast);
  {
    TNode<JSArray> array = CAST(maybe_array);
    TNode<Smi> length = LoadFastJSArrayLength(array);
    TNode<FixedArrayBase> elements = LoadElements(array);
    TNode<Int32T> elements_kind = LoadMapElementsKind(LoadMap(array));

    Label if_smiorobjects(this), if_doubles(this);
    Branch(IsFastSmiOrTaggedElementsKind(elements_kind), &if_smiorobjects,
           &if_doubles);
    BIND(&if_smiorobjects);
    {
      Label if_one(this), if_two(this);
      GotoIf(SmiGreaterThan(length, SmiConstant(1)), &if_two);
      GotoIf(SmiEqual(length, SmiConstant(1)), &if_one);
      {  // empty array
        *key = UndefinedConstant();
        *value = UndefinedConstant();
        Goto(&exit);
      }
      BIND(&if_one);
      {
        *key = LoadAndNormalizeFixedArrayElement(elements, IntPtrConstant(0));
        *value = UndefinedConstant();
        Goto(&exit);
      }
      BIND(&if_two);
      {
        *key = LoadAndNormalizeFixedArrayElement(elements, IntPtrConstant(0));
        *value = LoadAndNormalizeFixedArrayElement(elements, IntPtrConstant(1));
        Goto(&exit);
      }
    }
    BIND(&if_doubles);
    {
      Label if_one(this), if_two(this);
      GotoIf(SmiGreaterThan(length, SmiConstant(1)), &if_two);
      GotoIf(SmiEqual(length, SmiConstant(1)), &if_one);
      {  // empty array
        *key = UndefinedConstant();
        *value = UndefinedConstant();
        Goto(&exit);
      }
      BIND(&if_one);
      {
        *key = LoadAndNormalizeFixedDoubleArrayElement(elements,
                                                       IntPtrConstant(0));
        *value = UndefinedConstant();
        Goto(&exit);
      }
      BIND(&if_two);
      {
        *key = LoadAndNormalizeFixedDoubleArrayElement(elements,
                                                       IntPtrConstant(0));
        *value = LoadAndNormalizeFixedDoubleArrayElement(elements,
                                                         IntPtrConstant(1));
        Goto(&exit);
      }
    }
  }
  BIND(&if_slow);
  {
    *key = UncheckedCast<Object>(
        GetProperty(context, maybe_array, isolate()->factory()->zero_string()));
    if (if_exception != nullptr) {
      DCHECK(var_exception != nullptr);
      GotoIfException(*key, if_exception, var_exception);
    }

    *value = UncheckedCast<Object>(
        GetProperty(context, maybe_array, isolate()->factory()->one_string()));
    if (if_exception != nullptr) {
      DCHECK(var_exception != nullptr);
      GotoIfException(*value, if_exception, var_exception);
    }
    Goto(&exit);
  }
  BIND(&exit);
}

class CollectionsBuiltinsAssembler : public BaseCollectionsAssembler {
 public:
  explicit CollectionsBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : BaseCollectionsAssembler(state) {}

 protected:
  template <typename CollectionType>
  Node* AllocateOrderedHashTable();
  template <typename IteratorType>
  Node* AllocateJSCollectionIterator(Node* context, int map_index,
                                     Node* collection);
  TNode<Object> AllocateTable(Variant variant, TNode<Context> context,
                              TNode<IntPtrT> at_least_space_for);
  Node* GetHash(Node* const key);
  Node* CallGetHashRaw(Node* const key);
  Node* CallGetOrCreateHashRaw(Node* const key);

  // Transitions the iterator to the non obsolete backing store.
  // This is a NOP if the [table] is not obsolete.
  typedef std::function<void(Node* const table, Node* const index)>
      UpdateInTransition;
  template <typename TableType>
  std::tuple<Node*, Node*> Transition(
      Node* const table, Node* const index,
      UpdateInTransition const& update_in_transition);
  template <typename IteratorType, typename TableType>
  std::tuple<Node*, Node*> TransitionAndUpdate(Node* const iterator);
  template <typename TableType>
  std::tuple<Node*, Node*, Node*> NextSkipHoles(Node* table, Node* index,
                                                Label* if_end);

  // Builds code that finds OrderedHashTable entry for a key with hash code
  // {hash} with using the comparison code generated by {key_compare}. The code
  // jumps to {entry_found} if the key is found, or to {not_found} if the key
  // was not found. In the {entry_found} branch, the variable
  // entry_start_position will be bound to the index of the entry (relative to
  // OrderedHashTable::kHashTableStartIndex).
  //
  // The {CollectionType} template parameter stands for the particular instance
  // of OrderedHashTable, it should be OrderedHashMap or OrderedHashSet.
  template <typename CollectionType>
  void FindOrderedHashTableEntry(
      Node* table, Node* hash,
      std::function<void(Node* other, Label* if_same, Label* if_not_same)>
          key_compare,
      Variable* entry_start_position, Label* entry_found, Label* not_found);

  // Specialization for Smi.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise.
  template <typename CollectionType>
  void FindOrderedHashTableEntryForSmiKey(Node* table, Node* key_tagged,
                                          Variable* result, Label* entry_found,
                                          Label* not_found);
  void SameValueZeroSmi(Node* key_smi, Node* candidate_key, Label* if_same,
                        Label* if_not_same);

  // Specialization for heap numbers.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise.
  void SameValueZeroHeapNumber(Node* key_string, Node* candidate_key,
                               Label* if_same, Label* if_not_same);
  template <typename CollectionType>
  void FindOrderedHashTableEntryForHeapNumberKey(Node* context, Node* table,
                                                 Node* key_heap_number,
                                                 Variable* result,
                                                 Label* entry_found,
                                                 Label* not_found);

  // Specialization for bigints.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise.
  void SameValueZeroBigInt(Node* key, Node* candidate_key, Label* if_same,
                           Label* if_not_same);
  template <typename CollectionType>
  void FindOrderedHashTableEntryForBigIntKey(Node* context, Node* table,
                                             Node* key, Variable* result,
                                             Label* entry_found,
                                             Label* not_found);

  // Specialization for string.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise.
  template <typename CollectionType>
  void FindOrderedHashTableEntryForStringKey(Node* context, Node* table,
                                             Node* key_tagged, Variable* result,
                                             Label* entry_found,
                                             Label* not_found);
  Node* ComputeIntegerHashForString(Node* context, Node* string_key);
  void SameValueZeroString(Node* context, Node* key_string, Node* candidate_key,
                           Label* if_same, Label* if_not_same);

  // Specialization for non-strings, non-numbers. For those we only need
  // reference equality to compare the keys.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise. If the hash-code has not been computed, it
  // should be Smi -1.
  template <typename CollectionType>
  void FindOrderedHashTableEntryForOtherKey(Node* context, Node* table,
                                            Node* key, Variable* result,
                                            Label* entry_found,
                                            Label* not_found);

  template <typename CollectionType>
  void TryLookupOrderedHashTableIndex(Node* const table, Node* const key,
                                      Node* const context, Variable* result,
                                      Label* if_entry_found,
                                      Label* if_not_found);

  Node* NormalizeNumberKey(Node* key);
  void StoreOrderedHashMapNewEntry(Node* const table, Node* const key,
                                   Node* const value, Node* const hash,
                                   Node* const number_of_buckets,
                                   Node* const occupancy);
  void StoreOrderedHashSetNewEntry(Node* const table, Node* const key,
                                   Node* const hash,
                                   Node* const number_of_buckets,
                                   Node* const occupancy);
};

template <typename CollectionType>
Node* CollectionsBuiltinsAssembler::AllocateOrderedHashTable() {
  static const int kCapacity = CollectionType::kMinCapacity;
  static const int kBucketCount = kCapacity / CollectionType::kLoadFactor;
  static const int kDataTableLength = kCapacity * CollectionType::kEntrySize;
  static const int kFixedArrayLength =
      CollectionType::kHashTableStartIndex + kBucketCount + kDataTableLength;
  static const int kDataTableStartIndex =
      CollectionType::kHashTableStartIndex + kBucketCount;

  STATIC_ASSERT(base::bits::IsPowerOfTwo(kCapacity));
  STATIC_ASSERT(kCapacity <= CollectionType::kMaxCapacity);

  // Allocate the table and add the proper map.
  const ElementsKind elements_kind = HOLEY_ELEMENTS;
  Node* const length_intptr = IntPtrConstant(kFixedArrayLength);
  Node* const fixed_array_map = LoadRoot(
      static_cast<Heap::RootListIndex>(CollectionType::GetMapRootIndex()));
  Node* const table =
      AllocateFixedArray(elements_kind, length_intptr, INTPTR_PARAMETERS,
                         kAllowLargeObjectAllocation, fixed_array_map);

  // Initialize the OrderedHashTable fields.
  const WriteBarrierMode barrier_mode = SKIP_WRITE_BARRIER;
  StoreFixedArrayElement(table, CollectionType::kNumberOfElementsIndex,
                         SmiConstant(0), barrier_mode);
  StoreFixedArrayElement(table, CollectionType::kNumberOfDeletedElementsIndex,
                         SmiConstant(0), barrier_mode);
  StoreFixedArrayElement(table, CollectionType::kNumberOfBucketsIndex,
                         SmiConstant(kBucketCount), barrier_mode);

  // Fill the buckets with kNotFound.
  Node* const not_found = SmiConstant(CollectionType::kNotFound);
  STATIC_ASSERT(CollectionType::kHashTableStartIndex ==
                CollectionType::kNumberOfBucketsIndex + 1);
  STATIC_ASSERT((CollectionType::kHashTableStartIndex + kBucketCount) ==
                kDataTableStartIndex);
  for (int i = 0; i < kBucketCount; i++) {
    StoreFixedArrayElement(table, CollectionType::kHashTableStartIndex + i,
                           not_found, barrier_mode);
  }

  // Fill the data table with undefined.
  STATIC_ASSERT(kDataTableStartIndex + kDataTableLength == kFixedArrayLength);
  for (int i = 0; i < kDataTableLength; i++) {
    StoreFixedArrayElement(table, kDataTableStartIndex + i, UndefinedConstant(),
                           barrier_mode);
  }

  return table;
}

template <typename IteratorType>
Node* CollectionsBuiltinsAssembler::AllocateJSCollectionIterator(
    Node* context, int map_index, Node* collection) {
  Node* const table = LoadObjectField(collection, JSCollection::kTableOffset);
  Node* const native_context = LoadNativeContext(context);
  Node* const iterator_map = LoadContextElement(native_context, map_index);
  Node* const iterator = AllocateInNewSpace(IteratorType::kSize);
  StoreMapNoWriteBarrier(iterator, iterator_map);
  StoreObjectFieldRoot(iterator, IteratorType::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(iterator, IteratorType::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(iterator, IteratorType::kTableOffset, table);
  StoreObjectFieldNoWriteBarrier(iterator, IteratorType::kIndexOffset,
                                 SmiConstant(0));
  return iterator;
}

TNode<Object> CollectionsBuiltinsAssembler::AllocateTable(
    Variant variant, TNode<Context> context,
    TNode<IntPtrT> at_least_space_for) {
  return CAST(variant == kMap ? AllocateOrderedHashTable<OrderedHashMap>()
                              : AllocateOrderedHashTable<OrderedHashSet>());
}

TF_BUILTIN(MapConstructor, CollectionsBuiltinsAssembler) {
  GenerateConstructor(kMap, Context::JS_MAP_FUN_INDEX,
                      isolate()->factory()->Map_string(), JSMap::kTableOffset);
}

TF_BUILTIN(SetConstructor, CollectionsBuiltinsAssembler) {
  GenerateConstructor(kSet, Context::JS_SET_FUN_INDEX,
                      isolate()->factory()->Set_string(), JSSet::kTableOffset);
}

Node* CollectionsBuiltinsAssembler::CallGetOrCreateHashRaw(Node* const key) {
  Node* const function_addr =
      ExternalConstant(ExternalReference::get_or_create_hash_raw(isolate()));
  Node* const isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_tagged = MachineType::AnyTagged();

  Node* const result = CallCFunction2(type_tagged, type_ptr, type_tagged,
                                      function_addr, isolate_ptr, key);

  return result;
}

Node* CollectionsBuiltinsAssembler::CallGetHashRaw(Node* const key) {
  Node* const function_addr = ExternalConstant(
      ExternalReference::orderedhashmap_gethash_raw(isolate()));
  Node* const isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_tagged = MachineType::AnyTagged();

  Node* const result = CallCFunction2(type_tagged, type_ptr, type_tagged,
                                      function_addr, isolate_ptr, key);
  return SmiUntag(result);
}

Node* CollectionsBuiltinsAssembler::GetHash(Node* const key) {
  VARIABLE(var_hash, MachineType::PointerRepresentation());
  Label if_receiver(this), if_other(this), done(this);
  Branch(IsJSReceiver(key), &if_receiver, &if_other);

  BIND(&if_receiver);
  {
    var_hash.Bind(LoadJSReceiverIdentityHash(key));
    Goto(&done);
  }

  BIND(&if_other);
  {
    var_hash.Bind(CallGetHashRaw(key));
    Goto(&done);
  }

  BIND(&done);
  return var_hash.value();
}

void CollectionsBuiltinsAssembler::SameValueZeroSmi(Node* key_smi,
                                                    Node* candidate_key,
                                                    Label* if_same,
                                                    Label* if_not_same) {
  // If the key is the same, we are done.
  GotoIf(WordEqual(candidate_key, key_smi), if_same);

  // If the candidate key is smi, then it must be different (because
  // we already checked for equality above).
  GotoIf(TaggedIsSmi(candidate_key), if_not_same);

  // If the candidate key is not smi, we still have to check if it is a
  // heap number with the same value.
  GotoIfNot(IsHeapNumber(candidate_key), if_not_same);

  Node* const candidate_key_number = LoadHeapNumberValue(candidate_key);
  Node* const key_number = SmiToFloat64(key_smi);

  GotoIf(Float64Equal(candidate_key_number, key_number), if_same);

  Goto(if_not_same);
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForSmiKey(
    Node* table, Node* smi_key, Variable* result, Label* entry_found,
    Label* not_found) {
  Node* const key_untagged = SmiUntag(smi_key);
  Node* const hash =
      ChangeInt32ToIntPtr(ComputeIntegerHash(key_untagged, Int32Constant(0)));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  result->Bind(hash);
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](Node* other_key, Label* if_same, Label* if_not_same) {
        SameValueZeroSmi(smi_key, other_key, if_same, if_not_same);
      },
      result, entry_found, not_found);
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForStringKey(
    Node* context, Node* table, Node* key_tagged, Variable* result,
    Label* entry_found, Label* not_found) {
  Node* const hash = ComputeIntegerHashForString(context, key_tagged);
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  result->Bind(hash);
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](Node* other_key, Label* if_same, Label* if_not_same) {
        SameValueZeroString(context, key_tagged, other_key, if_same,
                            if_not_same);
      },
      result, entry_found, not_found);
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForHeapNumberKey(
    Node* context, Node* table, Node* key_heap_number, Variable* result,
    Label* entry_found, Label* not_found) {
  Node* hash = CallGetHashRaw(key_heap_number);
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  result->Bind(hash);
  Node* const key_float = LoadHeapNumberValue(key_heap_number);
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](Node* other_key, Label* if_same, Label* if_not_same) {
        SameValueZeroHeapNumber(key_float, other_key, if_same, if_not_same);
      },
      result, entry_found, not_found);
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForBigIntKey(
    Node* context, Node* table, Node* key, Variable* result, Label* entry_found,
    Label* not_found) {
  Node* hash = CallGetHashRaw(key);
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  result->Bind(hash);
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](Node* other_key, Label* if_same, Label* if_not_same) {
        SameValueZeroBigInt(key, other_key, if_same, if_not_same);
      },
      result, entry_found, not_found);
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForOtherKey(
    Node* context, Node* table, Node* key, Variable* result, Label* entry_found,
    Label* not_found) {
  Node* hash = GetHash(key);
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  result->Bind(hash);
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](Node* other_key, Label* if_same, Label* if_not_same) {
        Branch(WordEqual(key, other_key), if_same, if_not_same);
      },
      result, entry_found, not_found);
}

Node* CollectionsBuiltinsAssembler::ComputeIntegerHashForString(
    Node* context, Node* string_key) {
  VARIABLE(var_result, MachineType::PointerRepresentation());

  Label hash_not_computed(this), done(this, &var_result);
  Node* hash =
      ChangeInt32ToIntPtr(LoadNameHash(string_key, &hash_not_computed));
  var_result.Bind(hash);
  Goto(&done);

  BIND(&hash_not_computed);
  var_result.Bind(CallGetHashRaw(string_key));
  Goto(&done);

  BIND(&done);
  return var_result.value();
}

void CollectionsBuiltinsAssembler::SameValueZeroString(Node* context,
                                                       Node* key_string,
                                                       Node* candidate_key,
                                                       Label* if_same,
                                                       Label* if_not_same) {
  // If the candidate is not a string, the keys are not equal.
  GotoIf(TaggedIsSmi(candidate_key), if_not_same);
  GotoIfNot(IsString(candidate_key), if_not_same);

  Branch(WordEqual(CallBuiltin(Builtins::kStringEqual, context, key_string,
                               candidate_key),
                   TrueConstant()),
         if_same, if_not_same);
}

void CollectionsBuiltinsAssembler::SameValueZeroBigInt(Node* key,
                                                       Node* candidate_key,
                                                       Label* if_same,
                                                       Label* if_not_same) {
  CSA_ASSERT(this, IsBigInt(key));
  GotoIf(TaggedIsSmi(candidate_key), if_not_same);
  GotoIfNot(IsBigInt(candidate_key), if_not_same);

  Branch(WordEqual(CallRuntime(Runtime::kBigIntEqualToBigInt,
                               NoContextConstant(), key, candidate_key),
                   TrueConstant()),
         if_same, if_not_same);
}

void CollectionsBuiltinsAssembler::SameValueZeroHeapNumber(Node* key_float,
                                                           Node* candidate_key,
                                                           Label* if_same,
                                                           Label* if_not_same) {
  Label if_smi(this), if_keyisnan(this);

  GotoIf(TaggedIsSmi(candidate_key), &if_smi);
  GotoIfNot(IsHeapNumber(candidate_key), if_not_same);

  {
    // {candidate_key} is a heap number.
    Node* const candidate_float = LoadHeapNumberValue(candidate_key);
    GotoIf(Float64Equal(key_float, candidate_float), if_same);

    // SameValueZero needs to treat NaNs as equal. First check if {key_float}
    // is NaN.
    BranchIfFloat64IsNaN(key_float, &if_keyisnan, if_not_same);

    BIND(&if_keyisnan);
    {
      // Return true iff {candidate_key} is NaN.
      Branch(Float64Equal(candidate_float, candidate_float), if_not_same,
             if_same);
    }
  }

  BIND(&if_smi);
  {
    Node* const candidate_float = SmiToFloat64(candidate_key);
    Branch(Float64Equal(key_float, candidate_float), if_same, if_not_same);
  }
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntry(
    Node* table, Node* hash,
    std::function<void(Node*, Label*, Label*)> key_compare,
    Variable* entry_start_position, Label* entry_found, Label* not_found) {
  // Get the index of the bucket.
  Node* const number_of_buckets = SmiUntag(
      LoadFixedArrayElement(table, CollectionType::kNumberOfBucketsIndex));
  Node* const bucket =
      WordAnd(hash, IntPtrSub(number_of_buckets, IntPtrConstant(1)));
  Node* const first_entry = SmiUntag(LoadFixedArrayElement(
      table, bucket, CollectionType::kHashTableStartIndex * kPointerSize));

  // Walk the bucket chain.
  Node* entry_start;
  Label if_key_found(this);
  {
    VARIABLE(var_entry, MachineType::PointerRepresentation(), first_entry);
    Label loop(this, {&var_entry, entry_start_position}),
        continue_next_entry(this);
    Goto(&loop);
    BIND(&loop);

    // If the entry index is the not-found sentinel, we are done.
    GotoIf(
        WordEqual(var_entry.value(), IntPtrConstant(CollectionType::kNotFound)),
        not_found);

    // Make sure the entry index is within range.
    CSA_ASSERT(
        this,
        UintPtrLessThan(
            var_entry.value(),
            SmiUntag(SmiAdd(
                LoadFixedArrayElement(table,
                                      CollectionType::kNumberOfElementsIndex),
                LoadFixedArrayElement(
                    table, CollectionType::kNumberOfDeletedElementsIndex)))));

    // Compute the index of the entry relative to kHashTableStartIndex.
    entry_start =
        IntPtrAdd(IntPtrMul(var_entry.value(),
                            IntPtrConstant(CollectionType::kEntrySize)),
                  number_of_buckets);

    // Load the key from the entry.
    Node* const candidate_key = LoadFixedArrayElement(
        table, entry_start,
        CollectionType::kHashTableStartIndex * kPointerSize);

    key_compare(candidate_key, &if_key_found, &continue_next_entry);

    BIND(&continue_next_entry);
    // Load the index of the next entry in the bucket chain.
    var_entry.Bind(SmiUntag(LoadFixedArrayElement(
        table, entry_start,
        (CollectionType::kHashTableStartIndex + CollectionType::kChainOffset) *
            kPointerSize)));

    Goto(&loop);
  }

  BIND(&if_key_found);
  entry_start_position->Bind(entry_start);
  Goto(entry_found);
}

TF_BUILTIN(OrderedHashTableHealIndex, CollectionsBuiltinsAssembler) {
  Node* table = Parameter(Descriptor::kTable);
  Node* index = Parameter(Descriptor::kIndex);
  CSA_ASSERT(this, TaggedIsNotSmi(table));
  CSA_ASSERT(this, TaggedIsSmi(index));
  Label return_index(this), return_zero(this);

  // Check if we need to update the {index}.
  GotoIfNot(SmiLessThan(SmiConstant(0), index), &return_zero);

  // Check if the {table} was cleared.
  Node* number_of_deleted_elements = LoadAndUntagObjectField(
      table, OrderedHashTableBase::kNumberOfDeletedElementsOffset);
  GotoIf(WordEqual(number_of_deleted_elements,
                   IntPtrConstant(OrderedHashTableBase::kClearedTableSentinel)),
         &return_zero);

  VARIABLE(var_i, MachineType::PointerRepresentation(), IntPtrConstant(0));
  VARIABLE(var_index, MachineRepresentation::kTagged, index);
  Label loop(this, {&var_i, &var_index});
  Goto(&loop);
  BIND(&loop);
  {
    Node* i = var_i.value();
    GotoIfNot(IntPtrLessThan(i, number_of_deleted_elements), &return_index);
    Node* removed_index = LoadFixedArrayElement(
        table, i, OrderedHashTableBase::kRemovedHolesIndex * kPointerSize);
    GotoIf(SmiGreaterThanOrEqual(removed_index, index), &return_index);
    Decrement(&var_index, 1, SMI_PARAMETERS);
    Increment(&var_i);
    Goto(&loop);
  }

  BIND(&return_index);
  Return(var_index.value());

  BIND(&return_zero);
  Return(SmiConstant(0));
}

template <typename TableType>
std::tuple<Node*, Node*> CollectionsBuiltinsAssembler::Transition(
    Node* const table, Node* const index,
    UpdateInTransition const& update_in_transition) {
  VARIABLE(var_index, MachineType::PointerRepresentation(), index);
  VARIABLE(var_table, MachineRepresentation::kTagged, table);
  Label if_done(this), if_transition(this, Label::kDeferred);
  Branch(TaggedIsSmi(
             LoadObjectField(var_table.value(), TableType::kNextTableOffset)),
         &if_done, &if_transition);

  BIND(&if_transition);
  {
    Label loop(this, {&var_table, &var_index}), done_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      Node* table = var_table.value();
      Node* index = var_index.value();

      Node* next_table = LoadObjectField(table, TableType::kNextTableOffset);
      GotoIf(TaggedIsSmi(next_table), &done_loop);

      var_table.Bind(next_table);
      var_index.Bind(
          SmiUntag(CallBuiltin(Builtins::kOrderedHashTableHealIndex,
                               NoContextConstant(), table, SmiTag(index))));
      Goto(&loop);
    }
    BIND(&done_loop);

    // Update with the new {table} and {index}.
    update_in_transition(var_table.value(), var_index.value());
    Goto(&if_done);
  }

  BIND(&if_done);
  return std::tuple<Node*, Node*>(var_table.value(), var_index.value());
}

template <typename IteratorType, typename TableType>
std::tuple<Node*, Node*> CollectionsBuiltinsAssembler::TransitionAndUpdate(
    Node* const iterator) {
  return Transition<TableType>(
      LoadObjectField(iterator, IteratorType::kTableOffset),
      LoadAndUntagObjectField(iterator, IteratorType::kIndexOffset),
      [this, iterator](Node* const table, Node* const index) {
        // Update the {iterator} with the new state.
        StoreObjectField(iterator, IteratorType::kTableOffset, table);
        StoreObjectFieldNoWriteBarrier(iterator, IteratorType::kIndexOffset,
                                       SmiTag(index));
      });
}

template <typename TableType>
std::tuple<Node*, Node*, Node*> CollectionsBuiltinsAssembler::NextSkipHoles(
    Node* table, Node* index, Label* if_end) {
  // Compute the used capacity for the {table}.
  Node* number_of_buckets =
      LoadAndUntagObjectField(table, TableType::kNumberOfBucketsOffset);
  Node* number_of_elements =
      LoadAndUntagObjectField(table, TableType::kNumberOfElementsOffset);
  Node* number_of_deleted_elements =
      LoadAndUntagObjectField(table, TableType::kNumberOfDeletedElementsOffset);
  Node* used_capacity =
      IntPtrAdd(number_of_elements, number_of_deleted_elements);

  Node* entry_key;
  Node* entry_start_position;
  VARIABLE(var_index, MachineType::PointerRepresentation(), index);
  Label loop(this, &var_index), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    GotoIfNot(IntPtrLessThan(var_index.value(), used_capacity), if_end);
    entry_start_position = IntPtrAdd(
        IntPtrMul(var_index.value(), IntPtrConstant(TableType::kEntrySize)),
        number_of_buckets);
    entry_key =
        LoadFixedArrayElement(table, entry_start_position,
                              TableType::kHashTableStartIndex * kPointerSize);
    Increment(&var_index);
    Branch(IsTheHole(entry_key), &loop, &done_loop);
  }

  BIND(&done_loop);
  return std::tuple<Node*, Node*, Node*>(entry_key, entry_start_position,
                                         var_index.value());
}

TF_BUILTIN(MapPrototypeGet, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, "Map.prototype.get");

  Node* const table = LoadObjectField(receiver, JSMap::kTableOffset);
  Node* index =
      CallBuiltin(Builtins::kFindOrderedHashMapEntry, context, table, key);

  Label if_found(this), if_not_found(this);
  Branch(SmiGreaterThanOrEqual(index, SmiConstant(0)), &if_found,
         &if_not_found);

  BIND(&if_found);
  Return(LoadFixedArrayElement(
      table, SmiUntag(index),
      (OrderedHashMap::kHashTableStartIndex + OrderedHashMap::kValueOffset) *
          kPointerSize));

  BIND(&if_not_found);
  Return(UndefinedConstant());
}

TF_BUILTIN(MapPrototypeHas, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, "Map.prototype.has");

  Node* const table = LoadObjectField(receiver, JSMap::kTableOffset);
  Node* index =
      CallBuiltin(Builtins::kFindOrderedHashMapEntry, context, table, key);

  Label if_found(this), if_not_found(this);
  Branch(SmiGreaterThanOrEqual(index, SmiConstant(0)), &if_found,
         &if_not_found);

  BIND(&if_found);
  Return(TrueConstant());

  BIND(&if_not_found);
  Return(FalseConstant());
}

Node* CollectionsBuiltinsAssembler::NormalizeNumberKey(Node* const key) {
  VARIABLE(result, MachineRepresentation::kTagged, key);
  Label done(this);

  GotoIf(TaggedIsSmi(key), &done);
  GotoIfNot(IsHeapNumber(key), &done);
  Node* const number = LoadHeapNumberValue(key);
  GotoIfNot(Float64Equal(number, Float64Constant(0.0)), &done);
  // We know the value is zero, so we take the key to be Smi 0.
  // Another option would be to normalize to Smi here.
  result.Bind(SmiConstant(0));
  Goto(&done);

  BIND(&done);
  return result.value();
}

TF_BUILTIN(MapPrototypeSet, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kKey);
  Node* const value = Parameter(Descriptor::kValue);
  Node* const context = Parameter(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, "Map.prototype.set");

  key = NormalizeNumberKey(key);

  Node* const table = LoadObjectField(receiver, JSMap::kTableOffset);

  VARIABLE(entry_start_position_or_hash, MachineType::PointerRepresentation(),
           IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashMap>(table, key, context,
                                                 &entry_start_position_or_hash,
                                                 &entry_found, &not_found);

  BIND(&entry_found);
  // If we found the entry, we just store the value there.
  StoreFixedArrayElement(table, entry_start_position_or_hash.value(), value,
                         UPDATE_WRITE_BARRIER,
                         kPointerSize * (OrderedHashMap::kHashTableStartIndex +
                                         OrderedHashMap::kValueOffset));
  Return(receiver);

  Label no_hash(this), add_entry(this), store_new_entry(this);
  BIND(&not_found);
  {
    // If we have a hash code, we can start adding the new entry.
    GotoIf(IntPtrGreaterThan(entry_start_position_or_hash.value(),
                             IntPtrConstant(0)),
           &add_entry);

    // Otherwise, go to runtime to compute the hash code.
    entry_start_position_or_hash.Bind(SmiUntag(CallGetOrCreateHashRaw(key)));
    Goto(&add_entry);
  }

  BIND(&add_entry);
  VARIABLE(number_of_buckets, MachineType::PointerRepresentation());
  VARIABLE(occupancy, MachineType::PointerRepresentation());
  VARIABLE(table_var, MachineRepresentation::kTaggedPointer, table);
  {
    // Check we have enough space for the entry.
    number_of_buckets.Bind(SmiUntag(
        LoadFixedArrayElement(table, OrderedHashMap::kNumberOfBucketsIndex)));

    STATIC_ASSERT(OrderedHashMap::kLoadFactor == 2);
    Node* const capacity = WordShl(number_of_buckets.value(), 1);
    Node* const number_of_elements = SmiUntag(
        CAST(LoadObjectField(table, OrderedHashMap::kNumberOfElementsOffset)));
    Node* const number_of_deleted = SmiUntag(CAST(LoadObjectField(
        table, OrderedHashMap::kNumberOfDeletedElementsOffset)));
    occupancy.Bind(IntPtrAdd(number_of_elements, number_of_deleted));
    GotoIf(IntPtrLessThan(occupancy.value(), capacity), &store_new_entry);

    // We do not have enough space, grow the table and reload the relevant
    // fields.
    CallRuntime(Runtime::kMapGrow, context, receiver);
    table_var.Bind(LoadObjectField(receiver, JSMap::kTableOffset));
    number_of_buckets.Bind(SmiUntag(LoadFixedArrayElement(
        table_var.value(), OrderedHashMap::kNumberOfBucketsIndex)));
    Node* const new_number_of_elements = SmiUntag(CAST(LoadObjectField(
        table_var.value(), OrderedHashMap::kNumberOfElementsOffset)));
    Node* const new_number_of_deleted = SmiUntag(CAST(LoadObjectField(
        table_var.value(), OrderedHashMap::kNumberOfDeletedElementsOffset)));
    occupancy.Bind(IntPtrAdd(new_number_of_elements, new_number_of_deleted));
    Goto(&store_new_entry);
  }
  BIND(&store_new_entry);
  // Store the key, value and connect the element to the bucket chain.
  StoreOrderedHashMapNewEntry(table_var.value(), key, value,
                              entry_start_position_or_hash.value(),
                              number_of_buckets.value(), occupancy.value());
  Return(receiver);
}

void CollectionsBuiltinsAssembler::StoreOrderedHashMapNewEntry(
    Node* const table, Node* const key, Node* const value, Node* const hash,
    Node* const number_of_buckets, Node* const occupancy) {
  Node* const bucket =
      WordAnd(hash, IntPtrSub(number_of_buckets, IntPtrConstant(1)));
  Node* const bucket_entry = LoadFixedArrayElement(
      table, bucket, OrderedHashMap::kHashTableStartIndex * kPointerSize);

  // Store the entry elements.
  Node* const entry_start = IntPtrAdd(
      IntPtrMul(occupancy, IntPtrConstant(OrderedHashMap::kEntrySize)),
      number_of_buckets);
  StoreFixedArrayElement(table, entry_start, key, UPDATE_WRITE_BARRIER,
                         kPointerSize * OrderedHashMap::kHashTableStartIndex);
  StoreFixedArrayElement(table, entry_start, value, UPDATE_WRITE_BARRIER,
                         kPointerSize * (OrderedHashMap::kHashTableStartIndex +
                                         OrderedHashMap::kValueOffset));
  StoreFixedArrayElement(table, entry_start, bucket_entry, SKIP_WRITE_BARRIER,
                         kPointerSize * (OrderedHashMap::kHashTableStartIndex +
                                         OrderedHashMap::kChainOffset));

  // Update the bucket head.
  StoreFixedArrayElement(table, bucket, SmiTag(occupancy), SKIP_WRITE_BARRIER,
                         OrderedHashMap::kHashTableStartIndex * kPointerSize);

  // Bump the elements count.
  Node* const number_of_elements =
      LoadObjectField(table, OrderedHashMap::kNumberOfElementsOffset);
  StoreObjectFieldNoWriteBarrier(table, OrderedHashMap::kNumberOfElementsOffset,
                                 SmiAdd(number_of_elements, SmiConstant(1)));
}

TF_BUILTIN(MapPrototypeDelete, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE,
                         "Map.prototype.delete");

  Node* const table = LoadObjectField(receiver, JSMap::kTableOffset);

  VARIABLE(entry_start_position_or_hash, MachineType::PointerRepresentation(),
           IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashMap>(table, key, context,
                                                 &entry_start_position_or_hash,
                                                 &entry_found, &not_found);

  BIND(&not_found);
  Return(FalseConstant());

  BIND(&entry_found);
  // If we found the entry, mark the entry as deleted.
  StoreFixedArrayElement(table, entry_start_position_or_hash.value(),
                         TheHoleConstant(), UPDATE_WRITE_BARRIER,
                         kPointerSize * OrderedHashMap::kHashTableStartIndex);
  StoreFixedArrayElement(table, entry_start_position_or_hash.value(),
                         TheHoleConstant(), UPDATE_WRITE_BARRIER,
                         kPointerSize * (OrderedHashMap::kHashTableStartIndex +
                                         OrderedHashMap::kValueOffset));

  // Decrement the number of elements, increment the number of deleted elements.
  Node* const number_of_elements = SmiSub(
      CAST(LoadObjectField(table, OrderedHashMap::kNumberOfElementsOffset)),
      SmiConstant(1));
  StoreObjectFieldNoWriteBarrier(table, OrderedHashMap::kNumberOfElementsOffset,
                                 number_of_elements);
  Node* const number_of_deleted =
      SmiAdd(CAST(LoadObjectField(
                 table, OrderedHashMap::kNumberOfDeletedElementsOffset)),
             SmiConstant(1));
  StoreObjectFieldNoWriteBarrier(
      table, OrderedHashMap::kNumberOfDeletedElementsOffset, number_of_deleted);

  Node* const number_of_buckets =
      LoadFixedArrayElement(table, OrderedHashMap::kNumberOfBucketsIndex);

  // If there fewer elements than #buckets / 2, shrink the table.
  Label shrink(this);
  GotoIf(SmiLessThan(SmiAdd(number_of_elements, number_of_elements),
                     number_of_buckets),
         &shrink);
  Return(TrueConstant());

  BIND(&shrink);
  CallRuntime(Runtime::kMapShrink, context, receiver);
  Return(TrueConstant());
}

TF_BUILTIN(SetPrototypeAdd, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE, "Set.prototype.add");

  key = NormalizeNumberKey(key);

  Node* const table = LoadObjectField(receiver, JSMap::kTableOffset);

  VARIABLE(entry_start_position_or_hash, MachineType::PointerRepresentation(),
           IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashSet>(table, key, context,
                                                 &entry_start_position_or_hash,
                                                 &entry_found, &not_found);

  BIND(&entry_found);
  // The entry was found, there is nothing to do.
  Return(receiver);

  Label no_hash(this), add_entry(this), store_new_entry(this);
  BIND(&not_found);
  {
    // If we have a hash code, we can start adding the new entry.
    GotoIf(IntPtrGreaterThan(entry_start_position_or_hash.value(),
                             IntPtrConstant(0)),
           &add_entry);

    // Otherwise, go to runtime to compute the hash code.
    entry_start_position_or_hash.Bind(SmiUntag((CallGetOrCreateHashRaw(key))));
    Goto(&add_entry);
  }

  BIND(&add_entry);
  VARIABLE(number_of_buckets, MachineType::PointerRepresentation());
  VARIABLE(occupancy, MachineType::PointerRepresentation());
  VARIABLE(table_var, MachineRepresentation::kTaggedPointer, table);
  {
    // Check we have enough space for the entry.
    number_of_buckets.Bind(SmiUntag(
        LoadFixedArrayElement(table, OrderedHashSet::kNumberOfBucketsIndex)));

    STATIC_ASSERT(OrderedHashSet::kLoadFactor == 2);
    Node* const capacity = WordShl(number_of_buckets.value(), 1);
    Node* const number_of_elements = SmiUntag(
        CAST(LoadObjectField(table, OrderedHashSet::kNumberOfElementsOffset)));
    Node* const number_of_deleted = SmiUntag(CAST(LoadObjectField(
        table, OrderedHashSet::kNumberOfDeletedElementsOffset)));
    occupancy.Bind(IntPtrAdd(number_of_elements, number_of_deleted));
    GotoIf(IntPtrLessThan(occupancy.value(), capacity), &store_new_entry);

    // We do not have enough space, grow the table and reload the relevant
    // fields.
    CallRuntime(Runtime::kSetGrow, context, receiver);
    table_var.Bind(LoadObjectField(receiver, JSMap::kTableOffset));
    number_of_buckets.Bind(SmiUntag(LoadFixedArrayElement(
        table_var.value(), OrderedHashSet::kNumberOfBucketsIndex)));
    Node* const new_number_of_elements = SmiUntag(CAST(LoadObjectField(
        table_var.value(), OrderedHashSet::kNumberOfElementsOffset)));
    Node* const new_number_of_deleted = SmiUntag(CAST(LoadObjectField(
        table_var.value(), OrderedHashSet::kNumberOfDeletedElementsOffset)));
    occupancy.Bind(IntPtrAdd(new_number_of_elements, new_number_of_deleted));
    Goto(&store_new_entry);
  }
  BIND(&store_new_entry);
  // Store the key, value and connect the element to the bucket chain.
  StoreOrderedHashSetNewEntry(table_var.value(), key,
                              entry_start_position_or_hash.value(),
                              number_of_buckets.value(), occupancy.value());
  Return(receiver);
}

void CollectionsBuiltinsAssembler::StoreOrderedHashSetNewEntry(
    Node* const table, Node* const key, Node* const hash,
    Node* const number_of_buckets, Node* const occupancy) {
  Node* const bucket =
      WordAnd(hash, IntPtrSub(number_of_buckets, IntPtrConstant(1)));
  Node* const bucket_entry = LoadFixedArrayElement(
      table, bucket, OrderedHashSet::kHashTableStartIndex * kPointerSize);

  // Store the entry elements.
  Node* const entry_start = IntPtrAdd(
      IntPtrMul(occupancy, IntPtrConstant(OrderedHashSet::kEntrySize)),
      number_of_buckets);
  StoreFixedArrayElement(table, entry_start, key, UPDATE_WRITE_BARRIER,
                         kPointerSize * OrderedHashSet::kHashTableStartIndex);
  StoreFixedArrayElement(table, entry_start, bucket_entry, SKIP_WRITE_BARRIER,
                         kPointerSize * (OrderedHashSet::kHashTableStartIndex +
                                         OrderedHashSet::kChainOffset));

  // Update the bucket head.
  StoreFixedArrayElement(table, bucket, SmiTag(occupancy), SKIP_WRITE_BARRIER,
                         OrderedHashSet::kHashTableStartIndex * kPointerSize);

  // Bump the elements count.
  Node* const number_of_elements =
      LoadObjectField(table, OrderedHashSet::kNumberOfElementsOffset);
  StoreObjectFieldNoWriteBarrier(table, OrderedHashSet::kNumberOfElementsOffset,
                                 SmiAdd(number_of_elements, SmiConstant(1)));
}

TF_BUILTIN(SetPrototypeDelete, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE,
                         "Set.prototype.delete");

  Node* const table = LoadObjectField(receiver, JSMap::kTableOffset);

  VARIABLE(entry_start_position_or_hash, MachineType::PointerRepresentation(),
           IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashSet>(table, key, context,
                                                 &entry_start_position_or_hash,
                                                 &entry_found, &not_found);

  BIND(&not_found);
  Return(FalseConstant());

  BIND(&entry_found);
  // If we found the entry, mark the entry as deleted.
  StoreFixedArrayElement(table, entry_start_position_or_hash.value(),
                         TheHoleConstant(), UPDATE_WRITE_BARRIER,
                         kPointerSize * OrderedHashSet::kHashTableStartIndex);

  // Decrement the number of elements, increment the number of deleted elements.
  Node* const number_of_elements = SmiSub(
      CAST(LoadObjectField(table, OrderedHashSet::kNumberOfElementsOffset)),
      SmiConstant(1));
  StoreObjectFieldNoWriteBarrier(table, OrderedHashSet::kNumberOfElementsOffset,
                                 number_of_elements);
  Node* const number_of_deleted =
      SmiAdd(CAST(LoadObjectField(
                 table, OrderedHashSet::kNumberOfDeletedElementsOffset)),
             SmiConstant(1));
  StoreObjectFieldNoWriteBarrier(
      table, OrderedHashSet::kNumberOfDeletedElementsOffset, number_of_deleted);

  Node* const number_of_buckets =
      LoadFixedArrayElement(table, OrderedHashSet::kNumberOfBucketsIndex);

  // If there fewer elements than #buckets / 2, shrink the table.
  Label shrink(this);
  GotoIf(SmiLessThan(SmiAdd(number_of_elements, number_of_elements),
                     number_of_buckets),
         &shrink);
  Return(TrueConstant());

  BIND(&shrink);
  CallRuntime(Runtime::kSetShrink, context, receiver);
  Return(TrueConstant());
}

TF_BUILTIN(MapPrototypeEntries, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE,
                         "Map.prototype.entries");
  Return(AllocateJSCollectionIterator<JSMapIterator>(
      context, Context::MAP_KEY_VALUE_ITERATOR_MAP_INDEX, receiver));
}

TF_BUILTIN(MapPrototypeGetSize, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE,
                         "get Map.prototype.size");
  Node* const table = LoadObjectField(receiver, JSMap::kTableOffset);
  Return(LoadObjectField(table, OrderedHashMap::kNumberOfElementsOffset));
}

TF_BUILTIN(MapPrototypeForEach, CollectionsBuiltinsAssembler) {
  const char* const kMethodName = "Map.prototype.forEach";
  Node* const argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* const context = Parameter(BuiltinDescriptor::kContext);
  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  Node* const receiver = args.GetReceiver();
  Node* const callback = args.GetOptionalArgumentValue(0);
  Node* const this_arg = args.GetOptionalArgumentValue(1);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, kMethodName);

  // Ensure that {callback} is actually callable.
  Label callback_not_callable(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(callback), &callback_not_callable);
  GotoIfNot(IsCallable(callback), &callback_not_callable);

  VARIABLE(var_index, MachineType::PointerRepresentation(), IntPtrConstant(0));
  VARIABLE(var_table, MachineRepresentation::kTagged,
           LoadObjectField(receiver, JSMap::kTableOffset));
  Label loop(this, {&var_index, &var_table}), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Transition {table} and {index} if there was any modification to
    // the {receiver} while we're iterating.
    Node* index = var_index.value();
    Node* table = var_table.value();
    std::tie(table, index) =
        Transition<OrderedHashMap>(table, index, [](Node*, Node*) {});

    // Read the next entry from the {table}, skipping holes.
    Node* entry_key;
    Node* entry_start_position;
    std::tie(entry_key, entry_start_position, index) =
        NextSkipHoles<OrderedHashMap>(table, index, &done_loop);

    // Load the entry value as well.
    Node* entry_value = LoadFixedArrayElement(
        table, entry_start_position,
        (OrderedHashMap::kHashTableStartIndex + OrderedHashMap::kValueOffset) *
            kPointerSize);

    // Invoke the {callback} passing the {entry_key}, {entry_value} and the
    // {receiver}.
    CallJS(CodeFactory::Call(isolate()), context, callback, this_arg,
           entry_value, entry_key, receiver);

    // Continue with the next entry.
    var_index.Bind(index);
    var_table.Bind(table);
    Goto(&loop);
  }

  BIND(&done_loop);
  args.PopAndReturn(UndefinedConstant());

  BIND(&callback_not_callable);
  {
    CallRuntime(Runtime::kThrowCalledNonCallable, context, callback);
    Unreachable();
  }
}

TF_BUILTIN(MapPrototypeKeys, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, "Map.prototype.keys");
  Return(AllocateJSCollectionIterator<JSMapIterator>(
      context, Context::MAP_KEY_ITERATOR_MAP_INDEX, receiver));
}

TF_BUILTIN(MapPrototypeValues, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE,
                         "Map.prototype.values");
  Return(AllocateJSCollectionIterator<JSMapIterator>(
      context, Context::MAP_VALUE_ITERATOR_MAP_INDEX, receiver));
}

TF_BUILTIN(MapIteratorPrototypeNext, CollectionsBuiltinsAssembler) {
  const char* const kMethodName = "Map Iterator.prototype.next";
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);

  // Ensure that the {receiver} is actually a JSMapIterator.
  Label if_receiver_valid(this), if_receiver_invalid(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(receiver), &if_receiver_invalid);
  Node* const receiver_instance_type = LoadInstanceType(receiver);
  GotoIf(
      InstanceTypeEqual(receiver_instance_type, JS_MAP_KEY_VALUE_ITERATOR_TYPE),
      &if_receiver_valid);
  GotoIf(InstanceTypeEqual(receiver_instance_type, JS_MAP_KEY_ITERATOR_TYPE),
         &if_receiver_valid);
  Branch(InstanceTypeEqual(receiver_instance_type, JS_MAP_VALUE_ITERATOR_TYPE),
         &if_receiver_valid, &if_receiver_invalid);
  BIND(&if_receiver_invalid);
  ThrowIncompatibleMethodReceiver(context, kMethodName, receiver);
  BIND(&if_receiver_valid);

  // Check if the {receiver} is exhausted.
  VARIABLE(var_done, MachineRepresentation::kTagged, TrueConstant());
  VARIABLE(var_value, MachineRepresentation::kTagged, UndefinedConstant());
  Label return_value(this, {&var_done, &var_value}), return_entry(this),
      return_end(this, Label::kDeferred);

  // Transition the {receiver} table if necessary.
  Node* table;
  Node* index;
  std::tie(table, index) =
      TransitionAndUpdate<JSMapIterator, OrderedHashMap>(receiver);

  // Read the next entry from the {table}, skipping holes.
  Node* entry_key;
  Node* entry_start_position;
  std::tie(entry_key, entry_start_position, index) =
      NextSkipHoles<OrderedHashMap>(table, index, &return_end);
  StoreObjectFieldNoWriteBarrier(receiver, JSMapIterator::kIndexOffset,
                                 SmiTag(index));
  var_value.Bind(entry_key);
  var_done.Bind(FalseConstant());

  // Check how to return the {key} (depending on {receiver} type).
  GotoIf(InstanceTypeEqual(receiver_instance_type, JS_MAP_KEY_ITERATOR_TYPE),
         &return_value);
  var_value.Bind(LoadFixedArrayElement(
      table, entry_start_position,
      (OrderedHashMap::kHashTableStartIndex + OrderedHashMap::kValueOffset) *
          kPointerSize));
  Branch(InstanceTypeEqual(receiver_instance_type, JS_MAP_VALUE_ITERATOR_TYPE),
         &return_value, &return_entry);

  BIND(&return_entry);
  {
    Node* result =
        AllocateJSIteratorResultForEntry(context, entry_key, var_value.value());
    Return(result);
  }

  BIND(&return_value);
  {
    Node* result =
        AllocateJSIteratorResult(context, var_value.value(), var_done.value());
    Return(result);
  }

  BIND(&return_end);
  {
    StoreObjectFieldRoot(receiver, JSMapIterator::kTableOffset,
                         Heap::kEmptyOrderedHashMapRootIndex);
    Goto(&return_value);
  }
}

TF_BUILTIN(SetPrototypeHas, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE, "Set.prototype.has");

  Node* const table = LoadObjectField(receiver, JSMap::kTableOffset);

  VARIABLE(entry_start_position, MachineType::PointerRepresentation(),
           IntPtrConstant(0));
  VARIABLE(result, MachineRepresentation::kTaggedSigned, IntPtrConstant(0));
  Label if_key_smi(this), if_key_string(this), if_key_heap_number(this),
      if_key_bigint(this), entry_found(this), not_found(this), done(this);

  GotoIf(TaggedIsSmi(key), &if_key_smi);

  Node* key_map = LoadMap(key);
  Node* key_instance_type = LoadMapInstanceType(key_map);

  GotoIf(IsStringInstanceType(key_instance_type), &if_key_string);
  GotoIf(IsHeapNumberMap(key_map), &if_key_heap_number);
  GotoIf(IsBigIntInstanceType(key_instance_type), &if_key_bigint);

  FindOrderedHashTableEntryForOtherKey<OrderedHashSet>(
      context, table, key, &entry_start_position, &entry_found, &not_found);

  BIND(&if_key_smi);
  {
    FindOrderedHashTableEntryForSmiKey<OrderedHashSet>(
        table, key, &entry_start_position, &entry_found, &not_found);
  }

  BIND(&if_key_string);
  {
    FindOrderedHashTableEntryForStringKey<OrderedHashSet>(
        context, table, key, &entry_start_position, &entry_found, &not_found);
  }

  BIND(&if_key_heap_number);
  {
    FindOrderedHashTableEntryForHeapNumberKey<OrderedHashSet>(
        context, table, key, &entry_start_position, &entry_found, &not_found);
  }

  BIND(&if_key_bigint);
  {
    FindOrderedHashTableEntryForBigIntKey<OrderedHashSet>(
        context, table, key, &entry_start_position, &entry_found, &not_found);
  }

  BIND(&entry_found);
  Return(TrueConstant());

  BIND(&not_found);
  Return(FalseConstant());
}

TF_BUILTIN(SetPrototypeEntries, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE,
                         "Set.prototype.entries");
  Return(AllocateJSCollectionIterator<JSSetIterator>(
      context, Context::SET_KEY_VALUE_ITERATOR_MAP_INDEX, receiver));
}

TF_BUILTIN(SetPrototypeGetSize, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE,
                         "get Set.prototype.size");
  Node* const table = LoadObjectField(receiver, JSSet::kTableOffset);
  Return(LoadObjectField(table, OrderedHashSet::kNumberOfElementsOffset));
}

TF_BUILTIN(SetPrototypeForEach, CollectionsBuiltinsAssembler) {
  const char* const kMethodName = "Set.prototype.forEach";
  Node* const argc = Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* const context = Parameter(BuiltinDescriptor::kContext);
  CodeStubArguments args(this, ChangeInt32ToIntPtr(argc));
  Node* const receiver = args.GetReceiver();
  Node* const callback = args.GetOptionalArgumentValue(0);
  Node* const this_arg = args.GetOptionalArgumentValue(1);

  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE, kMethodName);

  // Ensure that {callback} is actually callable.
  Label callback_not_callable(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(callback), &callback_not_callable);
  GotoIfNot(IsCallable(callback), &callback_not_callable);

  VARIABLE(var_index, MachineType::PointerRepresentation(), IntPtrConstant(0));
  VARIABLE(var_table, MachineRepresentation::kTagged,
           LoadObjectField(receiver, JSSet::kTableOffset));
  Label loop(this, {&var_index, &var_table}), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Transition {table} and {index} if there was any modification to
    // the {receiver} while we're iterating.
    Node* index = var_index.value();
    Node* table = var_table.value();
    std::tie(table, index) =
        Transition<OrderedHashSet>(table, index, [](Node*, Node*) {});

    // Read the next entry from the {table}, skipping holes.
    Node* entry_key;
    Node* entry_start_position;
    std::tie(entry_key, entry_start_position, index) =
        NextSkipHoles<OrderedHashSet>(table, index, &done_loop);

    // Invoke the {callback} passing the {entry_key} (twice) and the {receiver}.
    CallJS(CodeFactory::Call(isolate()), context, callback, this_arg, entry_key,
           entry_key, receiver);

    // Continue with the next entry.
    var_index.Bind(index);
    var_table.Bind(table);
    Goto(&loop);
  }

  BIND(&done_loop);
  args.PopAndReturn(UndefinedConstant());

  BIND(&callback_not_callable);
  {
    CallRuntime(Runtime::kThrowCalledNonCallable, context, callback);
    Unreachable();
  }
}

TF_BUILTIN(SetPrototypeValues, CollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE,
                         "Set.prototype.values");
  Return(AllocateJSCollectionIterator<JSSetIterator>(
      context, Context::SET_VALUE_ITERATOR_MAP_INDEX, receiver));
}

TF_BUILTIN(SetIteratorPrototypeNext, CollectionsBuiltinsAssembler) {
  const char* const kMethodName = "Set Iterator.prototype.next";
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const context = Parameter(Descriptor::kContext);

  // Ensure that the {receiver} is actually a JSSetIterator.
  Label if_receiver_valid(this), if_receiver_invalid(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(receiver), &if_receiver_invalid);
  Node* const receiver_instance_type = LoadInstanceType(receiver);
  GotoIf(InstanceTypeEqual(receiver_instance_type, JS_SET_VALUE_ITERATOR_TYPE),
         &if_receiver_valid);
  Branch(
      InstanceTypeEqual(receiver_instance_type, JS_SET_KEY_VALUE_ITERATOR_TYPE),
      &if_receiver_valid, &if_receiver_invalid);
  BIND(&if_receiver_invalid);
  ThrowIncompatibleMethodReceiver(context, kMethodName, receiver);
  BIND(&if_receiver_valid);

  // Check if the {receiver} is exhausted.
  VARIABLE(var_done, MachineRepresentation::kTagged, TrueConstant());
  VARIABLE(var_value, MachineRepresentation::kTagged, UndefinedConstant());
  Label return_value(this, {&var_done, &var_value}), return_entry(this),
      return_end(this, Label::kDeferred);

  // Transition the {receiver} table if necessary.
  Node* table;
  Node* index;
  std::tie(table, index) =
      TransitionAndUpdate<JSSetIterator, OrderedHashSet>(receiver);

  // Read the next entry from the {table}, skipping holes.
  Node* entry_key;
  Node* entry_start_position;
  std::tie(entry_key, entry_start_position, index) =
      NextSkipHoles<OrderedHashSet>(table, index, &return_end);
  StoreObjectFieldNoWriteBarrier(receiver, JSSetIterator::kIndexOffset,
                                 SmiTag(index));
  var_value.Bind(entry_key);
  var_done.Bind(FalseConstant());

  // Check how to return the {key} (depending on {receiver} type).
  Branch(InstanceTypeEqual(receiver_instance_type, JS_SET_VALUE_ITERATOR_TYPE),
         &return_value, &return_entry);

  BIND(&return_entry);
  {
    Node* result = AllocateJSIteratorResultForEntry(context, var_value.value(),
                                                    var_value.value());
    Return(result);
  }

  BIND(&return_value);
  {
    Node* result =
        AllocateJSIteratorResult(context, var_value.value(), var_done.value());
    Return(result);
  }

  BIND(&return_end);
  {
    StoreObjectFieldRoot(receiver, JSSetIterator::kTableOffset,
                         Heap::kEmptyOrderedHashSetRootIndex);
    Goto(&return_value);
  }
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::TryLookupOrderedHashTableIndex(
    Node* const table, Node* const key, Node* const context, Variable* result,
    Label* if_entry_found, Label* if_not_found) {
  Label if_key_smi(this), if_key_string(this), if_key_heap_number(this),
      if_key_bigint(this);

  GotoIf(TaggedIsSmi(key), &if_key_smi);

  Node* key_map = LoadMap(key);
  Node* key_instance_type = LoadMapInstanceType(key_map);

  GotoIf(IsStringInstanceType(key_instance_type), &if_key_string);
  GotoIf(IsHeapNumberMap(key_map), &if_key_heap_number);
  GotoIf(IsBigIntInstanceType(key_instance_type), &if_key_bigint);

  FindOrderedHashTableEntryForOtherKey<CollectionType>(
      context, table, key, result, if_entry_found, if_not_found);

  BIND(&if_key_smi);
  {
    FindOrderedHashTableEntryForSmiKey<CollectionType>(
        table, key, result, if_entry_found, if_not_found);
  }

  BIND(&if_key_string);
  {
    FindOrderedHashTableEntryForStringKey<CollectionType>(
        context, table, key, result, if_entry_found, if_not_found);
  }

  BIND(&if_key_heap_number);
  {
    FindOrderedHashTableEntryForHeapNumberKey<CollectionType>(
        context, table, key, result, if_entry_found, if_not_found);
  }

  BIND(&if_key_bigint);
  {
    FindOrderedHashTableEntryForBigIntKey<CollectionType>(
        context, table, key, result, if_entry_found, if_not_found);
  }
}

TF_BUILTIN(FindOrderedHashMapEntry, CollectionsBuiltinsAssembler) {
  Node* const table = Parameter(Descriptor::kTable);
  Node* const key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  VARIABLE(entry_start_position, MachineType::PointerRepresentation(),
           IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashMap>(
      table, key, context, &entry_start_position, &entry_found, &not_found);

  BIND(&entry_found);
  Return(SmiTag(entry_start_position.value()));

  BIND(&not_found);
  Return(SmiConstant(-1));
}

class WeakCollectionsBuiltinsAssembler : public BaseCollectionsAssembler {
 public:
  explicit WeakCollectionsBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : BaseCollectionsAssembler(state) {}

 protected:
  void AddEntry(TNode<Object> table, TNode<IntPtrT> key_index,
                TNode<Object> key, TNode<Object> value,
                TNode<IntPtrT> number_of_elements);

  TNode<Object> AllocateTable(Variant variant, TNode<Context> context,
                              TNode<IntPtrT> at_least_space_for);

  // Generates and sets the identity for a JSRececiver.
  TNode<Smi> CreateIdentityHash(TNode<Object> receiver);
  TNode<IntPtrT> EntryMask(TNode<IntPtrT> capacity);

  // Builds code that finds the ObjectHashTable entry for a {key} using the
  // comparison code generated by {key_compare}. The key index is returned if
  // the {key} is found.
  typedef std::function<void(TNode<Object> entry_key, Label* if_same)>
      KeyComparator;
  TNode<IntPtrT> FindKeyIndex(TNode<Object> table, TNode<IntPtrT> key_hash,
                              TNode<IntPtrT> entry_mask,
                              const KeyComparator& key_compare);

  // Builds code that finds an ObjectHashTable entry available for a new entry.
  TNode<IntPtrT> FindKeyIndexForInsertion(TNode<Object> table,
                                          TNode<IntPtrT> key_hash,
                                          TNode<IntPtrT> entry_mask);

  // Builds code that finds the ObjectHashTable entry with key that matches
  // {key} and returns the entry's key index. If {key} cannot be found, jumps to
  // {if_not_found}.
  TNode<IntPtrT> FindKeyIndexForKey(TNode<Object> table, TNode<Object> key,
                                    TNode<IntPtrT> hash,
                                    TNode<IntPtrT> entry_mask,
                                    Label* if_not_found);

  TNode<Word32T> InsufficientCapacityToAdd(TNode<IntPtrT> capacity,
                                           TNode<IntPtrT> number_of_elements,
                                           TNode<IntPtrT> number_of_deleted);
  TNode<IntPtrT> KeyIndexFromEntry(TNode<IntPtrT> entry);

  TNode<IntPtrT> LoadNumberOfElements(TNode<Object> table, int offset);
  TNode<IntPtrT> LoadNumberOfDeleted(TNode<Object> table, int offset = 0);
  TNode<Object> LoadTable(SloppyTNode<Object> collection);
  TNode<IntPtrT> LoadTableCapacity(TNode<Object> table);

  void RemoveEntry(TNode<Object> table, TNode<IntPtrT> key_index,
                   TNode<IntPtrT> number_of_elements);
  TNode<BoolT> ShouldRehash(TNode<IntPtrT> number_of_elements,
                            TNode<IntPtrT> number_of_deleted);
  TNode<Word32T> ShouldShrink(TNode<IntPtrT> capacity,
                              TNode<IntPtrT> number_of_elements);
  TNode<IntPtrT> ValueIndexFromKeyIndex(TNode<IntPtrT> key_index);
};

void WeakCollectionsBuiltinsAssembler::AddEntry(
    TNode<Object> table, TNode<IntPtrT> key_index, TNode<Object> key,
    TNode<Object> value, TNode<IntPtrT> number_of_elements) {
  // See ObjectHashTable::AddEntry().
  TNode<IntPtrT> value_index = ValueIndexFromKeyIndex(key_index);
  StoreFixedArrayElement(table, key_index, key);
  StoreFixedArrayElement(table, value_index, value);

  // See HashTableBase::ElementAdded().
  StoreFixedArrayElement(table, ObjectHashTable::kNumberOfElementsIndex,
                         SmiFromWord(number_of_elements), SKIP_WRITE_BARRIER);
}

TNode<Object> WeakCollectionsBuiltinsAssembler::AllocateTable(
    Variant variant, TNode<Context> context,
    TNode<IntPtrT> at_least_space_for) {
  // See HashTable::New().
  CSA_ASSERT(this,
             IntPtrLessThanOrEqual(IntPtrConstant(0), at_least_space_for));
  TNode<IntPtrT> capacity = HashTableComputeCapacity(at_least_space_for);

  // See HashTable::NewInternal().
  TNode<IntPtrT> length = KeyIndexFromEntry(capacity);
  TNode<Object> table = CAST(AllocateFixedArray(
      HOLEY_ELEMENTS, length, INTPTR_PARAMETERS, kAllowLargeObjectAllocation));

  Heap::RootListIndex map_root_index =
      static_cast<Heap::RootListIndex>(ObjectHashTableShape::GetMapRootIndex());
  StoreMapNoWriteBarrier(table, map_root_index);
  StoreFixedArrayElement(table, ObjectHashTable::kNumberOfElementsIndex,
                         SmiConstant(0), SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(table, ObjectHashTable::kNumberOfDeletedElementsIndex,
                         SmiConstant(0), SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(table, ObjectHashTable::kCapacityIndex,
                         SmiFromWord(capacity), SKIP_WRITE_BARRIER);

  TNode<IntPtrT> start = KeyIndexFromEntry(IntPtrConstant(0));
  FillFixedArrayWithValue(HOLEY_ELEMENTS, table, start, length,
                          Heap::kUndefinedValueRootIndex);
  return table;
}

TNode<Smi> WeakCollectionsBuiltinsAssembler::CreateIdentityHash(
    TNode<Object> key) {
  TNode<ExternalReference> function_addr = ExternalConstant(
      ExternalReference::jsreceiver_create_identity_hash(isolate()));
  TNode<ExternalReference> isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_tagged = MachineType::AnyTagged();

  return CAST(CallCFunction2(type_tagged, type_ptr, type_tagged, function_addr,
                             isolate_ptr, key));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::EntryMask(
    TNode<IntPtrT> capacity) {
  return IntPtrSub(capacity, IntPtrConstant(1));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::FindKeyIndex(
    TNode<Object> table, TNode<IntPtrT> key_hash, TNode<IntPtrT> entry_mask,
    const KeyComparator& key_compare) {
  // See HashTable::FirstProbe().
  TVARIABLE(IntPtrT, var_entry, WordAnd(key_hash, entry_mask));
  TVARIABLE(IntPtrT, var_count, IntPtrConstant(0));

  Variable* loop_vars[] = {&var_count, &var_entry};
  Label loop(this, arraysize(loop_vars), loop_vars), if_found(this);
  Goto(&loop);
  BIND(&loop);
  TNode<IntPtrT> key_index;
  {
    key_index = KeyIndexFromEntry(var_entry);
    TNode<Object> entry_key = CAST(LoadFixedArrayElement(table, key_index));

    key_compare(entry_key, &if_found);

    // See HashTable::NextProbe().
    Increment(&var_count);
    var_entry = WordAnd(IntPtrAdd(UncheckedCast<IntPtrT>(var_entry),
                                  UncheckedCast<IntPtrT>(var_count)),
                        entry_mask);
    Goto(&loop);
  }

  BIND(&if_found);
  return key_index;
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::FindKeyIndexForInsertion(
    TNode<Object> table, TNode<IntPtrT> key_hash, TNode<IntPtrT> entry_mask) {
  // See HashTable::FindInsertionEntry().
  auto is_not_live = [&](TNode<Object> entry_key, Label* if_found) {
    // This is the the negative form BaseShape::IsLive().
    GotoIf(Word32Or(IsTheHole(entry_key), IsUndefined(entry_key)), if_found);
  };
  return FindKeyIndex(table, key_hash, entry_mask, is_not_live);
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::FindKeyIndexForKey(
    TNode<Object> table, TNode<Object> key, TNode<IntPtrT> hash,
    TNode<IntPtrT> entry_mask, Label* if_not_found) {
  // See HashTable::FindEntry().
  auto match_key_or_exit_on_empty = [&](TNode<Object> entry_key,
                                        Label* if_same) {
    GotoIf(IsUndefined(entry_key), if_not_found);
    GotoIf(WordEqual(entry_key, key), if_same);
  };
  return FindKeyIndex(table, hash, entry_mask, match_key_or_exit_on_empty);
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::KeyIndexFromEntry(
    TNode<IntPtrT> entry) {
  // See HashTable::KeyAt().
  // (entry * kEntrySize) + kElementsStartIndex + kEntryKeyIndex
  return IntPtrAdd(
      IntPtrMul(entry, IntPtrConstant(ObjectHashTable::kEntrySize)),
      IntPtrConstant(ObjectHashTable::kElementsStartIndex +
                     ObjectHashTable::kEntryKeyIndex));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::LoadNumberOfElements(
    TNode<Object> table, int offset) {
  TNode<IntPtrT> number_of_elements = SmiUntag(
      LoadFixedArrayElement(table, ObjectHashTable::kNumberOfElementsIndex));
  return IntPtrAdd(number_of_elements, IntPtrConstant(offset));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::LoadNumberOfDeleted(
    TNode<Object> table, int offset) {
  TNode<IntPtrT> number_of_deleted = SmiUntag(LoadFixedArrayElement(
      table, ObjectHashTable::kNumberOfDeletedElementsIndex));
  return IntPtrAdd(number_of_deleted, IntPtrConstant(offset));
}

TNode<Object> WeakCollectionsBuiltinsAssembler::LoadTable(
    SloppyTNode<Object> collection) {
  return LoadObjectField(CAST(collection), JSWeakCollection::kTableOffset);
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::LoadTableCapacity(
    TNode<Object> table) {
  return SmiUntag(
      LoadFixedArrayElement(table, ObjectHashTable::kCapacityIndex));
}

TNode<Word32T> WeakCollectionsBuiltinsAssembler::InsufficientCapacityToAdd(
    TNode<IntPtrT> capacity, TNode<IntPtrT> number_of_elements,
    TNode<IntPtrT> number_of_deleted) {
  // This is the negative form of HashTable::HasSufficientCapacityToAdd().
  // Return true if:
  //   - more than 50% of the available space are deleted elements
  //   - less than 50% will be available
  TNode<IntPtrT> available = IntPtrSub(capacity, number_of_elements);
  TNode<IntPtrT> half_available = WordShr(available, 1);
  TNode<IntPtrT> needed_available = WordShr(number_of_elements, 1);
  return Word32Or(
      // deleted > half
      IntPtrGreaterThan(number_of_deleted, half_available),
      // elements + needed available > capacity
      IntPtrGreaterThan(IntPtrAdd(number_of_elements, needed_available),
                        capacity));
}

void WeakCollectionsBuiltinsAssembler::RemoveEntry(
    TNode<Object> table, TNode<IntPtrT> key_index,
    TNode<IntPtrT> number_of_elements) {
  // See ObjectHashTable::RemoveEntry().
  TNode<IntPtrT> value_index = ValueIndexFromKeyIndex(key_index);
  StoreFixedArrayElement(table, key_index, TheHoleConstant());
  StoreFixedArrayElement(table, value_index, TheHoleConstant());

  // See HashTableBase::ElementRemoved().
  TNode<IntPtrT> number_of_deleted = LoadNumberOfDeleted(table, 1);
  StoreFixedArrayElement(table, ObjectHashTable::kNumberOfElementsIndex,
                         SmiFromWord(number_of_elements), SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(table, ObjectHashTable::kNumberOfDeletedElementsIndex,
                         SmiFromWord(number_of_deleted), SKIP_WRITE_BARRIER);
}

TNode<BoolT> WeakCollectionsBuiltinsAssembler::ShouldRehash(
    TNode<IntPtrT> number_of_elements, TNode<IntPtrT> number_of_deleted) {
  // Rehash if more than 33% of the entries are deleted.
  return IntPtrGreaterThanOrEqual(WordShl(number_of_deleted, 1),
                                  number_of_elements);
}

TNode<Word32T> WeakCollectionsBuiltinsAssembler::ShouldShrink(
    TNode<IntPtrT> capacity, TNode<IntPtrT> number_of_elements) {
  // See HashTable::Shrink().
  TNode<IntPtrT> quarter_capacity = WordShr(capacity, 2);
  return Word32And(
      // Shrink to fit the number of elements if only a quarter of the
      // capacity is filled with elements.
      IntPtrLessThanOrEqual(number_of_elements, quarter_capacity),

      // Allocate a new dictionary with room for at least the current
      // number of elements. The allocation method will make sure that
      // there is extra room in the dictionary for additions. Don't go
      // lower than room for 16 elements.
      IntPtrGreaterThanOrEqual(number_of_elements, IntPtrConstant(16)));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::ValueIndexFromKeyIndex(
    TNode<IntPtrT> key_index) {
  return IntPtrAdd(key_index,
                   IntPtrConstant(ObjectHashTableShape::kEntryValueIndex -
                                  ObjectHashTable::kEntryKeyIndex));
}

TF_BUILTIN(WeakMapConstructor, WeakCollectionsBuiltinsAssembler) {
  GenerateConstructor(kMap, Context::JS_WEAK_MAP_FUN_INDEX,
                      isolate()->factory()->WeakMap_string(),
                      JSWeakMap::kTableOffset);
}

TF_BUILTIN(WeakSetConstructor, WeakCollectionsBuiltinsAssembler) {
  GenerateConstructor(kSet, Context::JS_WEAK_SET_FUN_INDEX,
                      isolate()->factory()->WeakSet_string(),
                      JSWeakSet::kTableOffset);
}

TF_BUILTIN(WeakMapLookupHashIndex, WeakCollectionsBuiltinsAssembler) {
  TNode<Object> table = CAST(Parameter(Descriptor::kTable));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));

  Label if_not_found(this);

  GotoIfNotJSReceiver(key, &if_not_found);

  TNode<IntPtrT> hash = LoadJSReceiverIdentityHash(key, &if_not_found);
  TNode<IntPtrT> capacity = LoadTableCapacity(table);
  TNode<IntPtrT> key_index =
      FindKeyIndexForKey(table, key, hash, EntryMask(capacity), &if_not_found);
  Return(SmiTag(ValueIndexFromKeyIndex(key_index)));

  BIND(&if_not_found);
  Return(SmiConstant(-1));
}

TF_BUILTIN(WeakMapGet, WeakCollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  Label return_undefined(this);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_MAP_TYPE,
                         "WeakMap.prototype.get");

  Node* const table = LoadTable(receiver);
  Node* const index =
      CallBuiltin(Builtins::kWeakMapLookupHashIndex, context, table, key);

  GotoIf(WordEqual(index, SmiConstant(-1)), &return_undefined);

  Return(LoadFixedArrayElement(table, SmiUntag(index)));

  BIND(&return_undefined);
  Return(UndefinedConstant());
}

TF_BUILTIN(WeakMapHas, WeakCollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  Label return_false(this);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_MAP_TYPE,
                         "WeakMap.prototype.has");

  Node* const table = LoadTable(receiver);
  Node* const index =
      CallBuiltin(Builtins::kWeakMapLookupHashIndex, context, table, key);

  GotoIf(WordEqual(index, SmiConstant(-1)), &return_false);

  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

// Helper that removes the entry with a given key from the backing store
// (ObjectHashTable) of a WeakMap or WeakSet.
TF_BUILTIN(WeakCollectionDelete, WeakCollectionsBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> collection = CAST(Parameter(Descriptor::kCollection));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));

  Label call_runtime(this), if_not_found(this);

  GotoIfNotJSReceiver(key, &if_not_found);

  TNode<IntPtrT> hash = LoadJSReceiverIdentityHash(key, &if_not_found);
  TNode<Object> table = LoadTable(collection);
  TNode<IntPtrT> capacity = LoadTableCapacity(table);
  TNode<IntPtrT> key_index =
      FindKeyIndexForKey(table, key, hash, EntryMask(capacity), &if_not_found);
  TNode<IntPtrT> number_of_elements = LoadNumberOfElements(table, -1);
  GotoIf(ShouldShrink(capacity, number_of_elements), &call_runtime);

  RemoveEntry(table, key_index, number_of_elements);
  Return(TrueConstant());

  BIND(&if_not_found);
  Return(FalseConstant());

  BIND(&call_runtime);
  Return(CallRuntime(Runtime::kWeakCollectionDelete, context, collection, key,
                     SmiTag(hash)));
}

// Helper that sets the key and value to the backing store (ObjectHashTable) of
// a WeakMap or WeakSet.
TF_BUILTIN(WeakCollectionSet, WeakCollectionsBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> collection = CAST(Parameter(Descriptor::kCollection));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));

  CSA_ASSERT(this, IsJSReceiver(key));

  Label call_runtime(this), if_no_hash(this), if_not_found(this);

  TNode<Object> table = LoadTable(collection);
  TNode<IntPtrT> capacity = LoadTableCapacity(table);
  TNode<IntPtrT> entry_mask = EntryMask(capacity);

  TVARIABLE(IntPtrT, var_hash, LoadJSReceiverIdentityHash(key, &if_no_hash));
  TNode<IntPtrT> key_index =
      FindKeyIndexForKey(table, key, var_hash, entry_mask, &if_not_found);

  StoreFixedArrayElement(table, ValueIndexFromKeyIndex(key_index), value);
  Return(collection);

  BIND(&if_no_hash);
  {
    var_hash = SmiUntag(CreateIdentityHash(key));
    Goto(&if_not_found);
  }
  BIND(&if_not_found);
  {
    TNode<IntPtrT> number_of_deleted = LoadNumberOfDeleted(table);
    TNode<IntPtrT> number_of_elements = LoadNumberOfElements(table, 1);

    // TODO(pwong): Port HashTable's Rehash() and EnsureCapacity() to CSA.
    GotoIf(Word32Or(ShouldRehash(number_of_elements, number_of_deleted),
                    InsufficientCapacityToAdd(capacity, number_of_elements,
                                              number_of_deleted)),
           &call_runtime);

    TNode<IntPtrT> insertion_key_index =
        FindKeyIndexForInsertion(table, var_hash, entry_mask);
    AddEntry(table, insertion_key_index, key, value, number_of_elements);
    Return(collection);
  }
  BIND(&call_runtime);
  {
    CallRuntime(Runtime::kWeakCollectionSet, context, collection, key, value,
                SmiTag(var_hash));
    Return(collection);
  }
}

TF_BUILTIN(WeakMapPrototypeDelete, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_MAP_TYPE,
                         "WeakMap.prototype.delete");

  Return(CallBuiltin(Builtins::kWeakCollectionDelete, context, receiver, key));
}

TF_BUILTIN(WeakMapPrototypeSet, WeakCollectionsBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_MAP_TYPE,
                         "WeakMap.prototype.set");

  Label throw_invalid_key(this);
  GotoIfNotJSReceiver(key, &throw_invalid_key);

  Return(
      CallBuiltin(Builtins::kWeakCollectionSet, context, receiver, key, value));

  BIND(&throw_invalid_key);
  ThrowTypeError(context, MessageTemplate::kInvalidWeakMapKey, key);
}

TF_BUILTIN(WeakSetPrototypeAdd, WeakCollectionsBuiltinsAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_SET_TYPE,
                         "WeakSet.prototype.add");

  Label throw_invalid_value(this);
  GotoIfNotJSReceiver(value, &throw_invalid_value);

  Return(CallBuiltin(Builtins::kWeakCollectionSet, context, receiver, value,
                     TrueConstant()));

  BIND(&throw_invalid_value);
  ThrowTypeError(context, MessageTemplate::kInvalidWeakSetValue, value);
}

TF_BUILTIN(WeakSetPrototypeDelete, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_SET_TYPE,
                         "WeakSet.prototype.delete");

  Return(
      CallBuiltin(Builtins::kWeakCollectionDelete, context, receiver, value));
}

TF_BUILTIN(WeakSetHas, WeakCollectionsBuiltinsAssembler) {
  Node* const receiver = Parameter(Descriptor::kReceiver);
  Node* const key = Parameter(Descriptor::kKey);
  Node* const context = Parameter(Descriptor::kContext);

  Label return_false(this);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_SET_TYPE,
                         "WeakSet.prototype.has");

  Node* const table = LoadTable(receiver);
  Node* const index =
      CallBuiltin(Builtins::kWeakMapLookupHashIndex, context, table, key);

  GotoIf(WordEqual(index, SmiConstant(-1)), &return_false);

  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

}  // namespace internal
}  // namespace v8
