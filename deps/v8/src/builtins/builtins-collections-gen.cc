// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-collections-gen.h"

#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-iterator-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/execution/protectors.h"
#include "src/heap/factory-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-collection.h"
#include "src/objects/ordered-hash-table.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

template <class T>
using TVariable = compiler::TypedCodeAssemblerVariable<T>;

class BaseCollectionsAssembler : public CodeStubAssembler {
 public:
  explicit BaseCollectionsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  virtual ~BaseCollectionsAssembler() = default;

 protected:
  enum Variant { kMap, kSet, kWeakMap, kWeakSet };

  // Adds an entry to a collection.  For Maps, properly handles extracting the
  // key and value from the entry (see LoadKeyValue()).
  void AddConstructorEntry(Variant variant, TNode<Context> context,
                           TNode<Object> collection, TNode<Object> add_function,
                           TNode<Object> key_value,
                           Label* if_may_have_side_effects = nullptr,
                           Label* if_exception = nullptr,
                           TVariable<Object>* var_exception = nullptr);

  // Adds constructor entries to a collection.  Choosing a fast path when
  // possible.
  void AddConstructorEntries(Variant variant, TNode<Context> context,
                             TNode<Context> native_context,
                             TNode<HeapObject> collection,
                             TNode<Object> initial_entries);

  // Fast path for adding constructor entries.  Assumes the entries are a fast
  // JS array (see CodeStubAssembler::BranchIfFastJSArray()).
  void AddConstructorEntriesFromFastJSArray(Variant variant,
                                            TNode<Context> context,
                                            TNode<Context> native_context,
                                            TNode<Object> collection,
                                            TNode<JSArray> fast_jsarray,
                                            Label* if_may_have_side_effects);

  // Adds constructor entries to a collection using the iterator protocol.
  void AddConstructorEntriesFromIterable(Variant variant,
                                         TNode<Context> context,
                                         TNode<Context> native_context,
                                         TNode<Object> collection,
                                         TNode<Object> iterable);

  // Constructs a collection instance. Choosing a fast path when possible.
  TNode<JSObject> AllocateJSCollection(TNode<Context> context,
                                       TNode<JSFunction> constructor,
                                       TNode<JSReceiver> new_target);

  // Fast path for constructing a collection instance if the constructor
  // function has not been modified.
  TNode<JSObject> AllocateJSCollectionFast(TNode<JSFunction> constructor);

  // Fallback for constructing a collection instance if the constructor function
  // has been modified.
  TNode<JSObject> AllocateJSCollectionSlow(TNode<Context> context,
                                           TNode<JSFunction> constructor,
                                           TNode<JSReceiver> new_target);

  // Allocates the backing store for a collection.
  virtual TNode<HeapObject> AllocateTable(
      Variant variant, TNode<IntPtrT> at_least_space_for) = 0;

  // Main entry point for a collection constructor builtin.
  void GenerateConstructor(Variant variant,
                           Handle<String> constructor_function_name,
                           TNode<Object> new_target, TNode<IntPtrT> argc,
                           TNode<Context> context);

  // Retrieves the collection function that adds an entry. `set` for Maps and
  // `add` for Sets.
  TNode<Object> GetAddFunction(Variant variant, TNode<Context> context,
                               TNode<Object> collection);

  // Retrieves the collection constructor function.
  TNode<JSFunction> GetConstructor(Variant variant,
                                   TNode<Context> native_context);

  // Retrieves the initial collection function that adds an entry. Should only
  // be called when it is certain that a collection prototype's map hasn't been
  // changed.
  TNode<JSFunction> GetInitialAddFunction(Variant variant,
                                          TNode<Context> native_context);

  // Checks whether {collection}'s initial add/set function has been modified
  // (depending on {variant}, loaded from {native_context}).
  void GotoIfInitialAddFunctionModified(Variant variant,
                                        TNode<NativeContext> native_context,
                                        TNode<HeapObject> collection,
                                        Label* if_modified);

  // Gets root index for the name of the add/set function.
  RootIndex GetAddFunctionNameIndex(Variant variant);

  // Retrieves the offset to access the backing table from the collection.
  int GetTableOffset(Variant variant);

  // Estimates the number of entries the collection will have after adding the
  // entries passed in the constructor. AllocateTable() can use this to avoid
  // the time of growing/rehashing when adding the constructor entries.
  TNode<IntPtrT> EstimatedInitialSize(TNode<Object> initial_entries,
                                      TNode<BoolT> is_fast_jsarray);

  void GotoIfNotJSReceiver(const TNode<Object> obj, Label* if_not_receiver);

  // Determines whether the collection's prototype has been modified.
  TNode<BoolT> HasInitialCollectionPrototype(Variant variant,
                                             TNode<Context> native_context,
                                             TNode<Object> collection);

  // Gets the initial prototype map for given collection {variant}.
  TNode<Map> GetInitialCollectionPrototype(Variant variant,
                                           TNode<Context> native_context);

  // Loads an element from a fixed array.  If the element is the hole, returns
  // `undefined`.
  TNode<Object> LoadAndNormalizeFixedArrayElement(TNode<FixedArray> elements,
                                                  TNode<IntPtrT> index);

  // Loads an element from a fixed double array.  If the element is the hole,
  // returns `undefined`.
  TNode<Object> LoadAndNormalizeFixedDoubleArrayElement(
      TNode<HeapObject> elements, TNode<IntPtrT> index);
};

void BaseCollectionsAssembler::AddConstructorEntry(
    Variant variant, TNode<Context> context, TNode<Object> collection,
    TNode<Object> add_function, TNode<Object> key_value,
    Label* if_may_have_side_effects, Label* if_exception,
    TVariable<Object>* var_exception) {
  compiler::ScopedExceptionHandler handler(this, if_exception, var_exception);
  CSA_ASSERT(this, Word32BinaryNot(IsTheHole(key_value)));
  if (variant == kMap || variant == kWeakMap) {
    TorqueStructKeyValuePair pair =
        if_may_have_side_effects != nullptr
            ? LoadKeyValuePairNoSideEffects(context, key_value,
                                            if_may_have_side_effects)
            : LoadKeyValuePair(context, key_value);
    TNode<Object> key_n = pair.key;
    TNode<Object> value_n = pair.value;
    Call(context, add_function, collection, key_n, value_n);
  } else {
    DCHECK(variant == kSet || variant == kWeakSet);
    Call(context, add_function, collection, key_value);
  }
}

void BaseCollectionsAssembler::AddConstructorEntries(
    Variant variant, TNode<Context> context, TNode<Context> native_context,
    TNode<HeapObject> collection, TNode<Object> initial_entries) {
  TVARIABLE(BoolT, use_fast_loop,
            IsFastJSArrayWithNoCustomIteration(context, initial_entries));
  TNode<IntPtrT> at_least_space_for =
      EstimatedInitialSize(initial_entries, use_fast_loop.value());
  Label allocate_table(this, &use_fast_loop), exit(this), fast_loop(this),
      slow_loop(this, Label::kDeferred);
  Goto(&allocate_table);
  BIND(&allocate_table);
  {
    TNode<HeapObject> table = AllocateTable(variant, at_least_space_for);
    StoreObjectField(collection, GetTableOffset(variant), table);
    GotoIf(IsNullOrUndefined(initial_entries), &exit);
    GotoIfInitialAddFunctionModified(variant, CAST(native_context), collection,
                                     &slow_loop);
    Branch(use_fast_loop.value(), &fast_loop, &slow_loop);
  }
  BIND(&fast_loop);
  {
    TNode<JSArray> initial_entries_jsarray =
        UncheckedCast<JSArray>(initial_entries);
#if DEBUG
    CSA_ASSERT(this, IsFastJSArrayWithNoCustomIteration(
                         context, initial_entries_jsarray));
    TNode<Map> original_initial_entries_map = LoadMap(initial_entries_jsarray);
#endif

    Label if_may_have_side_effects(this, Label::kDeferred);
    AddConstructorEntriesFromFastJSArray(variant, context, native_context,
                                         collection, initial_entries_jsarray,
                                         &if_may_have_side_effects);
    Goto(&exit);

    if (variant == kMap || variant == kWeakMap) {
      BIND(&if_may_have_side_effects);
#if DEBUG
      {
        // Check that add/set function has not been modified.
        Label if_not_modified(this), if_modified(this);
        GotoIfInitialAddFunctionModified(variant, CAST(native_context),
                                         collection, &if_modified);
        Goto(&if_not_modified);
        BIND(&if_modified);
        Unreachable();
        BIND(&if_not_modified);
      }
      CSA_ASSERT(this, TaggedEqual(original_initial_entries_map,
                                   LoadMap(initial_entries_jsarray)));
#endif
      use_fast_loop = Int32FalseConstant();
      Goto(&allocate_table);
    }
  }
  BIND(&slow_loop);
  {
    AddConstructorEntriesFromIterable(variant, context, native_context,
                                      collection, initial_entries);
    Goto(&exit);
  }
  BIND(&exit);
}

void BaseCollectionsAssembler::AddConstructorEntriesFromFastJSArray(
    Variant variant, TNode<Context> context, TNode<Context> native_context,
    TNode<Object> collection, TNode<JSArray> fast_jsarray,
    Label* if_may_have_side_effects) {
  TNode<FixedArrayBase> elements = LoadElements(fast_jsarray);
  TNode<Int32T> elements_kind = LoadElementsKind(fast_jsarray);
  TNode<JSFunction> add_func = GetInitialAddFunction(variant, native_context);
  CSA_ASSERT(this,
             TaggedEqual(GetAddFunction(variant, native_context, collection),
                         add_func));
  CSA_ASSERT(this, IsFastJSArrayWithNoCustomIteration(context, fast_jsarray));
  TNode<IntPtrT> length = SmiUntag(LoadFastJSArrayLength(fast_jsarray));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(length, IntPtrConstant(0)));
  CSA_ASSERT(
      this, HasInitialCollectionPrototype(variant, native_context, collection));

#if DEBUG
  TNode<Map> original_collection_map = LoadMap(CAST(collection));
  TNode<Map> original_fast_js_array_map = LoadMap(fast_jsarray);
#endif
  Label exit(this), if_doubles(this), if_smiorobjects(this);
  GotoIf(IntPtrEqual(length, IntPtrConstant(0)), &exit);
  Branch(IsFastSmiOrTaggedElementsKind(elements_kind), &if_smiorobjects,
         &if_doubles);
  BIND(&if_smiorobjects);
  {
    auto set_entry = [&](TNode<IntPtrT> index) {
      TNode<Object> element = LoadAndNormalizeFixedArrayElement(
          CAST(elements), UncheckedCast<IntPtrT>(index));
      AddConstructorEntry(variant, context, collection, add_func, element,
                          if_may_have_side_effects);
    };

    // Instead of using the slower iteration protocol to iterate over the
    // elements, a fast loop is used.  This assumes that adding an element
    // to the collection does not call user code that could mutate the elements
    // or collection.
    BuildFastLoop<IntPtrT>(IntPtrConstant(0), length, set_entry, 1,
                           IndexAdvanceMode::kPost);
    Goto(&exit);
  }
  BIND(&if_doubles);
  {
    // A Map constructor requires entries to be arrays (ex. [key, value]),
    // so a FixedDoubleArray can never succeed.
    if (variant == kMap || variant == kWeakMap) {
      CSA_ASSERT(this, IntPtrGreaterThan(length, IntPtrConstant(0)));
      TNode<Object> element =
          LoadAndNormalizeFixedDoubleArrayElement(elements, IntPtrConstant(0));
      ThrowTypeError(context, MessageTemplate::kIteratorValueNotAnObject,
                     element);
    } else {
      DCHECK(variant == kSet || variant == kWeakSet);
      auto set_entry = [&](TNode<IntPtrT> index) {
        TNode<Object> entry = LoadAndNormalizeFixedDoubleArrayElement(
            elements, UncheckedCast<IntPtrT>(index));
        AddConstructorEntry(variant, context, collection, add_func, entry);
      };
      BuildFastLoop<IntPtrT>(IntPtrConstant(0), length, set_entry, 1,
                             IndexAdvanceMode::kPost);
      Goto(&exit);
    }
  }
  BIND(&exit);
#if DEBUG
  CSA_ASSERT(this,
             TaggedEqual(original_collection_map, LoadMap(CAST(collection))));
  CSA_ASSERT(this,
             TaggedEqual(original_fast_js_array_map, LoadMap(fast_jsarray)));
#endif
}

void BaseCollectionsAssembler::AddConstructorEntriesFromIterable(
    Variant variant, TNode<Context> context, TNode<Context> native_context,
    TNode<Object> collection, TNode<Object> iterable) {
  Label exit(this), loop(this), if_exception(this, Label::kDeferred);
  CSA_ASSERT(this, Word32BinaryNot(IsNullOrUndefined(iterable)));

  TNode<Object> add_func = GetAddFunction(variant, context, collection);
  IteratorBuiltinsAssembler iterator_assembler(this->state());
  TorqueStructIteratorRecord iterator =
      iterator_assembler.GetIterator(context, iterable);

  CSA_ASSERT(this, Word32BinaryNot(IsUndefined(iterator.object)));

  TNode<Map> fast_iterator_result_map = CAST(
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX));
  TVARIABLE(Object, var_exception);

  Goto(&loop);
  BIND(&loop);
  {
    TNode<JSReceiver> next = iterator_assembler.IteratorStep(
        context, iterator, &exit, fast_iterator_result_map);
    TNode<Object> next_value = iterator_assembler.IteratorValue(
        context, next, fast_iterator_result_map);
    AddConstructorEntry(variant, context, collection, add_func, next_value,
                        nullptr, &if_exception, &var_exception);
    Goto(&loop);
  }
  BIND(&if_exception);
  {
    IteratorCloseOnException(context, iterator);
    CallRuntime(Runtime::kReThrow, context, var_exception.value());
    Unreachable();
  }
  BIND(&exit);
}

RootIndex BaseCollectionsAssembler::GetAddFunctionNameIndex(Variant variant) {
  switch (variant) {
    case kMap:
    case kWeakMap:
      return RootIndex::kset_string;
    case kSet:
    case kWeakSet:
      return RootIndex::kadd_string;
  }
  UNREACHABLE();
}

