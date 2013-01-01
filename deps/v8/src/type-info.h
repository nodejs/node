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
#include "ast.h"
#include "globals.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {

const int kMaxKeyedPolymorphism = 4;

//         Unknown
//           |   \____________
//           |                |
//      Primitive       Non-primitive
//           |   \_______     |
//           |           |    |
//        Number       String |
//         /   \         |    |
//    Double  Integer32  |   /
//        |      |      /   /
//        |     Smi    /   /
//        |      |    / __/
//        Uninitialized.

class TypeInfo {
 public:
  TypeInfo() : type_(kUninitialized) { }

  static TypeInfo Unknown() { return TypeInfo(kUnknown); }
  // We know it's a primitive type.
  static TypeInfo Primitive() { return TypeInfo(kPrimitive); }
  // We know it's a number of some sort.
  static TypeInfo Number() { return TypeInfo(kNumber); }
  // We know it's a signed 32 bit integer.
  static TypeInfo Integer32() { return TypeInfo(kInteger32); }
  // We know it's a Smi.
  static TypeInfo Smi() { return TypeInfo(kSmi); }
  // We know it's a Symbol.
  static TypeInfo Symbol() { return TypeInfo(kSymbol); }
  // We know it's a heap number.
  static TypeInfo Double() { return TypeInfo(kDouble); }
  // We know it's a string.
  static TypeInfo String() { return TypeInfo(kString); }
  // We know it's a non-primitive (object) type.
  static TypeInfo NonPrimitive() { return TypeInfo(kNonPrimitive); }
  // We haven't started collecting info yet.
  static TypeInfo Uninitialized() { return TypeInfo(kUninitialized); }

  int ToInt() {
    return type_;
  }

  static TypeInfo FromInt(int bit_representation) {
    Type t = static_cast<Type>(bit_representation);
    ASSERT(t == kUnknown ||
           t == kPrimitive ||
           t == kNumber ||
           t == kInteger32 ||
           t == kSmi ||
           t == kDouble ||
           t == kString ||
           t == kNonPrimitive);
    return TypeInfo(t);
  }

  // Return the weakest (least precise) common type.
  static TypeInfo Combine(TypeInfo a, TypeInfo b) {
    return TypeInfo(static_cast<Type>(a.type_ & b.type_));
  }


  // Integer32 is an integer that can be represented as a signed
  // 32-bit integer. It has to be
  // in the range [-2^31, 2^31 - 1]. We also have to check for negative 0
  // as it is not an Integer32.
  static inline bool IsInt32Double(double value) {
    const DoubleRepresentation minus_zero(-0.0);
    DoubleRepresentation rep(value);
    if (rep.bits == minus_zero.bits) return false;
    if (value >= kMinInt && value <= kMaxInt &&
        value == static_cast<int32_t>(value)) {
      return true;
    }
    return false;
  }

  static TypeInfo TypeFromValue(Handle<Object> value);

  bool Equals(const TypeInfo& other) {
    return type_ == other.type_;
  }

  inline bool IsUnknown() {
    ASSERT(type_ != kUninitialized);
    return type_ == kUnknown;
  }

  inline bool IsPrimitive() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kPrimitive) == kPrimitive);
  }

  inline bool IsNumber() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kNumber) == kNumber);
  }

  inline bool IsSmi() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kSmi) == kSmi);
  }

  inline bool IsSymbol() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kSymbol) == kSymbol);
  }

  inline bool IsNonSymbol() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kSymbol) == kString);
  }

  inline bool IsInteger32() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kInteger32) == kInteger32);
  }

  inline bool IsDouble() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kDouble) == kDouble);
  }

  inline bool IsString() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kString) == kString);
  }

  inline bool IsNonPrimitive() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kNonPrimitive) == kNonPrimitive);
  }

  inline bool IsUninitialized() {
    return type_ == kUninitialized;
  }

  const char* ToString() {
    switch (type_) {
      case kUnknown: return "Unknown";
      case kPrimitive: return "Primitive";
      case kNumber: return "Number";
      case kInteger32: return "Integer32";
      case kSmi: return "Smi";
      case kSymbol: return "Symbol";
      case kDouble: return "Double";
      case kString: return "String";
      case kNonPrimitive: return "Object";
      case kUninitialized: return "Uninitialized";
    }
    UNREACHABLE();
    return "Unreachable code";
  }

 private:
  enum Type {
    kUnknown = 0,          // 0000000
    kPrimitive = 0x10,     // 0010000
    kNumber = 0x11,        // 0010001
    kInteger32 = 0x13,     // 0010011
    kSmi = 0x17,           // 0010111
    kDouble = 0x19,        // 0011001
    kString = 0x30,        // 0110000
    kSymbol = 0x32,        // 0110010
    kNonPrimitive = 0x40,  // 1000000
    kUninitialized = 0x7f  // 1111111
  };

  explicit inline TypeInfo(Type t) : type_(t) { }

  Type type_;
};


