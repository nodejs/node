// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_KEY_ACCUMULATOR_H_
#define V8_KEY_ACCUMULATOR_H_

#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

enum AddKeyConversion { DO_NOT_CONVERT, CONVERT_TO_ARRAY_INDEX, PROXY_MAGIC };

// This is a helper class for JSReceiver::GetKeys which collects and sorts keys.
// GetKeys needs to sort keys per prototype level, first showing the integer
// indices from elements then the strings from the properties. However, this
// does not apply to proxies which are in full control of how the keys are
// sorted.
//
// For performance reasons the KeyAccumulator internally separates integer keys
// in |elements_| into sorted lists per prototype level. String keys are
// collected in |string_properties_|, a single OrderedHashSet (similar for
// Symbols in |symbol_properties_|. To separate the keys per level later when
// assembling the final list, |levelLengths_| keeps track of the number of
// String and Symbol keys per level.
//
// Only unique keys are kept by the KeyAccumulator, strings are stored in a
// HashSet for inexpensive lookups. Integer keys are kept in sorted lists which
// are more compact and allow for reasonably fast includes check.
class KeyAccumulator final BASE_EMBEDDED {
 public:
  KeyAccumulator(Isolate* isolate, KeyCollectionType type,
                 PropertyFilter filter)
      : isolate_(isolate), type_(type), filter_(filter) {}
  ~KeyAccumulator();

  bool AddKey(uint32_t key);
  bool AddKey(Object* key, AddKeyConversion convert);
  bool AddKey(Handle<Object> key, AddKeyConversion convert);
  void AddKeys(Handle<FixedArray> array, AddKeyConversion convert);
  void AddKeys(Handle<JSObject> array, AddKeyConversion convert);
  void AddKeysFromProxy(Handle<JSObject> array);
  Maybe<bool> AddKeysFromProxy(Handle<JSProxy> proxy, Handle<FixedArray> keys);
  void AddElementKeysFromInterceptor(Handle<JSObject> array);
  // Jump to the next level, pushing the current |levelLength_| to
  // |levelLengths_| and adding a new list to |elements_|.
  void NextPrototype();
  // Sort the integer indices in the last list in |elements_|
  void SortCurrentElementsList();
  Handle<FixedArray> GetKeys(GetKeysConversion convert = KEEP_NUMBERS);
  int length() { return length_; }
  Isolate* isolate() { return isolate_; }

 private:
  bool AddIntegerKey(uint32_t key);
  bool AddStringKey(Handle<Object> key, AddKeyConversion convert);
  bool AddSymbolKey(Handle<Object> array);
  void SortCurrentElementsListRemoveDuplicates();

  Isolate* isolate_;
  KeyCollectionType type_;
  PropertyFilter filter_;
  // |elements_| contains the sorted element keys (indices) per level.
  std::vector<std::vector<uint32_t>*> elements_;
  // |protoLengths_| contains the total number of keys (elements + properties)
  // per level. Negative values mark counts for a level with keys from a proxy.
  std::vector<int> level_lengths_;
  // |string_properties_| contains the unique String property keys for all
  // levels in insertion order per level.
  Handle<OrderedHashSet> string_properties_;
  // |symbol_properties_| contains the unique Symbol property keys for all
  // levels in insertion order per level.
  Handle<OrderedHashSet> symbol_properties_;
  Handle<FixedArray> ownProxyKeys_;
  // |length_| keeps track of the total number of all element and property keys.
  int length_ = 0;
  // |levelLength_| keeps track of the number of String keys in the current
  // level.
  int level_string_length_ = 0;
  // |levelSymbolLength_| keeps track of the number of Symbol keys in the
  // current level.
  int level_symbol_length_ = 0;

  DISALLOW_COPY_AND_ASSIGN(KeyAccumulator);
};


}  // namespace internal
}  // namespace v8


#endif  // V8_KEY_ACCUMULATOR_H_