void BaseCollectionsAssembler::GotoIfInitialAddFunctionModified(
    Variant variant, TNode<NativeContext> native_context,
    TNode<HeapObject> collection, Label* if_modified) {
  STATIC_ASSERT(JSCollection::kAddFunctionDescriptorIndex ==
                JSWeakCollection::kAddFunctionDescriptorIndex);

  // TODO(jgruber): Investigate if this should also fall back to full prototype
  // verification.
  static constexpr PrototypeCheckAssembler::Flags flags{
      PrototypeCheckAssembler::kCheckPrototypePropertyConstness};

  static constexpr int kNoContextIndex = -1;
  STATIC_ASSERT(
      (flags & PrototypeCheckAssembler::kCheckPrototypePropertyIdentity) == 0);

  using DescriptorIndexNameValue =
      PrototypeCheckAssembler::DescriptorIndexNameValue;

  DescriptorIndexNameValue property_to_check{
      JSCollection::kAddFunctionDescriptorIndex,
      GetAddFunctionNameIndex(variant), kNoContextIndex};

  PrototypeCheckAssembler prototype_check_assembler(
      state(), flags, native_context,
      GetInitialCollectionPrototype(variant, native_context),
      base::Vector<DescriptorIndexNameValue>(&property_to_check, 1));

  TNode<HeapObject> prototype = LoadMapPrototype(LoadMap(collection));
  Label if_unmodified(this);
  prototype_check_assembler.CheckAndBranch(prototype, &if_unmodified,
                                           if_modified);

  BIND(&if_unmodified);
}

TNode<JSObject> BaseCollectionsAssembler::AllocateJSCollection(
    TNode<Context> context, TNode<JSFunction> constructor,
    TNode<JSReceiver> new_target) {
  TNode<BoolT> is_target_unmodified = TaggedEqual(constructor, new_target);

  return Select<JSObject>(
      is_target_unmodified,
      [=] { return AllocateJSCollectionFast(constructor); },
      [=] {
        return AllocateJSCollectionSlow(context, constructor, new_target);
      });
}

TNode<JSObject> BaseCollectionsAssembler::AllocateJSCollectionFast(
    TNode<JSFunction> constructor) {
  CSA_ASSERT(this, IsConstructorMap(LoadMap(constructor)));
  TNode<Map> initial_map =
      CAST(LoadJSFunctionPrototypeOrInitialMap(constructor));
  return AllocateJSObjectFromMap(initial_map);
}

TNode<JSObject> BaseCollectionsAssembler::AllocateJSCollectionSlow(
    TNode<Context> context, TNode<JSFunction> constructor,
    TNode<JSReceiver> new_target) {
  ConstructorBuiltinsAssembler constructor_assembler(this->state());
  return constructor_assembler.FastNewObject(context, constructor, new_target);
}

void BaseCollectionsAssembler::GenerateConstructor(
    Variant variant, Handle<String> constructor_function_name,
    TNode<Object> new_target, TNode<IntPtrT> argc, TNode<Context> context) {
  const int kIterableArg = 0;
  CodeStubArguments args(this, argc);
  TNode<Object> iterable = args.GetOptionalArgumentValue(kIterableArg);

  Label if_undefined(this, Label::kDeferred);
  GotoIf(IsUndefined(new_target), &if_undefined);

  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<JSObject> collection = AllocateJSCollection(
      context, GetConstructor(variant, native_context), CAST(new_target));

  AddConstructorEntries(variant, context, native_context, collection, iterable);
  Return(collection);

  BIND(&if_undefined);
  ThrowTypeError(context, MessageTemplate::kConstructorNotFunction,
                 HeapConstant(constructor_function_name));
}

TNode<Object> BaseCollectionsAssembler::GetAddFunction(
    Variant variant, TNode<Context> context, TNode<Object> collection) {
  Handle<String> add_func_name = (variant == kMap || variant == kWeakMap)
                                     ? isolate()->factory()->set_string()
                                     : isolate()->factory()->add_string();
  TNode<Object> add_func = GetProperty(context, collection, add_func_name);

  Label exit(this), if_notcallable(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(add_func), &if_notcallable);
  GotoIfNot(IsCallable(CAST(add_func)), &if_notcallable);
  Goto(&exit);

  BIND(&if_notcallable);
  ThrowTypeError(context, MessageTemplate::kPropertyNotFunction, add_func,
                 HeapConstant(add_func_name), collection);

  BIND(&exit);
  return add_func;
}

TNode<JSFunction> BaseCollectionsAssembler::GetConstructor(
    Variant variant, TNode<Context> native_context) {
  int index;
  switch (variant) {
    case kMap:
      index = Context::JS_MAP_FUN_INDEX;
      break;
    case kSet:
      index = Context::JS_SET_FUN_INDEX;
      break;
    case kWeakMap:
      index = Context::JS_WEAK_MAP_FUN_INDEX;
      break;
    case kWeakSet:
      index = Context::JS_WEAK_SET_FUN_INDEX;
      break;
  }
  return CAST(LoadContextElement(native_context, index));
}

TNode<JSFunction> BaseCollectionsAssembler::GetInitialAddFunction(
    Variant variant, TNode<Context> native_context) {
  int index;
  switch (variant) {
    case kMap:
      index = Context::MAP_SET_INDEX;
      break;
    case kSet:
      index = Context::SET_ADD_INDEX;
      break;
    case kWeakMap:
      index = Context::WEAKMAP_SET_INDEX;
      break;
    case kWeakSet:
      index = Context::WEAKSET_ADD_INDEX;
      break;
  }
  return CAST(LoadContextElement(native_context, index));
}

int BaseCollectionsAssembler::GetTableOffset(Variant variant) {
  switch (variant) {
    case kMap:
      return JSMap::kTableOffset;
    case kSet:
      return JSSet::kTableOffset;
    case kWeakMap:
      return JSWeakMap::kTableOffset;
    case kWeakSet:
      return JSWeakSet::kTableOffset;
  }
  UNREACHABLE();
}

TNode<IntPtrT> BaseCollectionsAssembler::EstimatedInitialSize(
    TNode<Object> initial_entries, TNode<BoolT> is_fast_jsarray) {
  return Select<IntPtrT>(
      is_fast_jsarray,
      [=] { return SmiUntag(LoadFastJSArrayLength(CAST(initial_entries))); },
      [=] { return IntPtrConstant(0); });
}

void BaseCollectionsAssembler::GotoIfNotJSReceiver(const TNode<Object> obj,
                                                   Label* if_not_receiver) {
  GotoIf(TaggedIsSmi(obj), if_not_receiver);
  GotoIfNot(IsJSReceiver(CAST(obj)), if_not_receiver);
}

TNode<Map> BaseCollectionsAssembler::GetInitialCollectionPrototype(
    Variant variant, TNode<Context> native_context) {
  int initial_prototype_index;
  switch (variant) {
    case kMap:
      initial_prototype_index = Context::INITIAL_MAP_PROTOTYPE_MAP_INDEX;
      break;
    case kSet:
      initial_prototype_index = Context::INITIAL_SET_PROTOTYPE_MAP_INDEX;
      break;
    case kWeakMap:
      initial_prototype_index = Context::INITIAL_WEAKMAP_PROTOTYPE_MAP_INDEX;
      break;
    case kWeakSet:
      initial_prototype_index = Context::INITIAL_WEAKSET_PROTOTYPE_MAP_INDEX;
      break;
  }
  return CAST(LoadContextElement(native_context, initial_prototype_index));
}

TNode<BoolT> BaseCollectionsAssembler::HasInitialCollectionPrototype(
    Variant variant, TNode<Context> native_context, TNode<Object> collection) {
  TNode<Map> collection_proto_map =
      LoadMap(LoadMapPrototype(LoadMap(CAST(collection))));

  return TaggedEqual(collection_proto_map,
                     GetInitialCollectionPrototype(variant, native_context));
}

TNode<Object> BaseCollectionsAssembler::LoadAndNormalizeFixedArrayElement(
    TNode<FixedArray> elements, TNode<IntPtrT> index) {
  TNode<Object> element = UnsafeLoadFixedArrayElement(elements, index);
  return Select<Object>(IsTheHole(element), [=] { return UndefinedConstant(); },
                        [=] { return element; });
}

TNode<Object> BaseCollectionsAssembler::LoadAndNormalizeFixedDoubleArrayElement(
    TNode<HeapObject> elements, TNode<IntPtrT> index) {
  TVARIABLE(Object, entry);
  Label if_hole(this, Label::kDeferred), next(this);
  TNode<Float64T> element =
      LoadFixedDoubleArrayElement(CAST(elements), index, &if_hole);
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
  return entry.value();
}

class CollectionsBuiltinsAssembler : public BaseCollectionsAssembler {
 public:
  explicit CollectionsBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : BaseCollectionsAssembler(state) {}

  // Check whether |iterable| is a JS_MAP_KEY_ITERATOR_TYPE or
  // JS_MAP_VALUE_ITERATOR_TYPE object that is not partially consumed and still
  // has original iteration behavior.
  void BranchIfIterableWithOriginalKeyOrValueMapIterator(TNode<Object> iterable,
                                                         TNode<Context> context,
                                                         Label* if_true,
                                                         Label* if_false);

  // Check whether |iterable| is a JS_SET_TYPE or JS_SET_VALUE_ITERATOR_TYPE
  // object that still has original iteration behavior. In case of the iterator,
  // the iterator also must not have been partially consumed.
  void BranchIfIterableWithOriginalValueSetIterator(TNode<Object> iterable,
                                                    TNode<Context> context,
                                                    Label* if_true,
                                                    Label* if_false);

 protected:
  template <typename IteratorType>
  TNode<HeapObject> AllocateJSCollectionIterator(
      const TNode<Context> context, int map_index,
      const TNode<HeapObject> collection);
  TNode<HeapObject> AllocateTable(Variant variant,
                                  TNode<IntPtrT> at_least_space_for) override;
  TNode<IntPtrT> GetHash(const TNode<HeapObject> key);
  TNode<IntPtrT> CallGetHashRaw(const TNode<HeapObject> key);
  TNode<Smi> CallGetOrCreateHashRaw(const TNode<HeapObject> key);

  // Transitions the iterator to the non obsolete backing store.
  // This is a NOP if the [table] is not obsolete.
  template <typename TableType>
  using UpdateInTransition = std::function<void(const TNode<TableType> table,
                                                const TNode<IntPtrT> index)>;
  template <typename TableType>
  std::pair<TNode<TableType>, TNode<IntPtrT>> Transition(
      const TNode<TableType> table, const TNode<IntPtrT> index,
      UpdateInTransition<TableType> const& update_in_transition);
  template <typename IteratorType, typename TableType>
  std::pair<TNode<TableType>, TNode<IntPtrT>> TransitionAndUpdate(
      const TNode<IteratorType> iterator);
  template <typename TableType>
  std::tuple<TNode<Object>, TNode<IntPtrT>, TNode<IntPtrT>> NextSkipHoles(
      TNode<TableType> table, TNode<IntPtrT> index, Label* if_end);

  // Specialization for Smi.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise.
  template <typename CollectionType>
  void FindOrderedHashTableEntryForSmiKey(TNode<CollectionType> table,
                                          TNode<Smi> key_tagged,
                                          TVariable<IntPtrT>* result,
                                          Label* entry_found, Label* not_found);
  void SameValueZeroSmi(TNode<Smi> key_smi, TNode<Object> candidate_key,
                        Label* if_same, Label* if_not_same);

  // Specialization for heap numbers.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise.
  void SameValueZeroHeapNumber(TNode<Float64T> key_float,
                               TNode<Object> candidate_key, Label* if_same,
                               Label* if_not_same);
  template <typename CollectionType>
  void FindOrderedHashTableEntryForHeapNumberKey(
      TNode<CollectionType> table, TNode<HeapNumber> key_heap_number,
      TVariable<IntPtrT>* result, Label* entry_found, Label* not_found);

  // Specialization for bigints.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise.
  void SameValueZeroBigInt(TNode<BigInt> key, TNode<Object> candidate_key,
                           Label* if_same, Label* if_not_same);
  template <typename CollectionType>
  void FindOrderedHashTableEntryForBigIntKey(TNode<CollectionType> table,
                                             TNode<BigInt> key_big_int,
                                             TVariable<IntPtrT>* result,
                                             Label* entry_found,
                                             Label* not_found);

  // Specialization for string.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise.
  template <typename CollectionType>
  void FindOrderedHashTableEntryForStringKey(TNode<CollectionType> table,
                                             TNode<String> key_tagged,
                                             TVariable<IntPtrT>* result,
                                             Label* entry_found,
                                             Label* not_found);
  TNode<IntPtrT> ComputeStringHash(TNode<String> string_key);
  void SameValueZeroString(TNode<String> key_string,
                           TNode<Object> candidate_key, Label* if_same,
                           Label* if_not_same);

  // Specialization for non-strings, non-numbers. For those we only need
  // reference equality to compare the keys.
  // The {result} variable will contain the entry index if the key was found,
  // or the hash code otherwise. If the hash-code has not been computed, it
  // should be Smi -1.
  template <typename CollectionType>
  void FindOrderedHashTableEntryForOtherKey(TNode<CollectionType> table,
                                            TNode<HeapObject> key_heap_object,
                                            TVariable<IntPtrT>* result,
                                            Label* entry_found,
                                            Label* not_found);

  template <typename CollectionType>
  void TryLookupOrderedHashTableIndex(const TNode<CollectionType> table,
                                      const TNode<Object> key,
                                      TVariable<IntPtrT>* result,
                                      Label* if_entry_found,
                                      Label* if_not_found);

  const TNode<Object> NormalizeNumberKey(const TNode<Object> key);
  void StoreOrderedHashMapNewEntry(const TNode<OrderedHashMap> table,
                                   const TNode<Object> key,
                                   const TNode<Object> value,
                                   const TNode<IntPtrT> hash,
                                   const TNode<IntPtrT> number_of_buckets,
                                   const TNode<IntPtrT> occupancy);

  void StoreOrderedHashSetNewEntry(const TNode<OrderedHashSet> table,
                                   const TNode<Object> key,
                                   const TNode<IntPtrT> hash,
                                   const TNode<IntPtrT> number_of_buckets,
                                   const TNode<IntPtrT> occupancy);