enum StringStubFeedback {
  DEFAULT_STRING_STUB = 0,
  STRING_INDEX_OUT_OF_BOUNDS = 1
};


// Forward declarations.
class Assignment;
class BinaryOperation;
class Call;
class CallNew;
class CaseClause;
class CompareOperation;
class CompilationInfo;
class CountOperation;
class Expression;
class Property;
class SmallMapList;
class UnaryOperation;
class ForInStatement;


class TypeFeedbackOracle: public ZoneObject {
 public:
  TypeFeedbackOracle(Handle<Code> code,
                     Handle<Context> native_context,
                     Isolate* isolate,
                     Zone* zone);

  bool LoadIsMonomorphicNormal(Property* expr);
  bool LoadIsUninitialized(Property* expr);
  bool LoadIsMegamorphicWithTypeInfo(Property* expr);
  bool StoreIsMonomorphicNormal(TypeFeedbackId ast_id);
  bool StoreIsMegamorphicWithTypeInfo(TypeFeedbackId ast_id);
  bool CallIsMonomorphic(Call* expr);
  bool CallNewIsMonomorphic(CallNew* expr);
  bool ObjectLiteralStoreIsMonomorphic(ObjectLiteral::Property* prop);

  bool IsForInFastCase(ForInStatement* expr);

  Handle<Map> LoadMonomorphicReceiverType(Property* expr);
  Handle<Map> StoreMonomorphicReceiverType(TypeFeedbackId ast_id);

  void LoadReceiverTypes(Property* expr,
                         Handle<String> name,
                         SmallMapList* types);
  void StoreReceiverTypes(Assignment* expr,
                          Handle<String> name,
                          SmallMapList* types);
  void CallReceiverTypes(Call* expr,
                         Handle<String> name,
                         CallKind call_kind,
                         SmallMapList* types);
  void CollectKeyedReceiverTypes(TypeFeedbackId ast_id,
                                 SmallMapList* types);

  static bool CanRetainOtherContext(Map* map, Context* native_context);
  static bool CanRetainOtherContext(JSFunction* function,
                                    Context* native_context);

  CheckType GetCallCheckType(Call* expr);
  Handle<JSObject> GetPrototypeForPrimitiveCheck(CheckType check);

  Handle<JSFunction> GetCallTarget(Call* expr);
  Handle<JSFunction> GetCallNewTarget(CallNew* expr);

  Handle<Map> GetObjectLiteralStoreMap(ObjectLiteral::Property* prop);

  bool LoadIsBuiltin(Property* expr, Builtins::Name id);

  // TODO(1571) We can't use ToBooleanStub::Types as the return value because
  // of various cylces in our headers. Death to tons of implementations in
  // headers!! :-P
  byte ToBooleanTypes(TypeFeedbackId ast_id);

  // Get type information for arithmetic operations and compares.
  TypeInfo UnaryType(UnaryOperation* expr);
  void BinaryType(BinaryOperation* expr,
                  TypeInfo* left,
                  TypeInfo* right,
                  TypeInfo* result);
  void CompareType(CompareOperation* expr,
                   TypeInfo* left_type,
                   TypeInfo* right_type,
                   TypeInfo* overall_type);
  Handle<Map> GetCompareMap(CompareOperation* expr);
  TypeInfo SwitchType(CaseClause* clause);
  TypeInfo IncrementType(CountOperation* expr);

  Zone* zone() const { return zone_; }

 private:
  void CollectReceiverTypes(TypeFeedbackId ast_id,
                            Handle<String> name,
                            Code::Flags flags,
                            SmallMapList* types);

  void SetInfo(TypeFeedbackId ast_id, Object* target);

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
  Handle<Object> GetInfo(TypeFeedbackId ast_id);

  Handle<Context> native_context_;
  Isolate* isolate_;
  Handle<UnseededNumberDictionary> dictionary_;
  Zone* zone_;

  DISALLOW_COPY_AND_ASSIGN(TypeFeedbackOracle);
};

} }  // namespace v8::internal

#endif  // V8_TYPE_INFO_H_
