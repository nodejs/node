// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_TYPE_INFO_H_
#define V8_TYPE_INFO_H_

#include "allocation.h"
#include "globals.h"
#include "types.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {

// Forward declarations.
class ICStub;
class SmallMapList;


class TypeFeedbackOracle: public ZoneObject {
 public:
  TypeFeedbackOracle(Handle<Code> code,
                     Handle<Context> native_context,
                     Zone* zone);

  bool LoadIsUninitialized(TypeFeedbackId id);
  bool StoreIsUninitialized(TypeFeedbackId id);
  bool StoreIsKeyedPolymorphic(TypeFeedbackId id);
  bool CallIsMonomorphic(TypeFeedbackId aid);
  bool CallNewIsMonomorphic(TypeFeedbackId id);

  // TODO(1571) We can't use ForInStatement::ForInType as the return value due
  // to various cycles in our headers.
  // TODO(rossberg): once all oracle access is removed from ast.cc, it should
  // be possible.
  byte ForInType(TypeFeedbackId id);

  KeyedAccessStoreMode GetStoreMode(TypeFeedbackId id);

  void PropertyReceiverTypes(TypeFeedbackId id,
                             Handle<String> name,
                             SmallMapList* receiver_types,
                             bool* is_prototype);
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

  Handle<JSFunction> GetCallTarget(TypeFeedbackId id);
  Handle<JSFunction> GetCallNewTarget(TypeFeedbackId id);
  Handle<AllocationSite> GetCallNewAllocationSite(TypeFeedbackId id);

  bool LoadIsBuiltin(TypeFeedbackId id, Builtins::Name builtin_id);
  bool LoadIsStub(TypeFeedbackId id, ICStub* stub);

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
                          byte* old_start,
                          byte* new_start);
  void ProcessRelocInfos(ZoneList<RelocInfo>* infos);
  void ProcessTypeFeedbackCells(Handle<Code> code);

  // Returns an element from the backing store. Returns undefined if
  // there is no information.
  Handle<Object> GetInfo(TypeFeedbackId id);

 private:
  Handle<Context> native_context_;
  Zone* zone_;
  Handle<UnseededNumberDictionary> dictionary_;

  DISALLOW_COPY_AND_ASSIGN(TypeFeedbackOracle);
};

} }  // namespace v8::internal

#endif  // V8_TYPE_INFO_H_
