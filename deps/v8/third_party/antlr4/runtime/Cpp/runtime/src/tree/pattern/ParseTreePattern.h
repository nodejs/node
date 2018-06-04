/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace tree {
namespace pattern {

/// <summary>
/// A pattern like {@code <ID> = <expr>;} converted to a <seealso
/// cref="ParseTree"/> by <seealso cref="ParseTreePatternMatcher#compile(String,
/// int)"/>.
/// </summary>
class ANTLR4CPP_PUBLIC ParseTreePattern {
 public:
  /// <summary>
  /// Construct a new instance of the <seealso cref="ParseTreePattern"/> class.
  /// </summary>
  /// <param name="matcher"> The <seealso cref="ParseTreePatternMatcher"/> which
  /// created this tree pattern. </param> <param name="pattern"> The tree
  /// pattern in concrete syntax form. </param> <param name="patternRuleIndex">
  /// The parser rule which serves as the root of the tree pattern. </param>
  /// <param name="patternTree"> The tree pattern in <seealso cref="ParseTree"/>
  /// form. </param>
  ParseTreePattern(ParseTreePatternMatcher* matcher, const std::string& pattern,
                   int patternRuleIndex, ParseTree* patternTree);
  ParseTreePattern(ParseTreePattern const&) = default;
  virtual ~ParseTreePattern();
  ParseTreePattern& operator=(ParseTreePattern const&) = default;

  /// <summary>
  /// Match a specific parse tree against this tree pattern.
  /// </summary>
  /// <param name="tree"> The parse tree to match against this tree pattern.
  /// </param> <returns> A <seealso cref="ParseTreeMatch"/> object describing
  /// the result of the match operation. The <seealso
  /// cref="ParseTreeMatch#succeeded()"/> method can be used to determine
  /// whether or not the match was successful. </returns>
  virtual ParseTreeMatch match(ParseTree* tree);

  /// <summary>
  /// Determine whether or not a parse tree matches this tree pattern.
  /// </summary>
  /// <param name="tree"> The parse tree to match against this tree pattern.
  /// </param> <returns> {@code true} if {@code tree} is a match for the current
  /// tree pattern; otherwise, {@code false}. </returns>
  virtual bool matches(ParseTree* tree);

  /// Find all nodes using XPath and then try to match those subtrees against
  /// this tree pattern.
  /// @param tree The ParseTree to match against this pattern.
  /// @param xpath An expression matching the nodes
  ///
  /// @returns A collection of ParseTreeMatch objects describing the
  /// successful matches. Unsuccessful matches are omitted from the result,
  /// regardless of the reason for the failure.
  virtual std::vector<ParseTreeMatch> findAll(ParseTree* tree,
                                              const std::string& xpath);

  /// <summary>
  /// Get the <seealso cref="ParseTreePatternMatcher"/> which created this tree
  /// pattern.
  /// </summary>
  /// <returns> The <seealso cref="ParseTreePatternMatcher"/> which created this
  /// tree pattern. </returns>
  virtual ParseTreePatternMatcher* getMatcher() const;

  /// <summary>
  /// Get the tree pattern in concrete syntax form.
  /// </summary>
  /// <returns> The tree pattern in concrete syntax form. </returns>
  virtual std::string getPattern() const;

  /// <summary>
  /// Get the parser rule which serves as the outermost rule for the tree
  /// pattern.
  /// </summary>
  /// <returns> The parser rule which serves as the outermost rule for the tree
  /// pattern. </returns>
  virtual int getPatternRuleIndex() const;

  /// <summary>
  /// Get the tree pattern as a <seealso cref="ParseTree"/>. The rule and token
  /// tags from the pattern are present in the parse tree as terminal nodes with
  /// a symbol of type <seealso cref="RuleTagToken"/> or <seealso
  /// cref="TokenTagToken"/>.
  /// </summary>
  /// <returns> The tree pattern as a <seealso cref="ParseTree"/>. </returns>
  virtual ParseTree* getPatternTree() const;

 private:
  const int patternRuleIndex;

  /// This is the backing field for <seealso cref="#getPattern()"/>.
  const std::string _pattern;

  /// This is the backing field for <seealso cref="#getPatternTree()"/>.
  ParseTree* _patternTree;

  /// This is the backing field for <seealso cref="#getMatcher()"/>.
  ParseTreePatternMatcher* const _matcher;
};

}  // namespace pattern
}  // namespace tree
}  // namespace antlr4