  // Create a JSArray with PACKED_ELEMENTS kind from a Map.prototype.keys() or
  // Map.prototype.values() iterator. The iterator is assumed to satisfy
  // IterableWithOriginalKeyOrValueMapIterator. This function will skip the
  // iterator and iterate directly on the underlying hash table. In the end it
  // will update the state of the iterator to 'exhausted'.
  TNode<JSArray> MapIteratorToList(TNode<Context> context,
                                   TNode<JSMapIterator> iterator);

  // Create a JSArray with PACKED_ELEMENTS kind from a Set.prototype.keys() or
  // Set.prototype.values() iterator, or a Set. The |iterable| is assumed to
  // satisfy IterableWithOriginalValueSetIterator. This function will skip the
  // iterator and iterate directly on the underlying hash table. In the end, if
  // |iterable| is an iterator, it will update the state of the iterator to
  // 'exhausted'.
  TNode<JSArray> SetOrSetIteratorToList(TNode<Context> context,
                                        TNode<HeapObject> iterable);

  void BranchIfMapIteratorProtectorValid(Label* if_true, Label* if_false);
  void BranchIfSetIteratorProtectorValid(Label* if_true, Label* if_false);

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
      const TNode<CollectionType> table, const TNode<IntPtrT> hash,
      const std::function<void(TNode<Object>, Label*, Label*)>& key_compare,
      TVariable<IntPtrT>* entry_start_position, Label* entry_found,
      Label* not_found);

  TNode<Word32T> ComputeUnseededHash(TNode<IntPtrT> key);
};

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntry(
    const TNode<CollectionType> table, const TNode<IntPtrT> hash,
    const std::function<void(TNode<Object>, Label*, Label*)>& key_compare,
    TVariable<IntPtrT>* entry_start_position, Label* entry_found,
    Label* not_found) {
  // Get the index of the bucket.
  const TNode<IntPtrT> number_of_buckets =
      SmiUntag(CAST(UnsafeLoadFixedArrayElement(
          table, CollectionType::NumberOfBucketsIndex())));
  const TNode<IntPtrT> bucket =
      WordAnd(hash, IntPtrSub(number_of_buckets, IntPtrConstant(1)));
  const TNode<IntPtrT> first_entry = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
      table, bucket, CollectionType::HashTableStartIndex() * kTaggedSize)));

  // Walk the bucket chain.
  TNode<IntPtrT> entry_start;
  Label if_key_found(this);
  {
    TVARIABLE(IntPtrT, var_entry, first_entry);
    Label loop(this, {&var_entry, entry_start_position}),
        continue_next_entry(this);
    Goto(&loop);
    BIND(&loop);

    // If the entry index is the not-found sentinel, we are done.
    GotoIf(IntPtrEqual(var_entry.value(),
                       IntPtrConstant(CollectionType::kNotFound)),
           not_found);

    // Make sure the entry index is within range.
    CSA_ASSERT(
        this,
        UintPtrLessThan(
            var_entry.value(),
            SmiUntag(SmiAdd(
                CAST(UnsafeLoadFixedArrayElement(
                    table, CollectionType::NumberOfElementsIndex())),
                CAST(UnsafeLoadFixedArrayElement(
                    table, CollectionType::NumberOfDeletedElementsIndex()))))));

    // Compute the index of the entry relative to kHashTableStartIndex.
    entry_start =
        IntPtrAdd(IntPtrMul(var_entry.value(),
                            IntPtrConstant(CollectionType::kEntrySize)),
                  number_of_buckets);

    // Load the key from the entry.
    const TNode<Object> candidate_key = UnsafeLoadFixedArrayElement(
        table, entry_start,
        CollectionType::HashTableStartIndex() * kTaggedSize);

    key_compare(candidate_key, &if_key_found, &continue_next_entry);

    BIND(&continue_next_entry);
    // Load the index of the next entry in the bucket chain.
    var_entry = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
        table, entry_start,
        (CollectionType::HashTableStartIndex() + CollectionType::kChainOffset) *
            kTaggedSize)));

    Goto(&loop);
  }

  BIND(&if_key_found);
  *entry_start_position = entry_start;
  Goto(entry_found);
}

template <typename IteratorType>
TNode<HeapObject> CollectionsBuiltinsAssembler::AllocateJSCollectionIterator(
    const TNode<Context> context, int map_index,
    const TNode<HeapObject> collection) {
  const TNode<Object> table =
      LoadObjectField(collection, JSCollection::kTableOffset);
  const TNode<NativeContext> native_context = LoadNativeContext(context);
  const TNode<Map> iterator_map =
      CAST(LoadContextElement(native_context, map_index));
  const TNode<HeapObject> iterator =
      AllocateInNewSpace(IteratorType::kHeaderSize);
  StoreMapNoWriteBarrier(iterator, iterator_map);
  StoreObjectFieldRoot(iterator, IteratorType::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(iterator, IteratorType::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(iterator, IteratorType::kTableOffset, table);
  StoreObjectFieldNoWriteBarrier(iterator, IteratorType::kIndexOffset,
                                 SmiConstant(0));
  return iterator;
}

TNode<HeapObject> CollectionsBuiltinsAssembler::AllocateTable(
    Variant variant, TNode<IntPtrT> at_least_space_for) {
  if (variant == kMap || variant == kWeakMap) {
    return AllocateOrderedHashMap();
  } else {
    return AllocateOrderedHashSet();
  }
}

TF_BUILTIN(MapConstructor, CollectionsBuiltinsAssembler) {
  auto new_target = Parameter<Object>(Descriptor::kJSNewTarget);
  TNode<IntPtrT> argc = ChangeInt32ToIntPtr(
      UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount));
  auto context = Parameter<Context>(Descriptor::kContext);

  GenerateConstructor(kMap, isolate()->factory()->Map_string(), new_target,
                      argc, context);
}

TF_BUILTIN(SetConstructor, CollectionsBuiltinsAssembler) {
  auto new_target = Parameter<Object>(Descriptor::kJSNewTarget);
  TNode<IntPtrT> argc = ChangeInt32ToIntPtr(
      UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount));
  auto context = Parameter<Context>(Descriptor::kContext);

  GenerateConstructor(kSet, isolate()->factory()->Set_string(), new_target,
                      argc, context);
}

TNode<Smi> CollectionsBuiltinsAssembler::CallGetOrCreateHashRaw(
    const TNode<HeapObject> key) {
  const TNode<ExternalReference> function_addr =
      ExternalConstant(ExternalReference::get_or_create_hash_raw());
  const TNode<ExternalReference> isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_tagged = MachineType::AnyTagged();

  TNode<Smi> result = CAST(CallCFunction(function_addr, type_tagged,
                                         std::make_pair(type_ptr, isolate_ptr),
                                         std::make_pair(type_tagged, key)));

  return result;
}

TNode<IntPtrT> CollectionsBuiltinsAssembler::CallGetHashRaw(
    const TNode<HeapObject> key) {
  const TNode<ExternalReference> function_addr =
      ExternalConstant(ExternalReference::orderedhashmap_gethash_raw());
  const TNode<ExternalReference> isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_tagged = MachineType::AnyTagged();

  TNode<Smi> result = CAST(CallCFunction(function_addr, type_tagged,
                                         std::make_pair(type_ptr, isolate_ptr),
                                         std::make_pair(type_tagged, key)));

  return SmiUntag(result);
}

TNode<IntPtrT> CollectionsBuiltinsAssembler::GetHash(
    const TNode<HeapObject> key) {
  TVARIABLE(IntPtrT, var_hash);
  Label if_receiver(this), if_other(this), done(this);
  Branch(IsJSReceiver(key), &if_receiver, &if_other);

  BIND(&if_receiver);
  {
    var_hash = LoadJSReceiverIdentityHash(CAST(key));
    Goto(&done);
  }

  BIND(&if_other);
  {
    var_hash = CallGetHashRaw(key);
    Goto(&done);
  }

  BIND(&done);
  return var_hash.value();
}

void CollectionsBuiltinsAssembler::SameValueZeroSmi(TNode<Smi> key_smi,
                                                    TNode<Object> candidate_key,
                                                    Label* if_same,
                                                    Label* if_not_same) {
  // If the key is the same, we are done.
  GotoIf(TaggedEqual(candidate_key, key_smi), if_same);

  // If the candidate key is smi, then it must be different (because
  // we already checked for equality above).
  GotoIf(TaggedIsSmi(candidate_key), if_not_same);

  // If the candidate key is not smi, we still have to check if it is a
  // heap number with the same value.
  GotoIfNot(IsHeapNumber(CAST(candidate_key)), if_not_same);

  const TNode<Float64T> candidate_key_number =
      LoadHeapNumberValue(CAST(candidate_key));
  const TNode<Float64T> key_number = SmiToFloat64(key_smi);

  GotoIf(Float64Equal(candidate_key_number, key_number), if_same);

  Goto(if_not_same);
}

void CollectionsBuiltinsAssembler::BranchIfMapIteratorProtectorValid(
    Label* if_true, Label* if_false) {
  TNode<PropertyCell> protector_cell = MapIteratorProtectorConstant();
  DCHECK(isolate()->heap()->map_iterator_protector().IsPropertyCell());
  Branch(
      TaggedEqual(LoadObjectField(protector_cell, PropertyCell::kValueOffset),
                  SmiConstant(Protectors::kProtectorValid)),
      if_true, if_false);
}

void CollectionsBuiltinsAssembler::
    BranchIfIterableWithOriginalKeyOrValueMapIterator(TNode<Object> iterator,
                                                      TNode<Context> context,
                                                      Label* if_true,
                                                      Label* if_false) {
  Label if_key_or_value_iterator(this), extra_checks(this);

  // Check if iterator is a keys or values JSMapIterator.
  GotoIf(TaggedIsSmi(iterator), if_false);
  TNode<Map> iter_map = LoadMap(CAST(iterator));
  const TNode<Uint16T> instance_type = LoadMapInstanceType(iter_map);
  GotoIf(InstanceTypeEqual(instance_type, JS_MAP_KEY_ITERATOR_TYPE),
         &if_key_or_value_iterator);
  Branch(InstanceTypeEqual(instance_type, JS_MAP_VALUE_ITERATOR_TYPE),
         &if_key_or_value_iterator, if_false);

  BIND(&if_key_or_value_iterator);
  // Check that the iterator is not partially consumed.
  const TNode<Object> index =
      LoadObjectField(CAST(iterator), JSMapIterator::kIndexOffset);
  GotoIfNot(TaggedEqual(index, SmiConstant(0)), if_false);
  BranchIfMapIteratorProtectorValid(&extra_checks, if_false);

  BIND(&extra_checks);
  // Check if the iterator object has the original %MapIteratorPrototype%.
  const TNode<NativeContext> native_context = LoadNativeContext(context);
  const TNode<Object> initial_map_iter_proto = LoadContextElement(
      native_context, Context::INITIAL_MAP_ITERATOR_PROTOTYPE_INDEX);
  const TNode<HeapObject> map_iter_proto = LoadMapPrototype(iter_map);
  GotoIfNot(TaggedEqual(map_iter_proto, initial_map_iter_proto), if_false);

  // Check if the original MapIterator prototype has the original
  // %IteratorPrototype%.
  const TNode<Object> initial_iter_proto = LoadContextElement(
      native_context, Context::INITIAL_ITERATOR_PROTOTYPE_INDEX);
  const TNode<HeapObject> iter_proto =
      LoadMapPrototype(LoadMap(map_iter_proto));
  Branch(TaggedEqual(iter_proto, initial_iter_proto), if_true, if_false);
}

void BranchIfIterableWithOriginalKeyOrValueMapIterator(
    compiler::CodeAssemblerState* state, TNode<Object> iterable,
    TNode<Context> context, compiler::CodeAssemblerLabel* if_true,
    compiler::CodeAssemblerLabel* if_false) {
  CollectionsBuiltinsAssembler assembler(state);
  assembler.BranchIfIterableWithOriginalKeyOrValueMapIterator(
      iterable, context, if_true, if_false);
}

void CollectionsBuiltinsAssembler::BranchIfSetIteratorProtectorValid(
    Label* if_true, Label* if_false) {
  const TNode<PropertyCell> protector_cell = SetIteratorProtectorConstant();
  DCHECK(isolate()->heap()->set_iterator_protector().IsPropertyCell());
  Branch(
      TaggedEqual(LoadObjectField(protector_cell, PropertyCell::kValueOffset),
                  SmiConstant(Protectors::kProtectorValid)),
      if_true, if_false);
}

void CollectionsBuiltinsAssembler::BranchIfIterableWithOriginalValueSetIterator(
    TNode<Object> iterable, TNode<Context> context, Label* if_true,
    Label* if_false) {
  Label if_set(this), if_value_iterator(this), check_protector(this);
  TVARIABLE(BoolT, var_result);

  GotoIf(TaggedIsSmi(iterable), if_false);
  TNode<Map> iterable_map = LoadMap(CAST(iterable));
  const TNode<Uint16T> instance_type = LoadMapInstanceType(iterable_map);

  GotoIf(InstanceTypeEqual(instance_type, JS_SET_TYPE), &if_set);
  Branch(InstanceTypeEqual(instance_type, JS_SET_VALUE_ITERATOR_TYPE),
         &if_value_iterator, if_false);

  BIND(&if_set);
  // Check if the set object has the original Set prototype.
  const TNode<Object> initial_set_proto = LoadContextElement(
      LoadNativeContext(context), Context::INITIAL_SET_PROTOTYPE_INDEX);
  const TNode<HeapObject> set_proto = LoadMapPrototype(iterable_map);
  GotoIfNot(TaggedEqual(set_proto, initial_set_proto), if_false);
  Goto(&check_protector);

  BIND(&if_value_iterator);
  // Check that the iterator is not partially consumed.
  const TNode<Object> index =
      LoadObjectField(CAST(iterable), JSSetIterator::kIndexOffset);
  GotoIfNot(TaggedEqual(index, SmiConstant(0)), if_false);

  // Check if the iterator object has the original SetIterator prototype.
  const TNode<NativeContext> native_context = LoadNativeContext(context);
  const TNode<Object> initial_set_iter_proto = LoadContextElement(
      native_context, Context::INITIAL_SET_ITERATOR_PROTOTYPE_INDEX);
  const TNode<HeapObject> set_iter_proto = LoadMapPrototype(iterable_map);
  GotoIfNot(TaggedEqual(set_iter_proto, initial_set_iter_proto), if_false);

  // Check if the original SetIterator prototype has the original
  // %IteratorPrototype%.
  const TNode<Object> initial_iter_proto = LoadContextElement(
      native_context, Context::INITIAL_ITERATOR_PROTOTYPE_INDEX);
  const TNode<HeapObject> iter_proto =
      LoadMapPrototype(LoadMap(set_iter_proto));
  GotoIfNot(TaggedEqual(iter_proto, initial_iter_proto), if_false);
  Goto(&check_protector);

  BIND(&check_protector);
  BranchIfSetIteratorProtectorValid(if_true, if_false);
}

