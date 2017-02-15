// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPE_INFO_H_
#define V8_TYPE_INFO_H_

#include "src/allocation.h"
#include "src/ast/ast-types.h"
#include "src/contexts.h"
#include "src/globals.h"
#include "src/parsing/token.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class SmallMapList;
class FeedbackNexus;
class StubCache;

class TypeFeedbackOracle: public ZoneObject {
 public:
  TypeFeedbackOracle(Isolate* isolate, Zone* zone, Handle<Code> code,
                     Handle<TypeFeedbackVector> feedback_vector,
                     Handle<Context> native_context);

  InlineCacheState LoadInlineCacheState(FeedbackVectorSlot slot);
  bool StoreIsUninitialized(FeedbackVectorSlot slot);
  bool CallIsUninitialized(FeedbackVectorSlot slot);
  bool CallIsMonomorphic(FeedbackVectorSlot slot);
  bool CallNewIsMonomorphic(FeedbackVectorSlot slot);

  // TODO(1571) We can't use ForInStatement::ForInType as the return value due
  // to various cycles in our headers.
  // TODO(rossberg): once all oracle access is removed from ast.cc, it should
  // be possible.
  byte ForInType(FeedbackVectorSlot feedback_vector_slot);

  void GetStoreModeAndKeyType(FeedbackVectorSlot slot,
                              KeyedAccessStoreMode* store_mode,
                              IcCheckType* key_type);

  void PropertyReceiverTypes(FeedbackVectorSlot slot, Handle<Name> name,
                             SmallMapList* receiver_types);
  void KeyedPropertyReceiverTypes(FeedbackVectorSlot slot,
                                  SmallMapList* receiver_types, bool* is_string,
                                  IcCheckType* key_type);
  void AssignmentReceiverTypes(FeedbackVectorSlot slot, Handle<Name> name,
                               SmallMapList* receiver_types);
  void KeyedAssignmentReceiverTypes(FeedbackVectorSlot slot,
                                    SmallMapList* receiver_types,
                                    KeyedAccessStoreMode* store_mode,
                                    IcCheckType* key_type);
  void CountReceiverTypes(FeedbackVectorSlot slot,
                          SmallMapList* receiver_types);

  void CollectReceiverTypes(FeedbackVectorSlot slot, SmallMapList* types);
  void CollectReceiverTypes(FeedbackNexus* nexus, SmallMapList* types);

  static bool IsRelevantFeedback(Map* map, Context* native_context) {
    Object* constructor = map->GetConstructor();
    return !constructor->IsJSFunction() ||
           JSFunction::cast(constructor)->context()->native_context() ==
               native_context;
  }

  Handle<JSFunction> GetCallTarget(FeedbackVectorSlot slot);
  Handle<AllocationSite> GetCallAllocationSite(FeedbackVectorSlot slot);
  Handle<JSFunction> GetCallNewTarget(FeedbackVectorSlot slot);
  Handle<AllocationSite> GetCallNewAllocationSite(FeedbackVectorSlot slot);

  // TODO(1571) We can't use ToBooleanICStub::Types as the return value because
  // of various cycles in our headers. Death to tons of implementations in
  // headers!! :-P
  uint16_t ToBooleanTypes(TypeFeedbackId id);

  // Get type information for arithmetic operations and compares.
  void BinaryType(TypeFeedbackId id, FeedbackVectorSlot slot, AstType** left,
                  AstType** right, AstType** result,
                  Maybe<int>* fixed_right_arg,
                  Handle<AllocationSite>* allocation_site,
                  Token::Value operation);

  void CompareType(TypeFeedbackId id, FeedbackVectorSlot slot, AstType** left,
                   AstType** right, AstType** combined);

  AstType* CountType(TypeFeedbackId id, FeedbackVectorSlot slot);

  Zone* zone() const { return zone_; }
  Isolate* isolate() const { return isolate_; }

 private:
  void CollectReceiverTypes(StubCache* stub_cache, FeedbackVectorSlot slot,
                            Handle<Name> name, SmallMapList* types);
  void CollectReceiverTypes(StubCache* stub_cache, FeedbackNexus* nexus,
                            Handle<Name> name, SmallMapList* types);

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

 private:
  Handle<Context> native_context_;
  Isolate* isolate_;
  Zone* zone_;
  Handle<UnseededNumberDictionary> dictionary_;
  Handle<TypeFeedbackVector> feedback_vector_;

  DISALLOW_COPY_AND_ASSIGN(TypeFeedbackOracle);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TYPE_INFO_H_
