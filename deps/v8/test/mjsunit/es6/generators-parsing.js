// Copyright 2013 the V8 project authors. All rights reserved.
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

// Test basic generator syntax.

// Yield statements.
function* g() { yield 3; yield 4; }

// Yield expressions.
function* g() { (yield 3) + (yield 4); }

// Yield without a RHS.
function* g() { yield; }
function* g() { yield }
function* g() {
  yield
}
function* g() { (yield) }
function* g() { [yield] }
function* g() { {yield} }
function* g() { yield, yield }
function* g() { yield; yield }
function* g() { (yield) ? yield : yield }
function* g() {
  (yield)
  ? yield
  : yield
}

// If yield has a RHS, it needs to start on the same line.  The * in a
// yield* counts as starting the RHS.
function* g() {
  yield *
  foo
}
assertThrows("function* g() { yield\n* foo }", SyntaxError);
assertEquals(undefined,
             (function*(){
               yield
               3
             })().next().value);

// A YieldExpression is not a LogicalORExpression.
assertThrows("function* g() { yield ? yield : yield }", SyntaxError);

// You can have a generator in strict mode.
function* g() { "use strict"; yield 3; yield 4; }

// Generators can have return statements also, which internally parse to a kind
// of yield expression.
function* g() { yield 1; return; }
function* g() { yield 1; return 2; }
function* g() { yield 1; return 2; yield "dead"; }

// Generator expression.
(function* () { yield 3; });

// Named generator expression.
(function* g() { yield 3; });

// You can have a generator without a yield.
function* g() { }

// A YieldExpression is valid as the RHS of a YieldExpression.
function* g() { yield yield 1; }
function* g() { yield 3 + (yield 4); }

// Generator definitions with a name of "yield" are not specifically ruled out
// by the spec, as the `yield' name is outside the generator itself.  However,
// in strict-mode, "yield" is an invalid identifier.
function* yield() { (yield 3) + (yield 4); }
assertThrows("function* yield() { \"use strict\"; (yield 3) + (yield 4); }",
             SyntaxError);

// In sloppy mode, yield is a normal identifier, outside of generators.
function yield(yield) { yield: yield (yield + yield (0)); }

// Yield is always valid as a key in an object literal.
({ yield: 1 });
function* g() { yield ({ yield: 1 }) }
function* g() { yield ({ get yield() { return 1; }}) }

// Checks that yield is a valid label in sloppy mode, but not valid in a strict
// mode or in generators.
function f() { yield: 1 }
assertThrows("function f() { \"use strict\"; yield: 1 }", SyntaxError)
assertThrows("function* g() { yield: 1 }", SyntaxError)

// Yield is only a keyword in the body of the generator, not in nested
// functions.
function* g() { function f() { yield (yield + yield (0)); } }

// Yield in a generator is not an identifier.
assertThrows("function* g() { yield = 10; }", SyntaxError);

// Yield binds very loosely, so this parses as "yield (3 + yield 4)", which is
// invalid.
assertThrows("function* g() { yield 3 + yield 4; }", SyntaxError);

// Yield is still a future-reserved-word in strict mode
assertThrows("function f() { \"use strict\"; var yield = 13; }", SyntaxError);

// The name of the NFE is let-bound in G, so is invalid.
assertThrows("function* g() { yield (function yield() {}); }", SyntaxError);

// In generators, yield is invalid as a formal argument name.
assertThrows("function* g(yield) { yield (10); }", SyntaxError);