void BranchIfIterableWithOriginalValueSetIterator(
    compiler::CodeAssemblerState* state, TNode<Object> iterable,
    TNode<Context> context, compiler::CodeAssemblerLabel* if_true,
    compiler::CodeAssemblerLabel* if_false) {
  CollectionsBuiltinsAssembler assembler(state);
  assembler.BranchIfIterableWithOriginalValueSetIterator(iterable, context,
                                                         if_true, if_false);
}

TNode<JSArray> CollectionsBuiltinsAssembler::MapIteratorToList(
    TNode<Context> context, TNode<JSMapIterator> iterator) {
  // Transition the {iterator} table if necessary.
  TNode<OrderedHashMap> table;
  TNode<IntPtrT> index;
  std::tie(table, index) =
      TransitionAndUpdate<JSMapIterator, OrderedHashMap>(iterator);
  CSA_ASSERT(this, IntPtrEqual(index, IntPtrConstant(0)));

  TNode<IntPtrT> size =
      LoadAndUntagObjectField(table, OrderedHashMap::NumberOfElementsOffset());

  const ElementsKind kind = PACKED_ELEMENTS;
  TNode<Map> array_map =
      LoadJSArrayElementsMap(kind, LoadNativeContext(context));
  TNode<JSArray> array = AllocateJSArray(kind, array_map, size, SmiTag(size),
                                         kAllowLargeObjectAllocation);
  TNode<FixedArray> elements = CAST(LoadElements(array));

  const int first_element_offset = FixedArray::kHeaderSize - kHeapObjectTag;
  TNode<IntPtrT> first_to_element_offset =
      ElementOffsetFromIndex(IntPtrConstant(0), kind, 0);
  TVARIABLE(
      IntPtrT, var_offset,
      IntPtrAdd(first_to_element_offset, IntPtrConstant(first_element_offset)));
  TVARIABLE(IntPtrT, var_index, index);
  VariableList vars({&var_index, &var_offset}, zone());
  Label done(this, {&var_index}), loop(this, vars), continue_loop(this, vars),
      write_key(this, vars), write_value(this, vars);

  Goto(&loop);

  BIND(&loop);
  {
    // Read the next entry from the {table}, skipping holes.
    TNode<Object> entry_key;
    TNode<IntPtrT> entry_start_position;
    TNode<IntPtrT> cur_index;
    std::tie(entry_key, entry_start_position, cur_index) =
        NextSkipHoles<OrderedHashMap>(table, var_index.value(), &done);

    // Decide to write key or value.
    Branch(
        InstanceTypeEqual(LoadInstanceType(iterator), JS_MAP_KEY_ITERATOR_TYPE),
        &write_key, &write_value);

    BIND(&write_key);
    {
      Store(elements, var_offset.value(), entry_key);
      Goto(&continue_loop);
    }

    BIND(&write_value);
    {
      CSA_ASSERT(this, InstanceTypeEqual(LoadInstanceType(iterator),
                                         JS_MAP_VALUE_ITERATOR_TYPE));
      TNode<Object> entry_value =
          UnsafeLoadFixedArrayElement(table, entry_start_position,
                                      (OrderedHashMap::HashTableStartIndex() +
                                       OrderedHashMap::kValueOffset) *
                                          kTaggedSize);

      Store(elements, var_offset.value(), entry_value);
      Goto(&continue_loop);
    }

    BIND(&continue_loop);
    {
      // Increment the array offset and continue the loop to the next entry.
      var_index = cur_index;
      var_offset = IntPtrAdd(var_offset.value(), IntPtrConstant(kTaggedSize));
      Goto(&loop);
    }
  }

  BIND(&done);
  // Set the {iterator} to exhausted.
  StoreObjectFieldRoot(iterator, JSMapIterator::kTableOffset,
                       RootIndex::kEmptyOrderedHashMap);
  StoreObjectFieldNoWriteBarrier(iterator, JSMapIterator::kIndexOffset,
                                 SmiTag(var_index.value()));
  return UncheckedCast<JSArray>(array);
}

TF_BUILTIN(MapIteratorToList, CollectionsBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto iterator = Parameter<JSMapIterator>(Descriptor::kSource);
  Return(MapIteratorToList(context, iterator));
}

TNode<JSArray> CollectionsBuiltinsAssembler::SetOrSetIteratorToList(
    TNode<Context> context, TNode<HeapObject> iterable) {
  TVARIABLE(OrderedHashSet, var_table);
  Label if_set(this), if_iterator(this), copy(this);

  const TNode<Uint16T> instance_type = LoadInstanceType(iterable);
  Branch(InstanceTypeEqual(instance_type, JS_SET_TYPE), &if_set, &if_iterator);

  BIND(&if_set);
  {
    // {iterable} is a JSSet.
    var_table = CAST(LoadObjectField(iterable, JSSet::kTableOffset));
    Goto(&copy);
  }

  BIND(&if_iterator);
  {
    // {iterable} is a JSSetIterator.
    // Transition the {iterable} table if necessary.
    TNode<OrderedHashSet> iter_table;
    TNode<IntPtrT> iter_index;
    std::tie(iter_table, iter_index) =
        TransitionAndUpdate<JSSetIterator, OrderedHashSet>(CAST(iterable));
    CSA_ASSERT(this, IntPtrEqual(iter_index, IntPtrConstant(0)));
    var_table = iter_table;
    Goto(&copy);
  }

  BIND(&copy);
  TNode<OrderedHashSet> table = var_table.value();
  TNode<IntPtrT> size =
      LoadAndUntagObjectField(table, OrderedHashMap::NumberOfElementsOffset());

  const ElementsKind kind = PACKED_ELEMENTS;
  TNode<Map> array_map =
      LoadJSArrayElementsMap(kind, LoadNativeContext(context));
  TNode<JSArray> array = AllocateJSArray(kind, array_map, size, SmiTag(size),
                                         kAllowLargeObjectAllocation);
  TNode<FixedArray> elements = CAST(LoadElements(array));

  const int first_element_offset = FixedArray::kHeaderSize - kHeapObjectTag;
  TNode<IntPtrT> first_to_element_offset =
      ElementOffsetFromIndex(IntPtrConstant(0), kind, 0);
  TVARIABLE(
      IntPtrT, var_offset,
      IntPtrAdd(first_to_element_offset, IntPtrConstant(first_element_offset)));
  TVARIABLE(IntPtrT, var_index, IntPtrConstant(0));
  Label done(this), finalize(this, {&var_index}),
      loop(this, {&var_index, &var_offset});

  Goto(&loop);

  BIND(&loop);
  {
    // Read the next entry from the {table}, skipping holes.
    TNode<Object> entry_key;
    TNode<IntPtrT> entry_start_position;
    TNode<IntPtrT> cur_index;
    std::tie(entry_key, entry_start_position, cur_index) =
        NextSkipHoles<OrderedHashSet>(table, var_index.value(), &finalize);

    Store(elements, var_offset.value(), entry_key);

    var_index = cur_index;
    var_offset = IntPtrAdd(var_offset.value(), IntPtrConstant(kTaggedSize));
    Goto(&loop);
  }

  BIND(&finalize);
  GotoIf(InstanceTypeEqual(instance_type, JS_SET_TYPE), &done);
  // Set the {iterable} to exhausted if it's an iterator.
  StoreObjectFieldRoot(iterable, JSSetIterator::kTableOffset,
                       RootIndex::kEmptyOrderedHashSet);
  StoreObjectFieldNoWriteBarrier(iterable, JSSetIterator::kIndexOffset,
                                 SmiTag(var_index.value()));
  Goto(&done);

  BIND(&done);
  return UncheckedCast<JSArray>(array);
}

TF_BUILTIN(SetOrSetIteratorToList, CollectionsBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto object = Parameter<HeapObject>(Descriptor::kSource);
  Return(SetOrSetIteratorToList(context, object));
}

TNode<Word32T> CollectionsBuiltinsAssembler::ComputeUnseededHash(
    TNode<IntPtrT> key) {
  // See v8::internal::ComputeUnseededHash()
  TNode<Word32T> hash = TruncateIntPtrToInt32(key);
  hash = Int32Add(Word32Xor(hash, Int32Constant(0xFFFFFFFF)),
                  Word32Shl(hash, Int32Constant(15)));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(12)));
  hash = Int32Add(hash, Word32Shl(hash, Int32Constant(2)));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(4)));
  hash = Int32Mul(hash, Int32Constant(2057));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(16)));
  return Word32And(hash, Int32Constant(0x3FFFFFFF));
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForSmiKey(
    TNode<CollectionType> table, TNode<Smi> smi_key, TVariable<IntPtrT>* result,
    Label* entry_found, Label* not_found) {
  const TNode<IntPtrT> key_untagged = SmiUntag(smi_key);
  const TNode<IntPtrT> hash =
      ChangeInt32ToIntPtr(ComputeUnseededHash(key_untagged));
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  *result = hash;
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](TNode<Object> other_key, Label* if_same, Label* if_not_same) {
        SameValueZeroSmi(smi_key, other_key, if_same, if_not_same);
      },
      result, entry_found, not_found);
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForStringKey(
    TNode<CollectionType> table, TNode<String> key_tagged,
    TVariable<IntPtrT>* result, Label* entry_found, Label* not_found) {
  const TNode<IntPtrT> hash = ComputeStringHash(key_tagged);
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  *result = hash;
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](TNode<Object> other_key, Label* if_same, Label* if_not_same) {
        SameValueZeroString(key_tagged, other_key, if_same, if_not_same);
      },
      result, entry_found, not_found);
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForHeapNumberKey(
    TNode<CollectionType> table, TNode<HeapNumber> key_heap_number,
    TVariable<IntPtrT>* result, Label* entry_found, Label* not_found) {
  const TNode<IntPtrT> hash = CallGetHashRaw(key_heap_number);
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  *result = hash;
  const TNode<Float64T> key_float = LoadHeapNumberValue(key_heap_number);
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](TNode<Object> other_key, Label* if_same, Label* if_not_same) {
        SameValueZeroHeapNumber(key_float, other_key, if_same, if_not_same);
      },
      result, entry_found, not_found);
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForBigIntKey(
    TNode<CollectionType> table, TNode<BigInt> key_big_int,
    TVariable<IntPtrT>* result, Label* entry_found, Label* not_found) {
  const TNode<IntPtrT> hash = CallGetHashRaw(key_big_int);
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  *result = hash;
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](TNode<Object> other_key, Label* if_same, Label* if_not_same) {
        SameValueZeroBigInt(key_big_int, other_key, if_same, if_not_same);
      },
      result, entry_found, not_found);
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::FindOrderedHashTableEntryForOtherKey(
    TNode<CollectionType> table, TNode<HeapObject> key_heap_object,
    TVariable<IntPtrT>* result, Label* entry_found, Label* not_found) {
  const TNode<IntPtrT> hash = GetHash(key_heap_object);
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(hash, IntPtrConstant(0)));
  *result = hash;
  FindOrderedHashTableEntry<CollectionType>(
      table, hash,
      [&](TNode<Object> other_key, Label* if_same, Label* if_not_same) {
        Branch(TaggedEqual(key_heap_object, other_key), if_same, if_not_same);
      },
      result, entry_found, not_found);
}

TNode<IntPtrT> CollectionsBuiltinsAssembler::ComputeStringHash(
    TNode<String> string_key) {
  TVARIABLE(IntPtrT, var_result);

  Label hash_not_computed(this), done(this, &var_result);
  const TNode<IntPtrT> hash =
      ChangeInt32ToIntPtr(LoadNameHash(string_key, &hash_not_computed));
  var_result = hash;
  Goto(&done);

  BIND(&hash_not_computed);
  var_result = CallGetHashRaw(string_key);
  Goto(&done);

  BIND(&done);
  return var_result.value();
}

void CollectionsBuiltinsAssembler::SameValueZeroString(
    TNode<String> key_string, TNode<Object> candidate_key, Label* if_same,
    Label* if_not_same) {
  // If the candidate is not a string, the keys are not equal.
  GotoIf(TaggedIsSmi(candidate_key), if_not_same);
  GotoIfNot(IsString(CAST(candidate_key)), if_not_same);

  Branch(TaggedEqual(CallBuiltin(Builtin::kStringEqual, NoContextConstant(),
                                 key_string, candidate_key),
                     TrueConstant()),
         if_same, if_not_same);
}

void CollectionsBuiltinsAssembler::SameValueZeroBigInt(
    TNode<BigInt> key, TNode<Object> candidate_key, Label* if_same,
    Label* if_not_same) {
  GotoIf(TaggedIsSmi(candidate_key), if_not_same);
  GotoIfNot(IsBigInt(CAST(candidate_key)), if_not_same);

  Branch(TaggedEqual(CallRuntime(Runtime::kBigIntEqualToBigInt,
                                 NoContextConstant(), key, candidate_key),
                     TrueConstant()),
         if_same, if_not_same);
}

void CollectionsBuiltinsAssembler::SameValueZeroHeapNumber(
    TNode<Float64T> key_float, TNode<Object> candidate_key, Label* if_same,
    Label* if_not_same) {
  Label if_smi(this), if_keyisnan(this);

  GotoIf(TaggedIsSmi(candidate_key), &if_smi);
  GotoIfNot(IsHeapNumber(CAST(candidate_key)), if_not_same);

  {
    // {candidate_key} is a heap number.
    const TNode<Float64T> candidate_float =
        LoadHeapNumberValue(CAST(candidate_key));
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
    const TNode<Float64T> candidate_float = SmiToFloat64(CAST(candidate_key));
    Branch(Float64Equal(key_float, candidate_float), if_same, if_not_same);
  }
}

