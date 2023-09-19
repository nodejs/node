// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_COLLECTIONS_GEN_H_
#define V8_BUILTINS_BUILTINS_COLLECTIONS_GEN_H_

#include "src/codegen/code-stub-assembler.h"

namespace v8 {
namespace internal {

void BranchIfIterableWithOriginalKeyOrValueMapIterator(
    compiler::CodeAssemblerState* state, TNode<Object> iterable,
    TNode<Context> context, compiler::CodeAssemblerLabel* if_true,
    compiler::CodeAssemblerLabel* if_false);

void BranchIfIterableWithOriginalValueSetIterator(
    compiler::CodeAssemblerState* state, TNode<Object> iterable,
    TNode<Context> context, compiler::CodeAssemblerLabel* if_true,
    compiler::CodeAssemblerLabel* if_false);

class BaseCollectionsAssembler : public CodeStubAssembler {
 public:
  explicit BaseCollectionsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  virtual ~BaseCollectionsAssembler() = default;

  void GotoIfCannotBeHeldWeakly(const TNode<Object> obj,
                                Label* if_cannot_be_held_weakly);

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
  void AddConstructorEntriesFromFastJSArray(
      Variant variant, TNode<Context> context, TNode<Context> native_context,
      TNode<Object> collection, TNode<JSArray> fast_jsarray,
      Label* if_may_have_side_effects, TVariable<IntPtrT>& var_current_index);

  // Adds constructor entries to a collection using the iterator protocol.
  void AddConstructorEntriesFromIterable(
      Variant variant, TNode<Context> context, TNode<Context> native_context,
      TNode<Object> collection, TNode<Object> iterable, Label* if_exception,
      TVariable<JSReceiver>* var_iterator, TVariable<Object>* var_exception);

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

  TNode<IntPtrT> GetHash(const TNode<HeapObject> key, Label* if_no_hash);
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

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_COLLECTIONS_GEN_H_
