// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/prelude.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "experimental/redline/base/log.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"

namespace redline {

namespace {

// Keep this small: it runs before every execution. The functions evaluate
// their arguments (so that the values flow) but never throw, except
// assertThrows-style helpers which must run their callback.
const char kDefaultPrelude[] = R"JS(
var __redline_noop = function() {};
var __redline_call = function(f) {
  try { return typeof f === "function" ? f() : eval(String(f)); } catch (e) { return e; }
};
var assertSame = function(a, b) { return a === b; };
var assertNotSame = function(a, b) { return a !== b; };
var assertEquals = function(a, b) { return a == b; };
var assertNotEquals = function(a, b) { return a != b; };
var assertEqualsDelta = function(a, b, d) { return Math.abs(a - b) <= d; };
var assertArrayEquals = function(a, b) { return a && b && a.length === b.length; };
var assertPropertiesEqual = function(a, b) { return a && b && Object.keys(a).length === Object.keys(b).length; };
var assertToStringEquals = function(a, b) { return a == String(b); };
var assertTrue = function(a) { return !!a; };
var assertFalse = function(a) { return !a; };
var assertNull = function(a) { return a === null; };
var assertNotNull = function(a) { return a !== null; };
var assertThrows = function(f) { return __redline_call(f); };
var assertThrowsEquals = function(f) { return __redline_call(f); };
var assertThrowsAtRuntime = function(f) { return __redline_call(f); };
var assertThrowsAsync = function(p) { if (p && p.then) p.then(__redline_noop, __redline_noop); return p; };
var assertDoesNotThrow = function(f) { return __redline_call(f); };
var assertEarlyError = function(s) { try { new Function(s); } catch (e) {} };
var assertException = __redline_call;
var assertUnreachable = __redline_noop;
var assertContains = function(a, b) { return b && b.indexOf && b.indexOf(a) >= 0; };
var assertMatches = function(r, s) { return r && r.test && r.test(s); };
var assertInstanceof = function(o, c) { return o instanceof c; };
var assertPromiseResult = function(p, f, r) {
  if (p && p.then) p.then(typeof f === "function" ? f : __redline_noop, typeof r === "function" ? r : __redline_noop);
  return p;
};
var assertOptimized = __redline_noop;
var assertUnoptimized = __redline_noop;
var assertMaglevved = __redline_noop;
var assertNotMaglevved = __redline_noop;
var fail = __redline_noop;
var failWithMessage = __redline_noop;
var isNeverOptimize = function() { return false; };
var isNeverOptimizeLiteMode = function() { return false; };
var isLazy = function() { return false; };
var isInterpreted = function(f) { return %ActiveTierIsIgnition(f); };
var isBaseline = function(f) { return %ActiveTierIsSparkplug(f); };
var isUnoptimized = function(f) { return !%ActiveTierIsMaglev(f) && !%ActiveTierIsTurbofan(f); };
var isOptimized = function(f) { return %ActiveTierIsMaglev(f) || %ActiveTierIsTurbofan(f); };
var isMaglevved = function(f) { return %ActiveTierIsMaglev(f); };
var isTurboFanned = function(f) { return %ActiveTierIsTurbofan(f); };
var topFrameIsInterpreted = function() { return false; };
var topFrameIsBaseline = function() { return false; };
var topFrameIsMaglevved = function() { return false; };
var topFrameIsTurboFanned = function() { return false; };
var willBeMaglevved = function() { return true; };
var willBeTurbofanned = function() { return true; };
var prettyPrinted = function(v) { try { return String(v); } catch (e) { return ""; } };
var deepEquals = function(a, b) { return a == b; };
var promiseTestChain = Promise.resolve();
var MjsUnitAssertionError = function(m) { this.message = m; };
var load = __redline_noop;
var quit = __redline_noop;
var testRunner = { quit: __redline_noop };
try { if (typeof d8 === "object" && d8.file) d8.file.execute = __redline_noop; } catch (e) {}
)JS";

}  // namespace

std::string_view DefaultPrelude() { return kDefaultPrelude; }

// static
std::unique_ptr<ScriptTree> ScriptTree::Parse(std::string_view prelude,
                                              std::string_view program,
                                              std::string* error) {
  fast::ParseError parse_error;
  size_t prelude_count = 0;
  if (!prelude.empty()) {
    std::unique_ptr<fast::Tree> alone =
        fast::Parser::ParseScript(prelude, fast::ParseOptions(), &parse_error);
    if (alone == nullptr) {
      *error = "the prelude does not parse: " + parse_error.ToString();
      return nullptr;
    }
    prelude_count = alone->root()->body.size();
  }
  std::unique_ptr<fast::Tree> alone =
      fast::Parser::ParseScript(program, fast::ParseOptions(), &parse_error);
  if (alone == nullptr) {
    *error = "the program does not parse: " + parse_error.ToString();
    return nullptr;
  }
  const size_t program_count = alone->root()->body.size();
  std::unique_ptr<ScriptTree> script(new ScriptTree());
  if (prelude.empty()) {
    script->tree_ = std::move(alone);
  } else {
    std::string text;
    text.reserve(prelude.size() + 1 + program.size());
    text.append(prelude).append("\n").append(program);
    script->tree_ =
        fast::Parser::ParseScript(text, fast::ParseOptions(), &parse_error);
    if (script->tree_ == nullptr) {
      *error = "the script does not parse: " + parse_error.ToString();
      return nullptr;
    }
  }
  const std::vector<fast::Statement*>& body = script->tree_->root()->body;
  // For example, a prelude that ends without a semicolon can continue in
  // the first statement of the program.
  if (body.size() != prelude_count + program_count) {
    *error = StringPrintf(
        "the script has %zu top-level statements, not %zu of the prelude and "
        "%zu of the program",
        body.size(), prelude_count, program_count);
    return nullptr;
  }
  for (size_t i = 0; i < prelude_count; i++) {
    fast::Walk(body[i], [&](fast::Node* node) {
      script->first_program_id_ =
          std::max(script->first_program_id_, node->id + 1);
      return true;
    });
  }
  script->prelude_statements_ = prelude_count;
  return script;
}

size_t ScriptTree::PreludeStatementsNow() const {
  const std::vector<fast::Statement*>& body = tree_->root()->body;
  return static_cast<size_t>(std::count_if(
      body.begin(), body.end(),
      [this](const fast::Statement* s) { return FromPrelude(s); }));
}

size_t ScriptTree::ProgramNodes() const {
  size_t nodes = 1;  // the root
  for (const fast::Statement* statement : tree_->root()->body) {
    if (!FromPrelude(statement)) {
      nodes += fast::CountNodes(statement);
    }
  }
  return nodes;
}

std::string ScriptTree::PrintProgram() {
  // Print the root with only the program statements, then restore it.
  std::vector<fast::Statement*>& body = tree_->root()->body;
  std::vector<fast::Statement*> all = body;
  body.erase(std::remove_if(
                 body.begin(), body.end(),
                 [this](const fast::Statement* s) { return FromPrelude(s); }),
             body.end());
  std::string text = fast::Print(tree_->root());
  body = std::move(all);
  return text;
}

}  // namespace redline