TF_BUILTIN(OrderedHashTableHealIndex, CollectionsBuiltinsAssembler) {
  auto table = Parameter<HeapObject>(Descriptor::kTable);
  auto index = Parameter<Smi>(Descriptor::kIndex);
  Label return_index(this), return_zero(this);

  // Check if we need to update the {index}.
  GotoIfNot(SmiLessThan(SmiConstant(0), index), &return_zero);

  // Check if the {table} was cleared.
  STATIC_ASSERT(OrderedHashMap::NumberOfDeletedElementsOffset() ==
                OrderedHashSet::NumberOfDeletedElementsOffset());
  TNode<IntPtrT> number_of_deleted_elements = LoadAndUntagObjectField(
      table, OrderedHashMap::NumberOfDeletedElementsOffset());
  STATIC_ASSERT(OrderedHashMap::kClearedTableSentinel ==
                OrderedHashSet::kClearedTableSentinel);
  GotoIf(IntPtrEqual(number_of_deleted_elements,
                     IntPtrConstant(OrderedHashMap::kClearedTableSentinel)),
         &return_zero);

  TVARIABLE(IntPtrT, var_i, IntPtrConstant(0));
  TVARIABLE(Smi, var_index, index);
  Label loop(this, {&var_i, &var_index});
  Goto(&loop);
  BIND(&loop);
  {
    TNode<IntPtrT> i = var_i.value();
    GotoIfNot(IntPtrLessThan(i, number_of_deleted_elements), &return_index);
    STATIC_ASSERT(OrderedHashMap::RemovedHolesIndex() ==
                  OrderedHashSet::RemovedHolesIndex());
    TNode<Smi> removed_index = CAST(LoadFixedArrayElement(
        CAST(table), i, OrderedHashMap::RemovedHolesIndex() * kTaggedSize));
    GotoIf(SmiGreaterThanOrEqual(removed_index, index), &return_index);
    Decrement(&var_index);
    Increment(&var_i);
    Goto(&loop);
  }

  BIND(&return_index);
  Return(var_index.value());

  BIND(&return_zero);
  Return(SmiConstant(0));
}

template <typename TableType>
std::pair<TNode<TableType>, TNode<IntPtrT>>
CollectionsBuiltinsAssembler::Transition(
    const TNode<TableType> table, const TNode<IntPtrT> index,
    UpdateInTransition<TableType> const& update_in_transition) {
  TVARIABLE(IntPtrT, var_index, index);
  TVARIABLE(TableType, var_table, table);
  Label if_done(this), if_transition(this, Label::kDeferred);
  Branch(TaggedIsSmi(
             LoadObjectField(var_table.value(), TableType::NextTableOffset())),
         &if_done, &if_transition);

  BIND(&if_transition);
  {
    Label loop(this, {&var_table, &var_index}), done_loop(this);
    Goto(&loop);
    BIND(&loop);
    {
      TNode<TableType> table = var_table.value();
      TNode<IntPtrT> index = var_index.value();

      TNode<Object> next_table =
          LoadObjectField(table, TableType::NextTableOffset());
      GotoIf(TaggedIsSmi(next_table), &done_loop);

      var_table = CAST(next_table);
      var_index = SmiUntag(
          CAST(CallBuiltin(Builtin::kOrderedHashTableHealIndex,
                           NoContextConstant(), table, SmiTag(index))));
      Goto(&loop);
    }
    BIND(&done_loop);

    // Update with the new {table} and {index}.
    update_in_transition(var_table.value(), var_index.value());
    Goto(&if_done);
  }

  BIND(&if_done);
  return {var_table.value(), var_index.value()};
}

template <typename IteratorType, typename TableType>
std::pair<TNode<TableType>, TNode<IntPtrT>>
CollectionsBuiltinsAssembler::TransitionAndUpdate(
    const TNode<IteratorType> iterator) {
  return Transition<TableType>(
      CAST(LoadObjectField(iterator, IteratorType::kTableOffset)),
      LoadAndUntagObjectField(iterator, IteratorType::kIndexOffset),
      [this, iterator](const TNode<TableType> table,
                       const TNode<IntPtrT> index) {
        // Update the {iterator} with the new state.
        StoreObjectField(iterator, IteratorType::kTableOffset, table);
        StoreObjectFieldNoWriteBarrier(iterator, IteratorType::kIndexOffset,
                                       SmiTag(index));
      });
}

template <typename TableType>
std::tuple<TNode<Object>, TNode<IntPtrT>, TNode<IntPtrT>>
CollectionsBuiltinsAssembler::NextSkipHoles(TNode<TableType> table,
                                            TNode<IntPtrT> index,
                                            Label* if_end) {
  // Compute the used capacity for the {table}.
  TNode<IntPtrT> number_of_buckets =
      LoadAndUntagObjectField(table, TableType::NumberOfBucketsOffset());
  TNode<IntPtrT> number_of_elements =
      LoadAndUntagObjectField(table, TableType::NumberOfElementsOffset());
  TNode<IntPtrT> number_of_deleted_elements = LoadAndUntagObjectField(
      table, TableType::NumberOfDeletedElementsOffset());
  TNode<IntPtrT> used_capacity =
      IntPtrAdd(number_of_elements, number_of_deleted_elements);

  TNode<Object> entry_key;
  TNode<IntPtrT> entry_start_position;
  TVARIABLE(IntPtrT, var_index, index);
  Label loop(this, &var_index), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    GotoIfNot(IntPtrLessThan(var_index.value(), used_capacity), if_end);
    entry_start_position = IntPtrAdd(
        IntPtrMul(var_index.value(), IntPtrConstant(TableType::kEntrySize)),
        number_of_buckets);
    entry_key = UnsafeLoadFixedArrayElement(
        table, entry_start_position,
        TableType::HashTableStartIndex() * kTaggedSize);
    Increment(&var_index);
    Branch(IsTheHole(entry_key), &loop, &done_loop);
  }

  BIND(&done_loop);
  return std::tuple<TNode<Object>, TNode<IntPtrT>, TNode<IntPtrT>>{
      entry_key, entry_start_position, var_index.value()};
}

TF_BUILTIN(MapPrototypeGet, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto key = Parameter<Object>(Descriptor::kKey);
  const auto context = Parameter<Context>(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, "Map.prototype.get");

  const TNode<Object> table =
      LoadObjectField<Object>(CAST(receiver), JSMap::kTableOffset);
  TNode<Smi> index =
      CAST(CallBuiltin(Builtin::kFindOrderedHashMapEntry, context, table, key));

  Label if_found(this), if_not_found(this);
  Branch(SmiGreaterThanOrEqual(index, SmiConstant(0)), &if_found,
         &if_not_found);

  BIND(&if_found);
  Return(LoadFixedArrayElement(
      CAST(table), SmiUntag(index),
      (OrderedHashMap::HashTableStartIndex() + OrderedHashMap::kValueOffset) *
          kTaggedSize));

  BIND(&if_not_found);
  Return(UndefinedConstant());
}

TF_BUILTIN(MapPrototypeHas, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto key = Parameter<Object>(Descriptor::kKey);
  const auto context = Parameter<Context>(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, "Map.prototype.has");

  const TNode<Object> table =
      LoadObjectField(CAST(receiver), JSMap::kTableOffset);
  TNode<Smi> index =
      CAST(CallBuiltin(Builtin::kFindOrderedHashMapEntry, context, table, key));

  Label if_found(this), if_not_found(this);
  Branch(SmiGreaterThanOrEqual(index, SmiConstant(0)), &if_found,
         &if_not_found);

  BIND(&if_found);
  Return(TrueConstant());

  BIND(&if_not_found);
  Return(FalseConstant());
}

const TNode<Object> CollectionsBuiltinsAssembler::NormalizeNumberKey(
    const TNode<Object> key) {
  TVARIABLE(Object, result, key);
  Label done(this);

  GotoIf(TaggedIsSmi(key), &done);
  GotoIfNot(IsHeapNumber(CAST(key)), &done);
  const TNode<Float64T> number = LoadHeapNumberValue(CAST(key));
  GotoIfNot(Float64Equal(number, Float64Constant(0.0)), &done);
  // We know the value is zero, so we take the key to be Smi 0.
  // Another option would be to normalize to Smi here.
  result = SmiConstant(0);
  Goto(&done);

  BIND(&done);
  return result.value();
}

TF_BUILTIN(MapPrototypeSet, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  auto key = Parameter<Object>(Descriptor::kKey);
  const auto value = Parameter<Object>(Descriptor::kValue);
  const auto context = Parameter<Context>(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, "Map.prototype.set");

  key = NormalizeNumberKey(key);

  const TNode<OrderedHashMap> table =
      LoadObjectField<OrderedHashMap>(CAST(receiver), JSMap::kTableOffset);

  TVARIABLE(IntPtrT, entry_start_position_or_hash, IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashMap>(
      table, key, &entry_start_position_or_hash, &entry_found, &not_found);

  BIND(&entry_found);
  // If we found the entry, we just store the value there.
  StoreFixedArrayElement(table, entry_start_position_or_hash.value(), value,
                         UPDATE_WRITE_BARRIER,
                         kTaggedSize * (OrderedHashMap::HashTableStartIndex() +
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
    entry_start_position_or_hash = SmiUntag(CallGetOrCreateHashRaw(CAST(key)));
    Goto(&add_entry);
  }

  BIND(&add_entry);
  TVARIABLE(IntPtrT, number_of_buckets);
  TVARIABLE(IntPtrT, occupancy);
  TVARIABLE(OrderedHashMap, table_var, table);
  {
    // Check we have enough space for the entry.
    number_of_buckets = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
        table, OrderedHashMap::NumberOfBucketsIndex())));

    STATIC_ASSERT(OrderedHashMap::kLoadFactor == 2);
    const TNode<WordT> capacity = WordShl(number_of_buckets.value(), 1);
    const TNode<IntPtrT> number_of_elements = SmiUntag(
        CAST(LoadObjectField(table, OrderedHashMap::NumberOfElementsOffset())));
    const TNode<IntPtrT> number_of_deleted = SmiUntag(CAST(LoadObjectField(
        table, OrderedHashMap::NumberOfDeletedElementsOffset())));
    occupancy = IntPtrAdd(number_of_elements, number_of_deleted);
    GotoIf(IntPtrLessThan(occupancy.value(), capacity), &store_new_entry);

    // We do not have enough space, grow the table and reload the relevant
    // fields.
    CallRuntime(Runtime::kMapGrow, context, receiver);
    table_var =
        LoadObjectField<OrderedHashMap>(CAST(receiver), JSMap::kTableOffset);
    number_of_buckets = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
        table_var.value(), OrderedHashMap::NumberOfBucketsIndex())));
    const TNode<IntPtrT> new_number_of_elements = SmiUntag(CAST(LoadObjectField(
        table_var.value(), OrderedHashMap::NumberOfElementsOffset())));
    const TNode<IntPtrT> new_number_of_deleted = SmiUntag(CAST(LoadObjectField(
        table_var.value(), OrderedHashMap::NumberOfDeletedElementsOffset())));
    occupancy = IntPtrAdd(new_number_of_elements, new_number_of_deleted);
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
    const TNode<OrderedHashMap> table, const TNode<Object> key,
    const TNode<Object> value, const TNode<IntPtrT> hash,
    const TNode<IntPtrT> number_of_buckets, const TNode<IntPtrT> occupancy) {
  const TNode<IntPtrT> bucket =
      WordAnd(hash, IntPtrSub(number_of_buckets, IntPtrConstant(1)));
  TNode<Smi> bucket_entry = CAST(UnsafeLoadFixedArrayElement(
      table, bucket, OrderedHashMap::HashTableStartIndex() * kTaggedSize));

  // Store the entry elements.
  const TNode<IntPtrT> entry_start = IntPtrAdd(
      IntPtrMul(occupancy, IntPtrConstant(OrderedHashMap::kEntrySize)),
      number_of_buckets);
  UnsafeStoreFixedArrayElement(
      table, entry_start, key, UPDATE_WRITE_BARRIER,
      kTaggedSize * OrderedHashMap::HashTableStartIndex());
  UnsafeStoreFixedArrayElement(
      table, entry_start, value, UPDATE_WRITE_BARRIER,
      kTaggedSize * (OrderedHashMap::HashTableStartIndex() +
                     OrderedHashMap::kValueOffset));
  UnsafeStoreFixedArrayElement(
      table, entry_start, bucket_entry,
      kTaggedSize * (OrderedHashMap::HashTableStartIndex() +
                     OrderedHashMap::kChainOffset));

  // Update the bucket head.
  UnsafeStoreFixedArrayElement(
      table, bucket, SmiTag(occupancy),
      OrderedHashMap::HashTableStartIndex() * kTaggedSize);

  // Bump the elements count.
  const TNode<Smi> number_of_elements =
      CAST(LoadObjectField(table, OrderedHashMap::NumberOfElementsOffset()));
  StoreObjectFieldNoWriteBarrier(table,
                                 OrderedHashMap::NumberOfElementsOffset(),
                                 SmiAdd(number_of_elements, SmiConstant(1)));
}

