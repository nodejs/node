/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC ATNSerializer {
 public:
  ATN* atn;

  ATNSerializer(ATN* atn);
  ATNSerializer(ATN* atn, const std::vector<std::string>& tokenNames);
  virtual ~ATNSerializer();

  /// <summary>
  /// Serialize state descriptors, edge descriptors, and decision->state map
  ///  into list of ints:
  ///
  /// 		grammar-type, (ANTLRParser.LEXER, ...)
  ///  	max token type,
  ///  	num states,
  ///  	state-0-type ruleIndex, state-1-type ruleIndex, ... state-i-type
  ///  ruleIndex optional-arg ...
  ///  	num rules,
  ///  	rule-1-start-state rule-1-args, rule-2-start-state  rule-2-args, ...
  ///  	(args are token type,actionIndex in lexer else 0,0)
  ///      num modes,
  ///      mode-0-start-state, mode-1-start-state, ... (parser has 0 modes)
  ///      num sets
  ///      set-0-interval-count intervals, set-1-interval-count intervals, ...
  ///  	num total edges,
  ///      src, trg, edge-type, edge arg1, optional edge arg2 (present always),
  ///      ...
  ///      num decisions,
  ///      decision-0-start-state, decision-1-start-state, ...
  ///
  ///  Convenient to pack into unsigned shorts to make as Java string.
  /// </summary>
  virtual std::vector<size_t> serialize();

  virtual std::string decode(const std::wstring& data);
  virtual std::string getTokenName(size_t t);

  /// Used by Java target to encode short/int array as chars in string.
  static std::wstring getSerializedAsString(ATN* atn);
  static std::vector<size_t> getSerialized(ATN* atn);

  static std::string getDecoded(ATN* atn, std::vector<std::string>& tokenNames);

 private:
  std::vector<std::string> _tokenNames;

  void serializeUUID(std::vector<size_t>& data, Guid uuid);
};

}  // namespace atn
}  // namespace antlr4
