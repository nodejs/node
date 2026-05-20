// Copyright 2011 the V8 project authors. All rights reserved.
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

// Test proper handling of keywords, future reserved words and
// future reserved words in strict mode as specific by 7.6.1 and 7.6.2
// in ECMA-262.

// This code is based on:
// http://trac.webkit.org/export/89109/trunk/LayoutTests/fast/js/script-tests/keywords-and-reserved_words.js

function isKeyword(x)
{
  try {
    eval("var " + x + ";");
  } catch(e) {
    return true;
  }

  return false;
}

function isStrictKeyword(x)
{
  try {
    eval("'use strict'; var "+x+";");
  } catch(e) {
    return true;
  }

  return false;
}

function classifyIdentifier(x)
{
  if (isKeyword(x)) {
    // All non-strict keywords are also keywords in strict code.
    if (!isStrictKeyword(x)) {
      return "ERROR";
    }
    return "keyword";
  }

  // Check for strict mode future reserved words.
  if (isStrictKeyword(x)) {
    return "strict";
  }

  return "identifier";
}

function testKeyword(word) {
  // Classify word
  assertEquals("keyword", classifyIdentifier(word));

  // Simple use of a keyword
  assertThrows("var " + word + " = 1;", SyntaxError);
  if (word != "this") {
    assertThrows("typeof (" + word + ");", SyntaxError);
  }

  // object literal properties
  eval("var x = { " + word + " : 42 };");
  eval("var x = { get " + word + " () {} };");
  eval("var x = { set " + word + " (value) {} };");

  // object literal with string literal property names
  eval("var x = { '" + word + "' : 42 };");
  eval("var x = { get '" + word + "' () { } };");
  eval("var x = { set '" + word + "' (value) { } };");

  // Function names and arguments
  assertThrows("function " + word + " () { }", SyntaxError);
  assertThrows("function foo (" + word + ") {}", SyntaxError);
  assertThrows("function foo (a, " + word + ") { }", SyntaxError);
  assertThrows("function foo (" + word + ", a) { }", SyntaxError);
  assertThrows("function foo (a, " + word + ", b) { }", SyntaxError);
  assertThrows("var foo = function (" + word + ") { }", SyntaxError);

  // setter parameter
  assertThrows("var x = { set foo(" + word + ") { } };", SyntaxError);
}

// Not keywords - these are all just identifiers.
var identifiers = [
  "x",            "keyword",
  "id",           "strict",
  "identifier",   "use",
  // The following are reserved in ES3 but not in ES5.
  "abstract",     "int",
  "boolean",      "long",
  "byte",         "native",
  "char",         "short",
  "double",       "synchronized",
  "final",        "throws",
  "float",        "transient",
  "goto",         "volatile" ];

for (var i = 0; i < identifiers.length; i++) {
  assertEquals ("identifier", classifyIdentifier(identifiers[i]));
}

// 7.6.1.1 Keywords
var keywords = [
  "break",        "in",
  "case",         "instanceof",
  "catch",        "new",
  "continue",     "return",
  "debugger",     "switch",
  "default",      "this",
  "delete",       "throw",
  "do",           "try",
  "else",         "typeof",
  "finally",      "var",
  "for",          "void",
  "function",     "while",
  "if",           "with",
  // In ES5 "const" is a "future reserved word" but we treat it as a keyword.
  "const" ];

for (var i = 0; i < keywords.length; i++) {
  testKeyword(keywords[i]);
}

// 7.6.1.2 Future Reserved Words (without "const")
var future_reserved_words = [
  "class",
  "enum",
  "export",
  "extends",
  "import",
  "super" ];

for (var i = 0; i < future_reserved_words.length; i++) {
  testKeyword(future_reserved_words[i]);
}

// 7.6.1.2 Future Reserved Words, in strict mode only.
var future_strict_reserved_words = [
  "implements",
  "interface",
  "let",
  "package",
  "private",
  "protected",
  "public",
  "static",
  "yield" ];

for (var i = 0; i < future_strict_reserved_words.length; i++) {
  assertEquals ("strict", classifyIdentifier(future_strict_reserved_words[i]));
}

// More strict mode specific tests can be found in mjsunit/strict-mode.js.
