// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_INFO_H_
#define V8_TYPE_INFO_H_

#include "src/allocation.h"
#include "src/globals.h"
#include "src/types.h"
#include "src/zone-inl.h"

namespace v8 {
namespace internal {

// Forward declarations.
class SmallMapList;


class TypeFeedbackOracle: public ZoneObject {
 public:
  TypeFeedbackOracle(Handle<Code> code,
                     Handle<TypeFeedbackVector> feedback_vector,
                     Handle<Context> native_context, Zone* zone);

  bool LoadIsUninitialized(TypeFeedbackId id);
  bool LoadIsUninitialized(FeedbackVectorICSlot slot);
  bool StoreIsUninitialized(TypeFeedbackId id);
  bool CallIsUninitialized(FeedbackVectorICSlot slot);
  bool CallIsMonomorphic(FeedbackVectorICSlot slot);
  bool KeyedArrayCallIsHoley(TypeFeedbackId id);
  bool CallNewIsMonomorphic(FeedbackVectorSlot slot);

  // TODO(1571) We can't use ForInStatement::ForInType as the return value due
  // to various cycles in our headers.
  // TODO(rossberg): once all oracle access is removed from ast.cc, it should
  // be possible.
  byte ForInType(FeedbackVectorSlot feedback_vector_slot);

  void GetStoreModeAndKeyType(TypeFeedbackId id,
                              KeyedAccessStoreMode* store_mode,
                              IcCheckType* key_type);
  void GetLoadKeyType(TypeFeedbackId id, IcCheckType* key_type);

  void PropertyReceiverTypes(TypeFeedbackId id, Handle<String> name,
                             SmallMapList* receiver_types);
  void PropertyReceiverTypes(FeedbackVectorICSlot slot, Handle<String> name,
                             SmallMapList* receiver_types);
  void KeyedPropertyReceiverTypes(TypeFeedbackId id,
                                  SmallMapList* receiver_types,
                                  bool* is_string,
                                  IcCheckType* key_type);
  void KeyedPropertyReceiverTypes(FeedbackVectorICSlot slot,
                                  SmallMapList* receiver_types, bool* is_string,
                                  IcCheckType* key_type);
  void AssignmentReceiverTypes(TypeFeedbackId id,
                               Handle<String> name,
                               SmallMapList* receiver_types);
  void KeyedAssignmentReceiverTypes(TypeFeedbackId id,
                                    SmallMapList* receiver_types,
                                    KeyedAccessStoreMode* store_mode,
                                    IcCheckType* key_type);
  void CountReceiverTypes(TypeFeedbackId id,
                          SmallMapList* receiver_types);

  void CollectReceiverTypes(TypeFeedbackId id,
                            SmallMapList* types);
  template <class T>
  void CollectReceiverTypes(T* obj, SmallMapList* types);

  static bool CanRetainOtherContext(Map* map, Context* native_context);
  static bool CanRetainOtherContext(JSFunction* function,
                                    Context* native_context);

  Handle<JSFunction> GetCallTarget(FeedbackVectorICSlot slot);
  Handle<AllocationSite> GetCallAllocationSite(FeedbackVectorICSlot slot);
  Handle<JSFunction> GetCallNewTarget(FeedbackVectorSlot slot);
  Handle<AllocationSite> GetCallNewAllocationSite(FeedbackVectorSlot slot);

  bool LoadIsBuiltin(TypeFeedbackId id, Builtins::Name builtin_id);

  // TODO(1571) We can't use ToBooleanStub::Types as the return value because
  // of various cycles in our headers. Death to tons of implementations in
  // headers!! :-P
  byte ToBooleanTypes(TypeFeedbackId id);

  // Get type information for arithmetic operations and compares.
  void BinaryType(TypeFeedbackId id,
                  Type** left,
                  Type** right,
                  Type** result,
                  Maybe<int>* fixed_right_arg,
                  Handle<AllocationSite>* allocation_site,
                  Token::Value operation);

  void CompareType(TypeFeedbackId id,
                   Type** left,
                   Type** right,
                   Type** combined);

  Type* CountType(TypeFeedbackId id);

  Zone* zone() const { return zone_; }
  Isolate* isolate() const { return zone_->isolate(); }

 private:
  void CollectReceiverTypes(TypeFeedbackId id,
                            Handle<String> name,
                            Code::Flags flags,
                            SmallMapList* types);
  template <class T>
  void CollectReceiverTypes(T* obj, Handle<String> name, Code::Flags flags,
                            SmallMapList* types);

  // Returns true if there is at least one string map and if
  // all maps are string maps.
  bool HasOnlyStringMaps(SmallMapList* receiver_types);

  void SetInfo(TypeFeedbackId id, Object* target);

  void BuildDictionary(Handle<Code> code);
  void GetRelocInfos(Handle<Code> code, ZoneList<RelocInfo>* infos);
  void CreateDictionary(Handle<Code> code, ZoneList<RelocInfo>* infos);
  void RelocateRelocInfos(ZoneList<RelocInfo>* infos,
                          Code* old_code,
                          Code* new_code);
  void ProcessRelocInfos(ZoneList<RelocInfo>* infos);

  // Returns an element from the backing store. Returns undefined if
  // there is no information.
  Handle<Object> GetInfo(TypeFeedbackId id);

  // Returns an element from the type feedback vector. Returns undefined
  // if there is no information.
  Handle<Object> GetInfo(FeedbackVectorSlot slot);
  Handle<Object> GetInfo(FeedbackVectorICSlot slot);

 private:
  Handle<Context> native_context_;
  Zone* zone_;
  Handle<UnseededNumberDictionary> dictionary_;
  Handle<TypeFeedbackVector> feedback_vector_;

  DISALLOW_COPY_AND_ASSIGN(TypeFeedbackOracle);
};

} }  // namespace v8::internal

#endif  // V8_TYPE_INFO_H_
