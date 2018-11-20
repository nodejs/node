/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace tree {

/// <summary>
/// Associate a property with a parse tree node. Useful with parse tree
/// listeners that need to associate values with particular tree nodes, kind of
/// like specifying a return value for the listener event method that visited a
/// particular node. Example:
///
/// <pre>
/// ParseTreeProperty&lt;Integer&gt; values = new
/// ParseTreeProperty&lt;Integer&gt;(); values.put(tree, 36); int x =
/// values.get(tree); values.removeFrom(tree);
/// </pre>
///
/// You would make one decl (values here) in the listener and use lots of times
/// in your event methods.
/// </summary>
template <typename V>
class ANTLR4CPP_PUBLIC ParseTreeProperty {
 public:
  virtual V get(ParseTree* node) { return _annotations[node]; }
  virtual void put(ParseTree* node, V value) { _annotations[node] = value; }
  virtual V removeFrom(ParseTree* node) {
    auto value = _annotations[node];
    _annotations.erase(node);
    return value;
  }

 protected:
  std::map<ParseTree*, V> _annotations;
};

}  // namespace tree
}  // namespace antlr4
