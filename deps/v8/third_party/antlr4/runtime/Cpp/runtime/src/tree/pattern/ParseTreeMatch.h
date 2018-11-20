/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace tree {
namespace pattern {

/// Represents the result of matching a ParseTree against a tree pattern.
class ANTLR4CPP_PUBLIC ParseTreeMatch {
 private:
  /// This is the backing field for getTree().
  ParseTree* _tree;

  /// This is the backing field for getPattern().
  const ParseTreePattern& _pattern;

  /// This is the backing field for getLabels().
  std::map<std::string, std::vector<ParseTree*>> _labels;

  /// This is the backing field for getMismatchedNode().
  ParseTree* _mismatchedNode;

 public:
  /// <summary>
  /// Constructs a new instance of <seealso cref="ParseTreeMatch"/> from the
  /// specified parse tree and pattern.
  /// </summary>
  /// <param name="tree"> The parse tree to match against the pattern. </param>
  /// <param name="pattern"> The parse tree pattern. </param>
  /// <param name="labels"> A mapping from label names to collections of
  /// <seealso cref="ParseTree"/> objects located by the tree pattern matching
  /// process. </param> <param name="mismatchedNode"> The first node which
  /// failed to match the tree pattern during the matching process.
  /// </param>
  /// <exception cref="IllegalArgumentException"> if {@code tree} is {@code
  /// null} </exception> <exception cref="IllegalArgumentException"> if {@code
  /// pattern} is {@code null} </exception> <exception
  /// cref="IllegalArgumentException"> if {@code labels} is {@code null}
  /// </exception>
  ParseTreeMatch(ParseTree* tree, ParseTreePattern const& pattern,
                 const std::map<std::string, std::vector<ParseTree*>>& labels,
                 ParseTree* mismatchedNode);
  ParseTreeMatch(ParseTreeMatch const&) = default;
  virtual ~ParseTreeMatch();
  ParseTreeMatch& operator=(ParseTreeMatch const&) = default;

  /// <summary>
  /// Get the last node associated with a specific {@code label}.
  /// <p/>
  /// For example, for pattern {@code <id:ID>}, {@code get("id")} returns the
  /// node matched for that {@code ID}. If more than one node
  /// matched the specified label, only the last is returned. If there is
  /// no node associated with the label, this returns {@code null}.
  /// <p/>
  /// Pattern tags like {@code <ID>} and {@code <expr>} without labels are
  /// considered to be labeled with {@code ID} and {@code expr}, respectively.
  /// </summary>
  /// <param name="labe"> The label to check.
  /// </param>
  /// <returns> The last <seealso cref="ParseTree"/> to match a tag with the
  /// specified label, or {@code null} if no parse tree matched a tag with the
  /// label. </returns>
  virtual ParseTree* get(const std::string& label);

  /// <summary>
  /// Return all nodes matching a rule or token tag with the specified label.
  /// <p/>
  /// If the {@code label} is the name of a parser rule or token in the
  /// grammar, the resulting list will contain both the parse trees matching
  /// rule or tags explicitly labeled with the label and the complete set of
  /// parse trees matching the labeled and unlabeled tags in the pattern for
  /// the parser rule or token. For example, if {@code label} is {@code "foo"},
  /// the result will contain <em>all</em> of the following.
  ///
  /// <ul>
  /// <li>Parse tree nodes matching tags of the form {@code <foo:anyRuleName>}
  /// and
  /// {@code <foo:AnyTokenName>}.</li>
  /// <li>Parse tree nodes matching tags of the form {@code
  /// <anyLabel:foo>}.</li> <li>Parse tree nodes matching tags of the form
  /// {@code <foo>}.</li>
  /// </ul>
  /// </summary>
  /// <param name="labe"> The label.
  /// </param>
  /// <returns> A collection of all <seealso cref="ParseTree"/> nodes matching
  /// tags with the specified {@code label}. If no nodes matched the label, an
  /// empty list is returned. </returns>
  virtual std::vector<ParseTree*> getAll(const std::string& label);

  /// <summary>
  /// Return a mapping from label &rarr; [list of nodes].
  /// <p/>
  /// The map includes special entries corresponding to the names of rules and
  /// tokens referenced in tags in the original pattern. For additional
  /// information, see the description of <seealso cref="#getAll(String)"/>.
  /// </summary>
  /// <returns> A mapping from labels to parse tree nodes. If the parse tree
  /// pattern did not contain any rule or token tags, this map will be empty.
  /// </returns>
  virtual std::map<std::string, std::vector<ParseTree*>>& getLabels();

  /// <summary>
  /// Get the node at which we first detected a mismatch.
  /// </summary>
  /// <returns> the node at which we first detected a mismatch, or {@code null}
  /// if the match was successful. </returns>
  virtual ParseTree* getMismatchedNode();

  /// <summary>
  /// Gets a value indicating whether the match operation succeeded.
  /// </summary>
  /// <returns> {@code true} if the match operation succeeded; otherwise,
  /// {@code false}. </returns>
  virtual bool succeeded();

  /// <summary>
  /// Get the tree pattern we are matching against.
  /// </summary>
  /// <returns> The tree pattern we are matching against. </returns>
  virtual const ParseTreePattern& getPattern();

  /// <summary>
  /// Get the parse tree we are trying to match to a pattern.
  /// </summary>
  /// <returns> The <seealso cref="ParseTree"/> we are trying to match to a
  /// pattern. </returns>
  virtual ParseTree* getTree();

  virtual std::string toString();
};

}  // namespace pattern
}  // namespace tree
}  // namespace antlr4
