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
                     Handle<FixedArray> feedback_vector,
                     Handle<Context> native_context,
                     Zone* zone);

  bool LoadIsUninitialized(TypeFeedbackId id);
  bool StoreIsUninitialized(TypeFeedbackId id);
  bool StoreIsKeyedPolymorphic(TypeFeedbackId id);
  bool CallIsMonomorphic(int slot);
  bool CallIsMonomorphic(TypeFeedbackId aid);
  bool KeyedArrayCallIsHoley(TypeFeedbackId id);
  bool CallNewIsMonomorphic(int slot);

  // TODO(1571) We can't use ForInStatement::ForInType as the return value due
  // to various cycles in our headers.
  // TODO(rossberg): once all oracle access is removed from ast.cc, it should
  // be possible.
  byte ForInType(int feedback_vector_slot);

  KeyedAccessStoreMode GetStoreMode(TypeFeedbackId id);

  void PropertyReceiverTypes(TypeFeedbackId id, Handle<String> name,
                             SmallMapList* receiver_types);
  void KeyedPropertyReceiverTypes(TypeFeedbackId id,
                                  SmallMapList* receiver_types,
                                  bool* is_string);
  void AssignmentReceiverTypes(TypeFeedbackId id,
                               Handle<String> name,
                               SmallMapList* receiver_types);
  void KeyedAssignmentReceiverTypes(TypeFeedbackId id,
                                    SmallMapList* receiver_types,
                                    KeyedAccessStoreMode* store_mode);
  void CountReceiverTypes(TypeFeedbackId id,
                          SmallMapList* receiver_types);

  void CollectReceiverTypes(TypeFeedbackId id,
                            SmallMapList* types);

  static bool CanRetainOtherContext(Map* map, Context* native_context);
  static bool CanRetainOtherContext(JSFunction* function,
                                    Context* native_context);

  Handle<JSFunction> GetCallTarget(int slot);
  Handle<AllocationSite> GetCallAllocationSite(int slot);
  Handle<JSFunction> GetCallNewTarget(int slot);
  Handle<AllocationSite> GetCallNewAllocationSite(int slot);

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
  Handle<Object> GetInfo(int slot);

 private:
  Handle<Context> native_context_;
  Zone* zone_;
  Handle<UnseededNumberDictionary> dictionary_;
  Handle<FixedArray> feedback_vector_;

  DISALLOW_COPY_AND_ASSIGN(TypeFeedbackOracle);
};

} }  // namespace v8::internal

#endif  // V8_TYPE_INFO_H_
