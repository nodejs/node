/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "Exceptions.h"

namespace antlr4 {
namespace tree {
namespace pattern {

/// <summary>
/// A tree pattern matching mechanism for ANTLR <seealso cref="ParseTree"/>s.
/// <p/>
/// Patterns are strings of source input text with special tags representing
/// token or rule references such as:
/// <p/>
/// {@code <ID> = <expr>;}
/// <p/>
/// Given a pattern start rule such as {@code statement}, this object constructs
/// a <seealso cref="ParseTree"/> with placeholders for the {@code ID} and
/// {@code expr} subtree. Then the <seealso cref="#match"/> routines can compare
/// an actual <seealso cref="ParseTree"/> from a parse with this pattern. Tag
/// {@code <ID>} matches any {@code ID} token and tag {@code <expr>} references
/// the result of the
/// {@code expr} rule (generally an instance of {@code ExprContext}.
/// <p/>
/// Pattern {@code x = 0;} is a similar pattern that matches the same pattern
/// except that it requires the identifier to be {@code x} and the expression to
/// be {@code 0}.
/// <p/>
/// The <seealso cref="#matches"/> routines return {@code true} or {@code false}
/// based upon a match for the tree rooted at the parameter sent in. The
/// <seealso cref="#match"/> routines return a <seealso cref="ParseTreeMatch"/>
/// object that contains the parse tree, the parse tree pattern, and a map from
/// tag name to matched nodes (more below). A subtree that fails to match,
/// returns with <seealso cref="ParseTreeMatch#mismatchedNode"/> set to the
/// first tree node that did not match. <p/> For efficiency, you can compile a
/// tree pattern in string form to a <seealso cref="ParseTreePattern"/> object.
/// <p/>
/// See {@code TestParseTreeMatcher} for lots of examples.
/// <seealso cref="ParseTreePattern"/> has two static helper methods:
/// <seealso cref="ParseTreePattern#findAll"/> and <seealso
/// cref="ParseTreePattern#match"/> that are easy to use but not super efficient
/// because they create new <seealso cref="ParseTreePatternMatcher"/> objects
/// each time and have to compile the pattern in string form before using it.
/// <p/>
/// The lexer and parser that you pass into the <seealso
/// cref="ParseTreePatternMatcher"/> constructor are used to parse the pattern
/// in string form. The lexer converts the {@code <ID> = <expr>;} into a
/// sequence of four tokens (assuming lexer throws out whitespace or puts it on
/// a hidden channel). Be aware that the input stream is reset for the lexer
/// (but not the parser; a <seealso cref="ParserInterpreter"/> is created to
/// parse the input.). Any user-defined fields you have put into the lexer might
/// get changed when this mechanism asks it to scan the pattern string. <p/>
/// Normally a parser does not accept token {@code <expr>} as a valid
/// {@code expr} but, from the parser passed in, we create a special version of
/// the underlying grammar representation (an <seealso cref="ATN"/>) that allows
/// imaginary tokens representing rules ({@code <expr>}) to match entire rules.
/// We call these <em>bypass alternatives</em>. <p/> Delimiters are {@code <}
/// and {@code >}, with {@code \} as the escape string by default, but you can
/// set them to whatever you want using <seealso cref="#setDelimiters"/>. You
/// must escape both start and stop strings
/// {@code \<} and {@code \>}.
/// </summary>
class ANTLR4CPP_PUBLIC ParseTreePatternMatcher {
 public:
  class CannotInvokeStartRule : public RuntimeException {
   public:
    CannotInvokeStartRule(const RuntimeException& e);
    ~CannotInvokeStartRule();
  };

  // Fixes https://github.com/antlr/antlr4/issues/413
  // "Tree pattern compilation doesn't check for a complete parse"
  class StartRuleDoesNotConsumeFullPattern : public RuntimeException {
   public:
    StartRuleDoesNotConsumeFullPattern() = default;
    StartRuleDoesNotConsumeFullPattern(
        StartRuleDoesNotConsumeFullPattern const&) = default;
    ~StartRuleDoesNotConsumeFullPattern();

    StartRuleDoesNotConsumeFullPattern& operator=(
        StartRuleDoesNotConsumeFullPattern const&) = default;
  };