TF_BUILTIN(MapPrototypeDelete, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto key = Parameter<Object>(Descriptor::kKey);
  const auto context = Parameter<Context>(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE,
                         "Map.prototype.delete");

  const TNode<OrderedHashMap> table =
      LoadObjectField<OrderedHashMap>(CAST(receiver), JSMap::kTableOffset);

  TVARIABLE(IntPtrT, entry_start_position_or_hash, IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashMap>(
      table, key, &entry_start_position_or_hash, &entry_found, &not_found);

  BIND(&not_found);
  Return(FalseConstant());

  BIND(&entry_found);
  // If we found the entry, mark the entry as deleted.
  StoreFixedArrayElement(table, entry_start_position_or_hash.value(),
                         TheHoleConstant(), UPDATE_WRITE_BARRIER,
                         kTaggedSize * OrderedHashMap::HashTableStartIndex());
  StoreFixedArrayElement(table, entry_start_position_or_hash.value(),
                         TheHoleConstant(), UPDATE_WRITE_BARRIER,
                         kTaggedSize * (OrderedHashMap::HashTableStartIndex() +
                                        OrderedHashMap::kValueOffset));

  // Decrement the number of elements, increment the number of deleted elements.
  const TNode<Smi> number_of_elements = SmiSub(
      CAST(LoadObjectField(table, OrderedHashMap::NumberOfElementsOffset())),
      SmiConstant(1));
  StoreObjectFieldNoWriteBarrier(
      table, OrderedHashMap::NumberOfElementsOffset(), number_of_elements);
  const TNode<Smi> number_of_deleted =
      SmiAdd(CAST(LoadObjectField(
                 table, OrderedHashMap::NumberOfDeletedElementsOffset())),
             SmiConstant(1));
  StoreObjectFieldNoWriteBarrier(
      table, OrderedHashMap::NumberOfDeletedElementsOffset(),
      number_of_deleted);

  const TNode<Smi> number_of_buckets = CAST(
      LoadFixedArrayElement(table, OrderedHashMap::NumberOfBucketsIndex()));

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
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  auto key = Parameter<Object>(Descriptor::kKey);
  const auto context = Parameter<Context>(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE, "Set.prototype.add");

  key = NormalizeNumberKey(key);

  const TNode<OrderedHashSet> table =
      LoadObjectField<OrderedHashSet>(CAST(receiver), JSMap::kTableOffset);

  TVARIABLE(IntPtrT, entry_start_position_or_hash, IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashSet>(
      table, key, &entry_start_position_or_hash, &entry_found, &not_found);

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
    entry_start_position_or_hash = SmiUntag(CallGetOrCreateHashRaw(CAST(key)));
    Goto(&add_entry);
  }

  BIND(&add_entry);
  TVARIABLE(IntPtrT, number_of_buckets);
  TVARIABLE(IntPtrT, occupancy);
  TVARIABLE(OrderedHashSet, table_var, table);
  {
    // Check we have enough space for the entry.
    number_of_buckets = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
        table, OrderedHashSet::NumberOfBucketsIndex())));

    STATIC_ASSERT(OrderedHashSet::kLoadFactor == 2);
    const TNode<WordT> capacity = WordShl(number_of_buckets.value(), 1);
    const TNode<IntPtrT> number_of_elements = SmiUntag(
        CAST(LoadObjectField(table, OrderedHashSet::NumberOfElementsOffset())));
    const TNode<IntPtrT> number_of_deleted = SmiUntag(CAST(LoadObjectField(
        table, OrderedHashSet::NumberOfDeletedElementsOffset())));
    occupancy = IntPtrAdd(number_of_elements, number_of_deleted);
    GotoIf(IntPtrLessThan(occupancy.value(), capacity), &store_new_entry);

    // We do not have enough space, grow the table and reload the relevant
    // fields.
    CallRuntime(Runtime::kSetGrow, context, receiver);
    table_var =
        LoadObjectField<OrderedHashSet>(CAST(receiver), JSMap::kTableOffset);
    number_of_buckets = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
        table_var.value(), OrderedHashSet::NumberOfBucketsIndex())));
    const TNode<IntPtrT> new_number_of_elements = SmiUntag(CAST(LoadObjectField(
        table_var.value(), OrderedHashSet::NumberOfElementsOffset())));
    const TNode<IntPtrT> new_number_of_deleted = SmiUntag(CAST(LoadObjectField(
        table_var.value(), OrderedHashSet::NumberOfDeletedElementsOffset())));
    occupancy = IntPtrAdd(new_number_of_elements, new_number_of_deleted);
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
    const TNode<OrderedHashSet> table, const TNode<Object> key,
    const TNode<IntPtrT> hash, const TNode<IntPtrT> number_of_buckets,
    const TNode<IntPtrT> occupancy) {
  const TNode<IntPtrT> bucket =
      WordAnd(hash, IntPtrSub(number_of_buckets, IntPtrConstant(1)));
  TNode<Smi> bucket_entry = CAST(UnsafeLoadFixedArrayElement(
      table, bucket, OrderedHashSet::HashTableStartIndex() * kTaggedSize));

  // Store the entry elements.
  const TNode<IntPtrT> entry_start = IntPtrAdd(
      IntPtrMul(occupancy, IntPtrConstant(OrderedHashSet::kEntrySize)),
      number_of_buckets);
  UnsafeStoreFixedArrayElement(
      table, entry_start, key, UPDATE_WRITE_BARRIER,
      kTaggedSize * OrderedHashSet::HashTableStartIndex());
  UnsafeStoreFixedArrayElement(
      table, entry_start, bucket_entry,
      kTaggedSize * (OrderedHashSet::HashTableStartIndex() +
                     OrderedHashSet::kChainOffset));

  // Update the bucket head.
  UnsafeStoreFixedArrayElement(
      table, bucket, SmiTag(occupancy),
      OrderedHashSet::HashTableStartIndex() * kTaggedSize);

  // Bump the elements count.
  const TNode<Smi> number_of_elements =
      CAST(LoadObjectField(table, OrderedHashSet::NumberOfElementsOffset()));
  StoreObjectFieldNoWriteBarrier(table,
                                 OrderedHashSet::NumberOfElementsOffset(),
                                 SmiAdd(number_of_elements, SmiConstant(1)));
}

TF_BUILTIN(SetPrototypeDelete, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto key = Parameter<Object>(Descriptor::kKey);
  const auto context = Parameter<Context>(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE,
                         "Set.prototype.delete");

  const TNode<OrderedHashSet> table =
      LoadObjectField<OrderedHashSet>(CAST(receiver), JSMap::kTableOffset);

  TVARIABLE(IntPtrT, entry_start_position_or_hash, IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashSet>(
      table, key, &entry_start_position_or_hash, &entry_found, &not_found);

  BIND(&not_found);
  Return(FalseConstant());

  BIND(&entry_found);
  // If we found the entry, mark the entry as deleted.
  StoreFixedArrayElement(table, entry_start_position_or_hash.value(),
                         TheHoleConstant(), UPDATE_WRITE_BARRIER,
                         kTaggedSize * OrderedHashSet::HashTableStartIndex());

  // Decrement the number of elements, increment the number of deleted elements.
  const TNode<Smi> number_of_elements = SmiSub(
      CAST(LoadObjectField(table, OrderedHashSet::NumberOfElementsOffset())),
      SmiConstant(1));
  StoreObjectFieldNoWriteBarrier(
      table, OrderedHashSet::NumberOfElementsOffset(), number_of_elements);
  const TNode<Smi> number_of_deleted =
      SmiAdd(CAST(LoadObjectField(
                 table, OrderedHashSet::NumberOfDeletedElementsOffset())),
             SmiConstant(1));
  StoreObjectFieldNoWriteBarrier(
      table, OrderedHashSet::NumberOfDeletedElementsOffset(),
      number_of_deleted);

  const TNode<Smi> number_of_buckets = CAST(
      LoadFixedArrayElement(table, OrderedHashSet::NumberOfBucketsIndex()));

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
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto context = Parameter<Context>(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE,
                         "Map.prototype.entries");
  Return(AllocateJSCollectionIterator<JSMapIterator>(
      context, Context::MAP_KEY_VALUE_ITERATOR_MAP_INDEX, CAST(receiver)));
}

TF_BUILTIN(MapPrototypeGetSize, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto context = Parameter<Context>(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE,
                         "get Map.prototype.size");
  const TNode<OrderedHashMap> table =
      LoadObjectField<OrderedHashMap>(CAST(receiver), JSMap::kTableOffset);
  Return(LoadObjectField(table, OrderedHashMap::NumberOfElementsOffset()));
}

TF_BUILTIN(MapPrototypeForEach, CollectionsBuiltinsAssembler) {
  const char* const kMethodName = "Map.prototype.forEach";
  auto argc = UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount);
  const auto context = Parameter<Context>(Descriptor::kContext);
  CodeStubArguments args(this, argc);
  const TNode<Object> receiver = args.GetReceiver();
  const TNode<Object> callback = args.GetOptionalArgumentValue(0);
  const TNode<Object> this_arg = args.GetOptionalArgumentValue(1);

  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, kMethodName);

  // Ensure that {callback} is actually callable.
  Label callback_not_callable(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(callback), &callback_not_callable);
  GotoIfNot(IsCallable(CAST(callback)), &callback_not_callable);

  TVARIABLE(IntPtrT, var_index, IntPtrConstant(0));
  TVARIABLE(OrderedHashMap, var_table,
            CAST(LoadObjectField(CAST(receiver), JSMap::kTableOffset)));
  Label loop(this, {&var_index, &var_table}), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Transition {table} and {index} if there was any modification to
    // the {receiver} while we're iterating.
    TNode<IntPtrT> index = var_index.value();
    TNode<OrderedHashMap> table = var_table.value();
    std::tie(table, index) = Transition<OrderedHashMap>(
        table, index, [](const TNode<OrderedHashMap>, const TNode<IntPtrT>) {});

    // Read the next entry from the {table}, skipping holes.
    TNode<Object> entry_key;
    TNode<IntPtrT> entry_start_position;
    std::tie(entry_key, entry_start_position, index) =
        NextSkipHoles<OrderedHashMap>(table, index, &done_loop);

    // Load the entry value as well.
    TNode<Object> entry_value = LoadFixedArrayElement(
        table, entry_start_position,
        (OrderedHashMap::HashTableStartIndex() + OrderedHashMap::kValueOffset) *
            kTaggedSize);

    // Invoke the {callback} passing the {entry_key}, {entry_value} and the
    // {receiver}.
    Call(context, callback, this_arg, entry_value, entry_key, receiver);

    // Continue with the next entry.
    var_index = index;
    var_table = table;
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
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto context = Parameter<Context>(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE, "Map.prototype.keys");
  Return(AllocateJSCollectionIterator<JSMapIterator>(
      context, Context::MAP_KEY_ITERATOR_MAP_INDEX, CAST(receiver)));
}

TF_BUILTIN(MapPrototypeValues, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto context = Parameter<Context>(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_MAP_TYPE,
                         "Map.prototype.values");
  Return(AllocateJSCollectionIterator<JSMapIterator>(
      context, Context::MAP_VALUE_ITERATOR_MAP_INDEX, CAST(receiver)));
}

TF_BUILTIN(MapIteratorPrototypeNext, CollectionsBuiltinsAssembler) {
  const char* const kMethodName = "Map Iterator.prototype.next";
  const auto maybe_receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto context = Parameter<Context>(Descriptor::kContext);

  // Ensure that {maybe_receiver} is actually a JSMapIterator.
  Label if_receiver_valid(this), if_receiver_invalid(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(maybe_receiver), &if_receiver_invalid);
  const TNode<Uint16T> receiver_instance_type =
      LoadInstanceType(CAST(maybe_receiver));
  GotoIf(
      InstanceTypeEqual(receiver_instance_type, JS_MAP_KEY_VALUE_ITERATOR_TYPE),
      &if_receiver_valid);
  GotoIf(InstanceTypeEqual(receiver_instance_type, JS_MAP_KEY_ITERATOR_TYPE),
         &if_receiver_valid);
  Branch(InstanceTypeEqual(receiver_instance_type, JS_MAP_VALUE_ITERATOR_TYPE),
         &if_receiver_valid, &if_receiver_invalid);
  BIND(&if_receiver_invalid);
  ThrowTypeError(context, MessageTemplate::kIncompatibleMethodReceiver,
                 StringConstant(kMethodName), maybe_receiver);
  BIND(&if_receiver_valid);
  TNode<JSMapIterator> receiver = CAST(maybe_receiver);

  // Check if the {receiver} is exhausted.
  TVARIABLE(Oddball, var_done, TrueConstant());
  TVARIABLE(Object, var_value, UndefinedConstant());
  Label return_value(this, {&var_done, &var_value}), return_entry(this),
      return_end(this, Label::kDeferred);

  // Transition the {receiver} table if necessary.
  TNode<OrderedHashMap> table;
  TNode<IntPtrT> index;
  std::tie(table, index) =
      TransitionAndUpdate<JSMapIterator, OrderedHashMap>(receiver);

  // Read the next entry from the {table}, skipping holes.
  TNode<Object> entry_key;
  TNode<IntPtrT> entry_start_position;
  std::tie(entry_key, entry_start_position, index) =
      NextSkipHoles<OrderedHashMap>(table, index, &return_end);
  StoreObjectFieldNoWriteBarrier(receiver, JSMapIterator::kIndexOffset,
                                 SmiTag(index));
  var_value = entry_key;
  var_done = FalseConstant();

  // Check how to return the {key} (depending on {receiver} type).
  GotoIf(InstanceTypeEqual(receiver_instance_type, JS_MAP_KEY_ITERATOR_TYPE),
         &return_value);
  var_value = LoadFixedArrayElement(
      table, entry_start_position,
      (OrderedHashMap::HashTableStartIndex() + OrderedHashMap::kValueOffset) *
          kTaggedSize);
  Branch(InstanceTypeEqual(receiver_instance_type, JS_MAP_VALUE_ITERATOR_TYPE),
         &return_value, &return_entry);

  BIND(&return_entry);
  {
    TNode<JSObject> result =
        AllocateJSIteratorResultForEntry(context, entry_key, var_value.value());
    Return(result);
  }

  BIND(&return_value);
  {
    TNode<JSObject> result =
        AllocateJSIteratorResult(context, var_value.value(), var_done.value());
    Return(result);
  }

  BIND(&return_end);
  {
    StoreObjectFieldRoot(receiver, JSMapIterator::kTableOffset,
                         RootIndex::kEmptyOrderedHashMap);
    Goto(&return_value);
  }
}

