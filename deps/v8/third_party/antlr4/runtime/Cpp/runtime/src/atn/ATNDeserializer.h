/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/ATNDeserializationOptions.h"
#include "atn/LexerAction.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC ATNDeserializer {
 public:
  static const size_t SERIALIZED_VERSION;

  /// This is the current serialized UUID.
  // ml: defined as function to avoid the “static initialization order fiasco”.
  static Guid SERIALIZED_UUID();

  ATNDeserializer();
  ATNDeserializer(const ATNDeserializationOptions& dso);
  virtual ~ATNDeserializer();

  static Guid toUUID(const unsigned short* data, size_t offset);

  virtual ATN deserialize(const std::vector<uint16_t>& input);
  virtual void verifyATN(const ATN& atn);

  static void checkCondition(bool condition);
  static void checkCondition(bool condition, const std::string& message);

  static Transition* edgeFactory(const ATN& atn, size_t type, size_t src,
                                 size_t trg, size_t arg1, size_t arg2,
                                 size_t arg3,
                                 const std::vector<misc::IntervalSet>& sets);

  static ATNState* stateFactory(size_t type, size_t ruleIndex);

 protected:
  /// Determines if a particular serialized representation of an ATN supports
  /// a particular feature, identified by the <seealso cref="UUID"/> used for
  /// serializing the ATN at the time the feature was first introduced.
  ///
  /// <param name="feature"> The <seealso cref="UUID"/> marking the first time
  /// the feature was supported in the serialized ATN. </param> <param
  /// name="actualUuid"> The <seealso cref="UUID"/> of the actual serialized ATN
  /// which is currently being deserialized. </param> <returns> {@code true} if
  /// the {@code actualUuid} value represents a serialized ATN at or after the
  /// feature identified by {@code feature} was introduced; otherwise, {@code
  /// false}. </returns>
  virtual bool isFeatureSupported(const Guid& feature, const Guid& actualUuid);
  void markPrecedenceDecisions(const ATN& atn);
  Ref<LexerAction> lexerActionFactory(LexerActionType type, int data1,
                                      int data2);

 private:
  /// This is the earliest supported serialized UUID.
  static Guid BASE_SERIALIZED_UUID();

  /// This UUID indicates an extension of <seealso cref="BASE_SERIALIZED_UUID"/>
  /// for the addition of precedence predicates.
  static Guid ADDED_PRECEDENCE_TRANSITIONS();

  /**
   * This UUID indicates an extension of ADDED_PRECEDENCE_TRANSITIONS
   * for the addition of lexer actions encoded as a sequence of
   * LexerAction instances.
   */
  static Guid ADDED_LEXER_ACTIONS();

  /**
   * This UUID indicates the serialized ATN contains two sets of
   * IntervalSets, where the second set's values are encoded as
   * 32-bit integers to support the full Unicode SMP range up to U+10FFFF.
   */
  static Guid ADDED_UNICODE_SMP();

  /// This list contains all of the currently supported UUIDs, ordered by when
  /// the feature first appeared in this branch.
  static std::vector<Guid>& SUPPORTED_UUIDS();

  ATNDeserializationOptions deserializationOptions;
};

}  // namespace atn
}  // namespace antlr4