  /// Constructs a <seealso cref="ParseTreePatternMatcher"/> or from a <seealso
  /// cref="Lexer"/> and <seealso cref="Parser"/> object. The lexer input stream
  /// is altered for tokenizing the tree patterns. The parser is used as a
  /// convenient mechanism to get the grammar name, plus token, rule names.
  ParseTreePatternMatcher(Lexer* lexer, Parser* parser);
  virtual ~ParseTreePatternMatcher();

  /// <summary>
  /// Set the delimiters used for marking rule and token tags within concrete
  /// syntax used by the tree pattern parser.
  /// </summary>
  /// <param name="start"> The start delimiter. </param>
  /// <param name="stop"> The stop delimiter. </param>
  /// <param name="escapeLeft"> The escape sequence to use for escaping a start
  /// or stop delimiter.
  /// </param>
  /// <exception cref="IllegalArgumentException"> if {@code start} is {@code
  /// null} or empty. </exception> <exception cref="IllegalArgumentException">
  /// if {@code stop} is {@code null} or empty. </exception>
  virtual void setDelimiters(const std::string& start, const std::string& stop,
                             const std::string& escapeLeft);

  /// <summary>
  /// Does {@code pattern} matched as rule {@code patternRuleIndex} match {@code
  /// tree}? </summary>
  virtual bool matches(ParseTree* tree, const std::string& pattern,
                       int patternRuleIndex);

  /// <summary>
  /// Does {@code pattern} matched as rule patternRuleIndex match tree? Pass in
  /// a
  ///  compiled pattern instead of a string representation of a tree pattern.
  /// </summary>
  virtual bool matches(ParseTree* tree, const ParseTreePattern& pattern);

  /// <summary>
  /// Compare {@code pattern} matched as rule {@code patternRuleIndex} against
  /// {@code tree} and return a <seealso cref="ParseTreeMatch"/> object that
  /// contains the matched elements, or the node at which the match failed.
  /// </summary>
  virtual ParseTreeMatch match(ParseTree* tree, const std::string& pattern,
                               int patternRuleIndex);

  /// <summary>
  /// Compare {@code pattern} matched against {@code tree} and return a
  /// <seealso cref="ParseTreeMatch"/> object that contains the matched
  /// elements, or the node at which the match failed. Pass in a compiled
  /// pattern instead of a string representation of a tree pattern.
  /// </summary>
  virtual ParseTreeMatch match(ParseTree* tree,
                               const ParseTreePattern& pattern);

  /// <summary>
  /// For repeated use of a tree pattern, compile it to a
  /// <seealso cref="ParseTreePattern"/> using this method.
  /// </summary>
  virtual ParseTreePattern compile(const std::string& pattern,
                                   int patternRuleIndex);

  /// <summary>
  /// Used to convert the tree pattern string into a series of tokens. The
  /// input stream is reset.
  /// </summary>
  virtual Lexer* getLexer();

  /// <summary>
  /// Used to collect to the grammar file name, token names, rule names for
  /// used to parse the pattern into a parse tree.
  /// </summary>
  virtual Parser* getParser();

  // ---- SUPPORT CODE ----

  virtual std::vector<std::unique_ptr<Token>> tokenize(
      const std::string& pattern);

  /// Split "<ID> = <e:expr>;" into 4 chunks for tokenizing by tokenize().
  virtual std::vector<Chunk> split(const std::string& pattern);

 protected:
  std::string _start;
  std::string _stop;
  std::string _escape;  // e.g., \< and \> must escape BOTH!

  /// Recursively walk {@code tree} against {@code patternTree}, filling
  /// {@code match.}<seealso cref="ParseTreeMatch#labels labels"/>.
  ///
  /// <returns> the first node encountered in {@code tree} which does not match
  /// a corresponding node in {@code patternTree}, or {@code null} if the match
  /// was successful. The specific node returned depends on the matching
  /// algorithm used by the implementation, and may be overridden. </returns>
  virtual ParseTree* matchImpl(
      ParseTree* tree, ParseTree* patternTree,
      std::map<std::string, std::vector<ParseTree*>>& labels);

  /// Is t <expr> subtree?
  virtual RuleTagToken* getRuleTagToken(ParseTree* t);

 private:
  Lexer* _lexer;
  Parser* _parser;

  void InitializeInstanceFields();
};

}  // namespace pattern
}  // namespace tree
}  // namespace antlr4