TF_BUILTIN(SetPrototypeHas, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto key = Parameter<Object>(Descriptor::kKey);
  const auto context = Parameter<Context>(Descriptor::kContext);

  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE, "Set.prototype.has");

  const TNode<Object> table =
      LoadObjectField(CAST(receiver), JSMap::kTableOffset);

  TVARIABLE(IntPtrT, entry_start_position, IntPtrConstant(0));
  Label if_key_smi(this), if_key_string(this), if_key_heap_number(this),
      if_key_bigint(this), entry_found(this), not_found(this), done(this);

  GotoIf(TaggedIsSmi(key), &if_key_smi);

  TNode<Map> key_map = LoadMap(CAST(key));
  TNode<Uint16T> key_instance_type = LoadMapInstanceType(key_map);

  GotoIf(IsStringInstanceType(key_instance_type), &if_key_string);
  GotoIf(IsHeapNumberMap(key_map), &if_key_heap_number);
  GotoIf(IsBigIntInstanceType(key_instance_type), &if_key_bigint);

  FindOrderedHashTableEntryForOtherKey<OrderedHashSet>(
      CAST(table), CAST(key), &entry_start_position, &entry_found, &not_found);

  BIND(&if_key_smi);
  {
    FindOrderedHashTableEntryForSmiKey<OrderedHashSet>(
        CAST(table), CAST(key), &entry_start_position, &entry_found,
        &not_found);
  }

  BIND(&if_key_string);
  {
    FindOrderedHashTableEntryForStringKey<OrderedHashSet>(
        CAST(table), CAST(key), &entry_start_position, &entry_found,
        &not_found);
  }

  BIND(&if_key_heap_number);
  {
    FindOrderedHashTableEntryForHeapNumberKey<OrderedHashSet>(
        CAST(table), CAST(key), &entry_start_position, &entry_found,
        &not_found);
  }

  BIND(&if_key_bigint);
  {
    FindOrderedHashTableEntryForBigIntKey<OrderedHashSet>(
        CAST(table), CAST(key), &entry_start_position, &entry_found,
        &not_found);
  }

  BIND(&entry_found);
  Return(TrueConstant());

  BIND(&not_found);
  Return(FalseConstant());
}

TF_BUILTIN(SetPrototypeEntries, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto context = Parameter<Context>(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE,
                         "Set.prototype.entries");
  Return(AllocateJSCollectionIterator<JSSetIterator>(
      context, Context::SET_KEY_VALUE_ITERATOR_MAP_INDEX, CAST(receiver)));
}

TF_BUILTIN(SetPrototypeGetSize, CollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto context = Parameter<Context>(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE,
                         "get Set.prototype.size");
  const TNode<OrderedHashSet> table =
      LoadObjectField<OrderedHashSet>(CAST(receiver), JSSet::kTableOffset);
  Return(LoadObjectField(table, OrderedHashSet::NumberOfElementsOffset()));
}

TF_BUILTIN(SetPrototypeForEach, CollectionsBuiltinsAssembler) {
  const char* const kMethodName = "Set.prototype.forEach";
  auto argc = UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount);
  const auto context = Parameter<Context>(Descriptor::kContext);
  CodeStubArguments args(this, argc);
  const TNode<Object> receiver = args.GetReceiver();
  const TNode<Object> callback = args.GetOptionalArgumentValue(0);
  const TNode<Object> this_arg = args.GetOptionalArgumentValue(1);

  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE, kMethodName);

  // Ensure that {callback} is actually callable.
  Label callback_not_callable(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(callback), &callback_not_callable);
  GotoIfNot(IsCallable(CAST(callback)), &callback_not_callable);

  TVARIABLE(IntPtrT, var_index, IntPtrConstant(0));
  TVARIABLE(OrderedHashSet, var_table,
            CAST(LoadObjectField(CAST(receiver), JSSet::kTableOffset)));
  Label loop(this, {&var_index, &var_table}), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Transition {table} and {index} if there was any modification to
    // the {receiver} while we're iterating.
    TNode<IntPtrT> index = var_index.value();
    TNode<OrderedHashSet> table = var_table.value();
    std::tie(table, index) = Transition<OrderedHashSet>(
        table, index, [](const TNode<OrderedHashSet>, const TNode<IntPtrT>) {});

    // Read the next entry from the {table}, skipping holes.
    TNode<Object> entry_key;
    TNode<IntPtrT> entry_start_position;
    std::tie(entry_key, entry_start_position, index) =
        NextSkipHoles<OrderedHashSet>(table, index, &done_loop);

    // Invoke the {callback} passing the {entry_key} (twice) and the {receiver}.
    Call(context, callback, this_arg, entry_key, entry_key, receiver);

    // Continue with the next entry.
    var_index = index;
    var_table = table;
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
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto context = Parameter<Context>(Descriptor::kContext);
  ThrowIfNotInstanceType(context, receiver, JS_SET_TYPE,
                         "Set.prototype.values");
  Return(AllocateJSCollectionIterator<JSSetIterator>(
      context, Context::SET_VALUE_ITERATOR_MAP_INDEX, CAST(receiver)));
}

TF_BUILTIN(SetIteratorPrototypeNext, CollectionsBuiltinsAssembler) {
  const char* const kMethodName = "Set Iterator.prototype.next";
  const auto maybe_receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto context = Parameter<Context>(Descriptor::kContext);

  // Ensure that {maybe_receiver} is actually a JSSetIterator.
  Label if_receiver_valid(this), if_receiver_invalid(this, Label::kDeferred);
  GotoIf(TaggedIsSmi(maybe_receiver), &if_receiver_invalid);
  const TNode<Uint16T> receiver_instance_type =
      LoadInstanceType(CAST(maybe_receiver));
  GotoIf(InstanceTypeEqual(receiver_instance_type, JS_SET_VALUE_ITERATOR_TYPE),
         &if_receiver_valid);
  Branch(
      InstanceTypeEqual(receiver_instance_type, JS_SET_KEY_VALUE_ITERATOR_TYPE),
      &if_receiver_valid, &if_receiver_invalid);
  BIND(&if_receiver_invalid);
  ThrowTypeError(context, MessageTemplate::kIncompatibleMethodReceiver,
                 StringConstant(kMethodName), maybe_receiver);
  BIND(&if_receiver_valid);

  TNode<JSSetIterator> receiver = CAST(maybe_receiver);

  // Check if the {receiver} is exhausted.
  TVARIABLE(Oddball, var_done, TrueConstant());
  TVARIABLE(Object, var_value, UndefinedConstant());
  Label return_value(this, {&var_done, &var_value}), return_entry(this),
      return_end(this, Label::kDeferred);

  // Transition the {receiver} table if necessary.
  TNode<OrderedHashSet> table;
  TNode<IntPtrT> index;
  std::tie(table, index) =
      TransitionAndUpdate<JSSetIterator, OrderedHashSet>(receiver);

  // Read the next entry from the {table}, skipping holes.
  TNode<Object> entry_key;
  TNode<IntPtrT> entry_start_position;
  std::tie(entry_key, entry_start_position, index) =
      NextSkipHoles<OrderedHashSet>(table, index, &return_end);
  StoreObjectFieldNoWriteBarrier(receiver, JSSetIterator::kIndexOffset,
                                 SmiTag(index));
  var_value = entry_key;
  var_done = FalseConstant();

  // Check how to return the {key} (depending on {receiver} type).
  Branch(InstanceTypeEqual(receiver_instance_type, JS_SET_VALUE_ITERATOR_TYPE),
         &return_value, &return_entry);

  BIND(&return_entry);
  {
    TNode<JSObject> result = AllocateJSIteratorResultForEntry(
        context, var_value.value(), var_value.value());
    Return(result);
  }

  BIND(&return_value);
  {
    TNode<JSObject> result =
        AllocateJSIteratorResult(context, var_value.value(), var_done.value());
    Return(result);
  }

  BIND(&return_end);
  {
    StoreObjectFieldRoot(receiver, JSSetIterator::kTableOffset,
                         RootIndex::kEmptyOrderedHashSet);
    Goto(&return_value);
  }
}

template <typename CollectionType>
void CollectionsBuiltinsAssembler::TryLookupOrderedHashTableIndex(
    const TNode<CollectionType> table, const TNode<Object> key,
    TVariable<IntPtrT>* result, Label* if_entry_found, Label* if_not_found) {
  Label if_key_smi(this), if_key_string(this), if_key_heap_number(this),
      if_key_bigint(this);

  GotoIf(TaggedIsSmi(key), &if_key_smi);

  TNode<Map> key_map = LoadMap(CAST(key));
  TNode<Uint16T> key_instance_type = LoadMapInstanceType(key_map);

  GotoIf(IsStringInstanceType(key_instance_type), &if_key_string);
  GotoIf(IsHeapNumberMap(key_map), &if_key_heap_number);
  GotoIf(IsBigIntInstanceType(key_instance_type), &if_key_bigint);

  FindOrderedHashTableEntryForOtherKey<CollectionType>(
      table, CAST(key), result, if_entry_found, if_not_found);

  BIND(&if_key_smi);
  {
    FindOrderedHashTableEntryForSmiKey<CollectionType>(
        table, CAST(key), result, if_entry_found, if_not_found);
  }

  BIND(&if_key_string);
  {
    FindOrderedHashTableEntryForStringKey<CollectionType>(
        table, CAST(key), result, if_entry_found, if_not_found);
  }

  BIND(&if_key_heap_number);
  {
    FindOrderedHashTableEntryForHeapNumberKey<CollectionType>(
        table, CAST(key), result, if_entry_found, if_not_found);
  }

  BIND(&if_key_bigint);
  {
    FindOrderedHashTableEntryForBigIntKey<CollectionType>(
        table, CAST(key), result, if_entry_found, if_not_found);
  }
}

TF_BUILTIN(FindOrderedHashMapEntry, CollectionsBuiltinsAssembler) {
  const auto table = Parameter<OrderedHashMap>(Descriptor::kTable);
  const auto key = Parameter<Object>(Descriptor::kKey);

  TVARIABLE(IntPtrT, entry_start_position, IntPtrConstant(0));
  Label entry_found(this), not_found(this);

  TryLookupOrderedHashTableIndex<OrderedHashMap>(
      table, key, &entry_start_position, &entry_found, &not_found);

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
  void AddEntry(TNode<EphemeronHashTable> table, TNode<IntPtrT> key_index,
                TNode<Object> key, TNode<Object> value,
                TNode<IntPtrT> number_of_elements);

  TNode<HeapObject> AllocateTable(Variant variant,
                                  TNode<IntPtrT> at_least_space_for) override;

  // Generates and sets the identity for a JSRececiver.
  TNode<Smi> CreateIdentityHash(TNode<Object> receiver);
  TNode<IntPtrT> EntryMask(TNode<IntPtrT> capacity);

  // Builds code that finds the EphemeronHashTable entry for a {key} using the
  // comparison code generated by {key_compare}. The key index is returned if
  // the {key} is found.
  using KeyComparator =
      std::function<void(TNode<Object> entry_key, Label* if_same)>;
  TNode<IntPtrT> FindKeyIndex(TNode<HeapObject> table, TNode<IntPtrT> key_hash,
                              TNode<IntPtrT> entry_mask,
                              const KeyComparator& key_compare);

  // Builds code that finds an EphemeronHashTable entry available for a new
  // entry.
  TNode<IntPtrT> FindKeyIndexForInsertion(TNode<HeapObject> table,
                                          TNode<IntPtrT> key_hash,
                                          TNode<IntPtrT> entry_mask);

  // Builds code that finds the EphemeronHashTable entry with key that matches
  // {key} and returns the entry's key index. If {key} cannot be found, jumps to
  // {if_not_found}.
  TNode<IntPtrT> FindKeyIndexForKey(TNode<HeapObject> table, TNode<Object> key,
                                    TNode<IntPtrT> hash,
                                    TNode<IntPtrT> entry_mask,
                                    Label* if_not_found);

  TNode<Word32T> InsufficientCapacityToAdd(TNode<IntPtrT> capacity,
                                           TNode<IntPtrT> number_of_elements,
                                           TNode<IntPtrT> number_of_deleted);
  TNode<IntPtrT> KeyIndexFromEntry(TNode<IntPtrT> entry);

  TNode<IntPtrT> LoadNumberOfElements(TNode<EphemeronHashTable> table,
                                      int offset);
  TNode<IntPtrT> LoadNumberOfDeleted(TNode<EphemeronHashTable> table,
                                     int offset = 0);
  TNode<EphemeronHashTable> LoadTable(TNode<JSWeakCollection> collection);
  TNode<IntPtrT> LoadTableCapacity(TNode<EphemeronHashTable> table);

  void RemoveEntry(TNode<EphemeronHashTable> table, TNode<IntPtrT> key_index,
                   TNode<IntPtrT> number_of_elements);
  TNode<BoolT> ShouldRehash(TNode<IntPtrT> number_of_elements,
                            TNode<IntPtrT> number_of_deleted);
  TNode<Word32T> ShouldShrink(TNode<IntPtrT> capacity,
                              TNode<IntPtrT> number_of_elements);
  TNode<IntPtrT> ValueIndexFromKeyIndex(TNode<IntPtrT> key_index);
};

void WeakCollectionsBuiltinsAssembler::AddEntry(
    TNode<EphemeronHashTable> table, TNode<IntPtrT> key_index,
    TNode<Object> key, TNode<Object> value, TNode<IntPtrT> number_of_elements) {
  // See EphemeronHashTable::AddEntry().
  TNode<IntPtrT> value_index = ValueIndexFromKeyIndex(key_index);
  UnsafeStoreFixedArrayElement(table, key_index, key,
                               UPDATE_EPHEMERON_KEY_WRITE_BARRIER);
  UnsafeStoreFixedArrayElement(table, value_index, value);

  // See HashTableBase::ElementAdded().
  UnsafeStoreFixedArrayElement(table,
                               EphemeronHashTable::kNumberOfElementsIndex,
                               SmiFromIntPtr(number_of_elements));
}

TNode<HeapObject> WeakCollectionsBuiltinsAssembler::AllocateTable(
    Variant variant, TNode<IntPtrT> at_least_space_for) {
  // See HashTable::New().
  CSA_ASSERT(this,
             IntPtrLessThanOrEqual(IntPtrConstant(0), at_least_space_for));
  TNode<IntPtrT> capacity = HashTableComputeCapacity(at_least_space_for);

  // See HashTable::NewInternal().
  TNode<IntPtrT> length = KeyIndexFromEntry(capacity);
  TNode<FixedArray> table = CAST(
      AllocateFixedArray(HOLEY_ELEMENTS, length, kAllowLargeObjectAllocation));

  TNode<Map> map =
      HeapConstant(EphemeronHashTable::GetMap(ReadOnlyRoots(isolate())));
  StoreMapNoWriteBarrier(table, map);
  StoreFixedArrayElement(table, EphemeronHashTable::kNumberOfElementsIndex,
                         SmiConstant(0), SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(table,
                         EphemeronHashTable::kNumberOfDeletedElementsIndex,
                         SmiConstant(0), SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(table, EphemeronHashTable::kCapacityIndex,
                         SmiFromIntPtr(capacity), SKIP_WRITE_BARRIER);

  TNode<IntPtrT> start = KeyIndexFromEntry(IntPtrConstant(0));
  FillFixedArrayWithValue(HOLEY_ELEMENTS, table, start, length,
                          RootIndex::kUndefinedValue);
  return table;
}

TNode<Smi> WeakCollectionsBuiltinsAssembler::CreateIdentityHash(
    TNode<Object> key) {
  TNode<ExternalReference> function_addr =
      ExternalConstant(ExternalReference::jsreceiver_create_identity_hash());
  TNode<ExternalReference> isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_tagged = MachineType::AnyTagged();

  return CAST(CallCFunction(function_addr, type_tagged,
                            std::make_pair(type_ptr, isolate_ptr),
                            std::make_pair(type_tagged, key)));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::EntryMask(
    TNode<IntPtrT> capacity) {
  return IntPtrSub(capacity, IntPtrConstant(1));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::FindKeyIndex(
    TNode<HeapObject> table, TNode<IntPtrT> key_hash, TNode<IntPtrT> entry_mask,
    const KeyComparator& key_compare) {
  // See HashTable::FirstProbe().
  TVARIABLE(IntPtrT, var_entry, WordAnd(key_hash, entry_mask));
  TVARIABLE(IntPtrT, var_count, IntPtrConstant(0));

  Label loop(this, {&var_count, &var_entry}), if_found(this);
  Goto(&loop);
  BIND(&loop);
  TNode<IntPtrT> key_index;
  {
    key_index = KeyIndexFromEntry(var_entry.value());
    TNode<Object> entry_key =
        UnsafeLoadFixedArrayElement(CAST(table), key_index);

    key_compare(entry_key, &if_found);

    // See HashTable::NextProbe().
    Increment(&var_count);
    var_entry =
        WordAnd(IntPtrAdd(var_entry.value(), var_count.value()), entry_mask);
    Goto(&loop);
  }

  BIND(&if_found);
  return key_index;
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::FindKeyIndexForInsertion(
    TNode<HeapObject> table, TNode<IntPtrT> key_hash,
    TNode<IntPtrT> entry_mask) {
  // See HashTable::FindInsertionEntry().
  auto is_not_live = [&](TNode<Object> entry_key, Label* if_found) {
    // This is the the negative form BaseShape::IsLive().
    GotoIf(Word32Or(IsTheHole(entry_key), IsUndefined(entry_key)), if_found);
  };
  return FindKeyIndex(table, key_hash, entry_mask, is_not_live);
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::FindKeyIndexForKey(
    TNode<HeapObject> table, TNode<Object> key, TNode<IntPtrT> hash,
    TNode<IntPtrT> entry_mask, Label* if_not_found) {
  // See HashTable::FindEntry().
  auto match_key_or_exit_on_empty = [&](TNode<Object> entry_key,
                                        Label* if_same) {
    GotoIf(IsUndefined(entry_key), if_not_found);
    GotoIf(TaggedEqual(entry_key, key), if_same);
  };
  return FindKeyIndex(table, hash, entry_mask, match_key_or_exit_on_empty);
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::KeyIndexFromEntry(
    TNode<IntPtrT> entry) {
  // See HashTable::KeyAt().
  // (entry * kEntrySize) + kElementsStartIndex + kEntryKeyIndex
  return IntPtrAdd(
      IntPtrMul(entry, IntPtrConstant(EphemeronHashTable::kEntrySize)),
      IntPtrConstant(EphemeronHashTable::kElementsStartIndex +
                     EphemeronHashTable::kEntryKeyIndex));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::LoadNumberOfElements(
    TNode<EphemeronHashTable> table, int offset) {
  TNode<IntPtrT> number_of_elements = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
      table, EphemeronHashTable::kNumberOfElementsIndex)));
  return IntPtrAdd(number_of_elements, IntPtrConstant(offset));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::LoadNumberOfDeleted(
    TNode<EphemeronHashTable> table, int offset) {
  TNode<IntPtrT> number_of_deleted = SmiUntag(CAST(UnsafeLoadFixedArrayElement(
      table, EphemeronHashTable::kNumberOfDeletedElementsIndex)));
  return IntPtrAdd(number_of_deleted, IntPtrConstant(offset));
}

TNode<EphemeronHashTable> WeakCollectionsBuiltinsAssembler::LoadTable(
    TNode<JSWeakCollection> collection) {
  return CAST(LoadObjectField(collection, JSWeakCollection::kTableOffset));
}

TNode<IntPtrT> WeakCollectionsBuiltinsAssembler::LoadTableCapacity(
    TNode<EphemeronHashTable> table) {
  return SmiUntag(CAST(
      UnsafeLoadFixedArrayElement(table, EphemeronHashTable::kCapacityIndex)));
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
    TNode<EphemeronHashTable> table, TNode<IntPtrT> key_index,
    TNode<IntPtrT> number_of_elements) {
  // See EphemeronHashTable::RemoveEntry().
  TNode<IntPtrT> value_index = ValueIndexFromKeyIndex(key_index);
  StoreFixedArrayElement(table, key_index, TheHoleConstant());
  StoreFixedArrayElement(table, value_index, TheHoleConstant());

  // See HashTableBase::ElementRemoved().
  TNode<IntPtrT> number_of_deleted = LoadNumberOfDeleted(table, 1);
  StoreFixedArrayElement(table, EphemeronHashTable::kNumberOfElementsIndex,
                         SmiFromIntPtr(number_of_elements), SKIP_WRITE_BARRIER);
  StoreFixedArrayElement(table,
                         EphemeronHashTable::kNumberOfDeletedElementsIndex,
                         SmiFromIntPtr(number_of_deleted), SKIP_WRITE_BARRIER);
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
                   IntPtrConstant(EphemeronHashTable::ShapeT::kEntryValueIndex -
                                  EphemeronHashTable::kEntryKeyIndex));
}

TF_BUILTIN(WeakMapConstructor, WeakCollectionsBuiltinsAssembler) {
  auto new_target = Parameter<Object>(Descriptor::kJSNewTarget);
  TNode<IntPtrT> argc = ChangeInt32ToIntPtr(
      UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount));
  auto context = Parameter<Context>(Descriptor::kContext);

  GenerateConstructor(kWeakMap, isolate()->factory()->WeakMap_string(),
                      new_target, argc, context);
}

TF_BUILTIN(WeakSetConstructor, WeakCollectionsBuiltinsAssembler) {
  auto new_target = Parameter<Object>(Descriptor::kJSNewTarget);
  TNode<IntPtrT> argc = ChangeInt32ToIntPtr(
      UncheckedParameter<Int32T>(Descriptor::kJSActualArgumentsCount));
  auto context = Parameter<Context>(Descriptor::kContext);

  GenerateConstructor(kWeakSet, isolate()->factory()->WeakSet_string(),
                      new_target, argc, context);
}

TF_BUILTIN(WeakMapLookupHashIndex, WeakCollectionsBuiltinsAssembler) {
  auto table = Parameter<EphemeronHashTable>(Descriptor::kTable);
  auto key = Parameter<Object>(Descriptor::kKey);

  Label if_not_found(this);

  GotoIfNotJSReceiver(key, &if_not_found);

  TNode<IntPtrT> hash = LoadJSReceiverIdentityHash(CAST(key), &if_not_found);
  TNode<IntPtrT> capacity = LoadTableCapacity(table);
  TNode<IntPtrT> key_index =
      FindKeyIndexForKey(table, key, hash, EntryMask(capacity), &if_not_found);
  Return(SmiTag(ValueIndexFromKeyIndex(key_index)));

  BIND(&if_not_found);
  Return(SmiConstant(-1));
}

TF_BUILTIN(WeakMapGet, WeakCollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto key = Parameter<Object>(Descriptor::kKey);
  const auto context = Parameter<Context>(Descriptor::kContext);

  Label return_undefined(this);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_MAP_TYPE,
                         "WeakMap.prototype.get");

  const TNode<EphemeronHashTable> table = LoadTable(CAST(receiver));
  const TNode<Smi> index =
      CAST(CallBuiltin(Builtin::kWeakMapLookupHashIndex, context, table, key));

  GotoIf(TaggedEqual(index, SmiConstant(-1)), &return_undefined);

  Return(LoadFixedArrayElement(table, SmiUntag(index)));

  BIND(&return_undefined);
  Return(UndefinedConstant());
}

TF_BUILTIN(WeakMapPrototypeHas, WeakCollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto key = Parameter<Object>(Descriptor::kKey);
  const auto context = Parameter<Context>(Descriptor::kContext);

  Label return_false(this);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_MAP_TYPE,
                         "WeakMap.prototype.has");

  const TNode<EphemeronHashTable> table = LoadTable(CAST(receiver));
  const TNode<Object> index =
      CallBuiltin(Builtin::kWeakMapLookupHashIndex, context, table, key);

  GotoIf(TaggedEqual(index, SmiConstant(-1)), &return_false);

  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

// Helper that removes the entry with a given key from the backing store
// (EphemeronHashTable) of a WeakMap or WeakSet.
TF_BUILTIN(WeakCollectionDelete, WeakCollectionsBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto collection = Parameter<JSWeakCollection>(Descriptor::kCollection);
  auto key = Parameter<Object>(Descriptor::kKey);

  Label call_runtime(this), if_not_found(this);

  GotoIfNotJSReceiver(key, &if_not_found);

  TNode<IntPtrT> hash = LoadJSReceiverIdentityHash(CAST(key), &if_not_found);
  TNode<EphemeronHashTable> table = LoadTable(collection);
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

// Helper that sets the key and value to the backing store (EphemeronHashTable)
// of a WeakMap or WeakSet.
TF_BUILTIN(WeakCollectionSet, WeakCollectionsBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto collection = Parameter<JSWeakCollection>(Descriptor::kCollection);
  auto key = Parameter<JSReceiver>(Descriptor::kKey);
  auto value = Parameter<Object>(Descriptor::kValue);

  CSA_ASSERT(this, IsJSReceiver(key));

  Label call_runtime(this), if_no_hash(this), if_not_found(this);

  TNode<EphemeronHashTable> table = LoadTable(collection);
  TNode<IntPtrT> capacity = LoadTableCapacity(table);
  TNode<IntPtrT> entry_mask = EntryMask(capacity);

  TVARIABLE(IntPtrT, var_hash, LoadJSReceiverIdentityHash(key, &if_no_hash));
  TNode<IntPtrT> key_index = FindKeyIndexForKey(table, key, var_hash.value(),
                                                entry_mask, &if_not_found);

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
        FindKeyIndexForInsertion(table, var_hash.value(), entry_mask);
    AddEntry(table, insertion_key_index, key, value, number_of_elements);
    Return(collection);
  }
  BIND(&call_runtime);
  {
    CallRuntime(Runtime::kWeakCollectionSet, context, collection, key, value,
                SmiTag(var_hash.value()));
    Return(collection);
  }
}

TF_BUILTIN(WeakMapPrototypeDelete, CodeStubAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  auto key = Parameter<Object>(Descriptor::kKey);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_MAP_TYPE,
                         "WeakMap.prototype.delete");

  Return(CallBuiltin(Builtin::kWeakCollectionDelete, context, receiver, key));
}

TF_BUILTIN(WeakMapPrototypeSet, WeakCollectionsBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  auto key = Parameter<Object>(Descriptor::kKey);
  auto value = Parameter<Object>(Descriptor::kValue);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_MAP_TYPE,
                         "WeakMap.prototype.set");

  Label throw_invalid_key(this);
  GotoIfNotJSReceiver(key, &throw_invalid_key);

  Return(
      CallBuiltin(Builtin::kWeakCollectionSet, context, receiver, key, value));

  BIND(&throw_invalid_key);
  ThrowTypeError(context, MessageTemplate::kInvalidWeakMapKey, key);
}

TF_BUILTIN(WeakSetPrototypeAdd, WeakCollectionsBuiltinsAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  auto value = Parameter<Object>(Descriptor::kValue);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_SET_TYPE,
                         "WeakSet.prototype.add");

  Label throw_invalid_value(this);
  GotoIfNotJSReceiver(value, &throw_invalid_value);

  Return(CallBuiltin(Builtin::kWeakCollectionSet, context, receiver, value,
                     TrueConstant()));

  BIND(&throw_invalid_value);
  ThrowTypeError(context, MessageTemplate::kInvalidWeakSetValue, value);
}

TF_BUILTIN(WeakSetPrototypeDelete, CodeStubAssembler) {
  auto context = Parameter<Context>(Descriptor::kContext);
  auto receiver = Parameter<Object>(Descriptor::kReceiver);
  auto value = Parameter<Object>(Descriptor::kValue);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_SET_TYPE,
                         "WeakSet.prototype.delete");

  Return(CallBuiltin(Builtin::kWeakCollectionDelete, context, receiver, value));
}

TF_BUILTIN(WeakSetPrototypeHas, WeakCollectionsBuiltinsAssembler) {
  const auto receiver = Parameter<Object>(Descriptor::kReceiver);
  const auto key = Parameter<Object>(Descriptor::kKey);
  const auto context = Parameter<Context>(Descriptor::kContext);

  Label return_false(this);

  ThrowIfNotInstanceType(context, receiver, JS_WEAK_SET_TYPE,
                         "WeakSet.prototype.has");

  const TNode<EphemeronHashTable> table = LoadTable(CAST(receiver));
  const TNode<Object> index =
      CallBuiltin(Builtin::kWeakMapLookupHashIndex, context, table, key);

  GotoIf(TaggedEqual(index, SmiConstant(-1)), &return_false);

  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

}  // namespace internal
}  // namespace v8
