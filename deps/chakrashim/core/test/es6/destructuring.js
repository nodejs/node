//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
  {
    name: "Basic parsing and early errors",
    body: function () {
      // No initializer
      assert.throws(function () { eval("var [];"); },    SyntaxError, "Destructured var array declaration must have at least one identifier reference",   "Destructuring declarations must have an initializer");
      assert.throws(function () { eval("let [];"); },    SyntaxError, "Destructured let array declaration must have at least one identifier reference",   "Destructuring declarations must have an initializer");
      assert.throws(function () { eval("const [];"); },  SyntaxError, "Destructured const array declaration must have at least one identifier reference", "Destructuring declarations must have an initializer");
      assert.throws(function () { eval("var [a];"); },   SyntaxError, "Destructured var array declaration must have an initializer",                      "Destructuring declarations must have an initializer");
      assert.throws(function () { eval("let [a];"); },   SyntaxError, "Destructured let array declaration must have an initializer",                      "Destructuring declarations must have an initializer");
      assert.throws(function () { eval("const [a];"); }, SyntaxError, "Destructured const array declaration must have an initializer",                    "Destructuring declarations must have an initializer");

      // No identifiers
      assert.doesNotThrow(function () { eval("var [] = [];"); },   "Destructured var array declaration with no identifiers does not throw");
      assert.doesNotThrow(function () { eval("let [] = [];"); },   "Destructured let array declaration with no identifiers does not throw");
      assert.doesNotThrow(function () { eval("const [] = [];"); }, "Destructured const array declaration with no identifiers does not throw");
      assert.doesNotThrow(function () { eval("[] = [];"); },       "Destructured array assignment with no identifiers does not throw");

      // Mismatched expression and initializer length
      assert.doesNotThrow(function () { eval("var [a] = [];"); },    "Destructured var array declaration with an empty initializer array does not throw");
      assert.doesNotThrow(function () { eval("let [a] = [];"); },    "Destructured let array declaration with an empty initializer array does not throw");
      assert.doesNotThrow(function () { eval("const [a] = [];"); },  "Destructured const array declaration with an empty initializer array does not throw");
      assert.doesNotThrow(function () { eval("var a; [a] = [];"); }, "Destructured var array assignment with an empty initializer array does not throw");
      assert.doesNotThrow(function () { eval("let a; [a] = [];"); }, "Destructured let array assignment with an empty initializer array does not throw");

      assert.doesNotThrow(function () { eval("var [a] = [1];"); },    "Destructured var array declaration with matching initializer array size does not throw");
      assert.doesNotThrow(function () { eval("let [a] = [2];"); },    "Destructured let array declaration with matching initializer array size does not throw");
      assert.doesNotThrow(function () { eval("const [a] = [1];"); },  "Destructured const array declaration with matching initializer array size does not throw");
      assert.doesNotThrow(function () { eval("var a; [a] = [1];"); }, "Destructured var array assignment with matching initializer array size does not throw");
      assert.doesNotThrow(function () { eval("let a; [a] = [2];"); }, "Destructured let array assignment with matching initializer array size does not throw");

      assert.doesNotThrow(function () { eval("var [a, b] = [1];"); },       "Destructured var array declaration with smaller initializer array size does not throw");
      assert.doesNotThrow(function () { eval("let [a, b] = [1];"); },       "Destructured let array declaration with smaller initializer array size does not throw");
      assert.doesNotThrow(function () { eval("const [a, b] = [1];"); },     "Destructured const array declaration with smaller initializer array size does not throw");
      assert.doesNotThrow(function () { eval("var a, b; [a, b] = [1];"); }, "Destructured var array assignment with smaller initializer array size does not throw");
      assert.doesNotThrow(function () { eval("let a, b; [a, b] = [1];"); }, "Destructured let array assignment with smaller initializer array size does not throw");

      assert.doesNotThrow(function () { eval("var [a] = [1, 2];"); },       "Destructured var array declaration with larger initializer array size does not throw");
      assert.doesNotThrow(function () { eval("let [a] = [1, 2];"); },       "Destructured let array declaration with larger initializer array size does not throw");
      assert.doesNotThrow(function () { eval("const [a] = [1, 2];"); },     "Destructured const array declaration with larger initializer array size does not throw");
      assert.doesNotThrow(function () { eval("var a; [a] = [1, 2];"); },    "Destructured var array assignment with larger initializer array size does not throw");
      assert.doesNotThrow(function () { eval("let a; [a] = [1, 2];"); },    "Destructured let array assignment with larger initializer array size does not throw");

      // Disallowed operators
      assert.throws(function () { eval("var [a--] = [];"); },        SyntaxError, "Destructured var array declaration with an operator throws",   "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("let [a--] = [];"); },        SyntaxError, "Destructured let array declaration with an operator throws",   "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("const [a--] = [];"); },      SyntaxError, "Destructured const array declaration with an operator throws", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("var [a + 1] = [];"); },      SyntaxError, "Destructured var array declaration with an operator throws",   "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("let [a + 1] = [];"); },      SyntaxError, "Destructured let array declaration with an operator throws",   "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("const [a + 1] = [];"); },    SyntaxError, "Destructured const array declaration with an operator throws", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("var [++a] = [];"); },        SyntaxError, "Destructured var array declaration with an operator throws",   "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("let [++a] = [];"); },        SyntaxError, "Destructured let array declaration with an operator throws",   "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("const [++a] = [];"); },      SyntaxError, "Destructured const array declaration with an operator throws", "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("var a; [a--] = [];"); },     SyntaxError, "Destructured var array assignment with an operator throws",    "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("let a; [a--] = [];"); },     SyntaxError, "Destructured let array assignment with an operator throws",    "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("var a; [a + 1] = [];"); },   SyntaxError, "Destructured var array assignment with an operator throws",    "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("let a; [a + 1] = [];"); },   SyntaxError, "Destructured let array assignment with an operator throws",    "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("var a; [++a] = [];"); },     SyntaxError, "Destructured var array assignment with an operator throws",    "Unexpected operator in destructuring expression");
      assert.throws(function () { eval("let a; [++a] = [];"); },     SyntaxError, "Destructured let array assignment with an operator throws",    "Unexpected operator in destructuring expression");
      assert.doesNotThrow(function () { eval("var a = [1], i = 0; [a[i++]] = [];"); }, "Destructured var array assignment operators inside an identifier reference does not throw");
      assert.doesNotThrow(function () { eval("let a = [1], i = 0; [a[i++]] = [];"); }, "Destructured var array assignment operators inside an identifier reference does not throw");
      //assert.doesNotThrow(function () { eval("var a = [1], i = 0; [a[(() => 1 + i)()]] = [];"); }, "Destructured var array assignment operators inside an identifier reference does not throw");
      //assert.doesNotThrow(function () { eval("let a = [1], i = 0; [a[(() => 1 + i)()]] = [];"); }, "Destructured var array assignment operators inside an identifier reference does not throw");

      // Missing values
      assert.doesNotThrow(function () { eval("var [,] = [];"); },   "Destructured var array declaration with no identifiers and missing values does not throw");
      assert.doesNotThrow(function () { eval("let [,] = [];"); },   "Destructured let array declaration with no identifiers and missing values does not throw");
      assert.doesNotThrow(function () { eval("const [,] = [];"); }, "Destructured const array declaration with no identifiers and missing values does not throw");
      assert.doesNotThrow(function () { eval("[,] = [];"); },       "Destructured array assignment with no identifiers and missing values does not throw");

      assert.doesNotThrow(function () { eval("var [a,] = [];"); },    "Destructured var array declaration ending with a missing value does not throw");
      assert.doesNotThrow(function () { eval("let [a,] = [];"); },    "Destructured let array declaration ending with a missing value does not throw");
      assert.doesNotThrow(function () { eval("const [a,] = [];"); },  "Destructured const array declaration ending with a missing value does not throw");
      assert.doesNotThrow(function () { eval("var a; [a,] = [];"); }, "Destructured var array assignment ending with a missing value does not throw");
      assert.doesNotThrow(function () { eval("let a; [a,] = [];"); }, "Destructured let array assignment ending with a missing value does not throw");

      assert.doesNotThrow(function () { eval("var [a,,b] = [];"); },       "Destructured var array declaration with some missing values does not throw");
      assert.doesNotThrow(function () { eval("let [a,,b] = [];"); },       "Destructured let array declaration with some missing values does not throw");
      assert.doesNotThrow(function () { eval("const [a,,b] = [];"); },     "Destructured const array declaration with some missing values does not throw");
      assert.doesNotThrow(function () { eval("var a, b; [a,,b] = [];"); }, "Destructured var array assignment with some missing values does not throw");
      assert.doesNotThrow(function () { eval("let a, b; [a,,b] = [];"); }, "Destructured let array assignment with some missing values does not throw");

      assert.doesNotThrow(function () { eval("var [,,a] = [];"); },    "Destructured var array declaration beginning with missing values does not throw");
      assert.doesNotThrow(function () { eval("let [,,a] = [];"); },    "Destructured let array declaration beginning with missing values does not throw");
      assert.doesNotThrow(function () { eval("const [,,a] = [];"); },  "Destructured const array declaration beginning with missing values does not throw");
      assert.doesNotThrow(function () { eval("var a; [,,a] = [];"); }, "Destructured var array assignment beginning with missing values does not throw");
      assert.doesNotThrow(function () { eval("let a; [,,a] = [];"); }, "Destructured let array assignment beginning with missing values does not throw");

      assert.doesNotThrow(function () { eval("var [a] = [,,];"); },    "Destructured var array declaration with an initializer containing missing values does not throw");
      assert.doesNotThrow(function () { eval("let [a] = [,,];"); },    "Destructured let array declaration with an initializer containing missing values does not throw");
      assert.doesNotThrow(function () { eval("const [a] = [,,];"); },  "Destructured const array declaration with an initializer containing missing values does not throw");
      assert.doesNotThrow(function () { eval("var a; [a] = [,,];"); }, "Destructured var array assignment with an initializer containing missing values does not throw");
      assert.doesNotThrow(function () { eval("let a; [a] = [,,];"); }, "Destructured let array assignment with an initializer containing missing values does not throw");

      // Non-identifiers
      assert.throws(function () { eval("var [1] = [];"); },         SyntaxError, "Destructured var array declaration with no identifier references throws",                  "Destructuring expressions can only have identifier references");
      assert.throws(function () { eval("let [1] = [];"); },         SyntaxError, "Destructured let array declaration with no identifier references throws",                  "Destructuring expressions can only have identifier references");
      assert.throws(function () { eval("const [1] = [];"); },       SyntaxError, "Destructured const array declaration with no identifier references throws",                "Destructuring expressions can only have identifier references");
      assert.throws(function () { eval("[1] = [];"); },             SyntaxError, "Destructured array assignment with no identifier references throws",                       "Destructuring expressions can only have identifier references");
      assert.throws(function () { eval("var [1, a] = [];"); },      SyntaxError, "Destructured var array declaration with valid and invalid identifier references throws",   "Destructuring expressions can only have identifier references");
      assert.throws(function () { eval("let [1, a] = [];"); },      SyntaxError, "Destructured let array declaration with valid and invalid identifier references throws",   "Destructuring expressions can only have identifier references");
      assert.throws(function () { eval("const [1, a] = [];"); },    SyntaxError, "Destructured const array declaration with valid and invalid identifier references throws", "Destructuring expressions can only have identifier references");
      assert.throws(function () { eval("var a; [1, a] = [];"); },   SyntaxError, "Destructured var array assignment with valid and invalid identifier references throws",    "Destructuring expressions can only have identifier references");
      assert.throws(function () { eval("let a; [1, a] = [];"); },   SyntaxError, "Destructured let array assignment with valid and invalid identifier references throws",    "Destructuring expressions can only have identifier references");

      // Rest parameter
      assert.doesNotThrow(function () { eval("var [...a] = [];"); },    "Destructured var array declaration with a single rest parameter does not throw");
      assert.doesNotThrow(function () { eval("let [...a] = [];"); },    "Destructured let array declaration with a single rest parameter does not throw");
      assert.doesNotThrow(function () { eval("const [...a] = [];"); },  "Destructured const array declaration with a single rest parameter does not throw");
      assert.doesNotThrow(function () { eval("var a; [...a] = [];"); }, "Destructured var array assignment with a single rest parameter does not throw");
      assert.doesNotThrow(function () { eval("let a; [...a] = [];"); }, "Destructured let array assignment with a single rest parameter does not throw");

      assert.throws(function () { eval("var [...a, ...b] = [];"); },         SyntaxError, "Destructured var array declaration with multiple rest parameters throws",   "Destructuring rest variables must be in the last position of the expression");
      assert.throws(function () { eval("let [...a, ...b] = [];"); },         SyntaxError, "Destructured let array declaration with multiple rest parameters throws",   "Destructuring rest variables must be in the last position of the expression");
      assert.throws(function () { eval("const [...a, ...b] = [];"); },       SyntaxError, "Destructured const array declaration with multiple rest parameters throws", "Destructuring rest variables must be in the last position of the expression");
      assert.throws(function () { eval("var a, b; [...a, ...b] = [];"); },   SyntaxError, "Destructured var array assignment with multiple rest parameters throws",    "Destructuring rest variables must be in the last position of the expression");
      assert.throws(function () { eval("let a, b; [...a, ...b] = [];"); },   SyntaxError, "Destructured let array assignment with multiple rest parameters throws",    "Destructuring rest variables must be in the last position of the expression");

      assert.throws(function () { eval("var [...a, b] = [];"); },         SyntaxError, "Destructured var array declaration with rest parameter in non-last position throws",   "Destructuring rest variables must be in the last position of the expression");
      assert.throws(function () { eval("let [...a, b] = [];"); },         SyntaxError, "Destructured let array declaration with rest parameter in non-last position throws",   "Destructuring rest variables must be in the last position of the expression");
      assert.throws(function () { eval("const [...a, b] = [];"); },       SyntaxError, "Destructured const array declaration with rest parameter in non-last position throws", "Destructuring rest variables must be in the last position of the expression");
      assert.throws(function () { eval("var a, b; [...a, b] = [];"); },   SyntaxError, "Destructured var array assignment with rest parameter in non-last position throws",    "Destructuring rest variables must be in the last position of the expression");
      assert.throws(function () { eval("let a, b; [...a, b] = [];"); },   SyntaxError, "Destructured let array assignment with rest parameter in non-last position throws",    "Destructuring rest variables must be in the last position of the expression");

      // Default values
      assert.doesNotThrow(function () { eval("var [a = 1] = [];"); },    "Destructured var array declaration with all default values does not throw");
      assert.doesNotThrow(function () { eval("let [a = 1] = [];"); },    "Destructured let array declaration with all default values does not throw");
      assert.doesNotThrow(function () { eval("const [a = 1] = [];"); },  "Destructured const array declaration with all default values does not throw");
      assert.doesNotThrow(function () { eval("var a; [a = 1] = [];"); }, "Destructured var array assignment with all default values does not throw");
      assert.doesNotThrow(function () { eval("let a; [a = 1] = [];"); }, "Destructured let array assignment with all default values does not throw");

      assert.doesNotThrow(function () { eval("var [a, b = 1] = [];"); },       "Destructured var array declaration with trailing default values does not throw");
      assert.doesNotThrow(function () { eval("let [a, b = 1] = [];"); },       "Destructured let array declaration with trailing default values does not throw");
      assert.doesNotThrow(function () { eval("const [a, b = 1] = [];"); },     "Destructured const array declaration with trailing default values does not throw");
      assert.doesNotThrow(function () { eval("var a, b; [a, b = 1] = [];"); }, "Destructured var array assignment with trailing default values does not throw");
      assert.doesNotThrow(function () { eval("let a, b; [a, b = 1] = [];"); }, "Destructured let array assignment with trailing default values does not throw");

      assert.doesNotThrow(function () { eval("var [a = 1, b] = [];"); },       "Destructured var array declaration with mixed default values does not throw");
      assert.doesNotThrow(function () { eval("let [a = 1, b] = [];"); },       "Destructured let array declaration with mixed default values does not throw");
      assert.doesNotThrow(function () { eval("const [a = 1, b] = [];"); },     "Destructured const array declaration with mixed default values does not throw");
      assert.doesNotThrow(function () { eval("var a, b; [a = 1, b] = [];"); }, "Destructured var array assignment with mixed default values does not throw");
      assert.doesNotThrow(function () { eval("let a, b; [a = 1, b] = [];"); }, "Destructured let array assignment with mixed default values does not throw");

      // Rest with default
      assert.throws(function () { eval("var [...a = 1] = [];"); },      SyntaxError, "Destructured var array declaration with a rest parameter with a default value throws",   "Unexpected default initializer");
      assert.throws(function () { eval("let [...a = 1] = [];"); },      SyntaxError, "Destructured let array declaration with a rest parameter with a default value throws",   "Unexpected default initializer");
      assert.throws(function () { eval("const [...a = 1] = [];"); },    SyntaxError, "Destructured const array declaration with a rest parameter with a default value throws", "Unexpected default initializer");
      assert.throws(function () { eval("var a; [...a = 1] = [];"); },   SyntaxError, "Destructured var array assignment with a rest parameter with a default value throws",    "The rest parameter cannot have a default intializer.");
      assert.throws(function () { eval("let a; [...a = 1] = [];"); },   SyntaxError, "Destructured let array assignment with a rest parameter with a default value throws",    "The rest parameter cannot have a default intializer.");

      // Nesting
      assert.doesNotThrow(function () { eval("var [[a]] = [[]];"); },    "Destructured var array declaration with nesting does not throw");
      assert.doesNotThrow(function () { eval("let [[a]] = [[]];"); },    "Destructured let array declaration with nesting does not throw");
      assert.doesNotThrow(function () { eval("const [[a]] = [[]];"); },  "Destructured const array declaration with nesting does not throw");
      assert.doesNotThrow(function () { eval("var a; [[a]] = [[]];"); }, "Destructured var array assignment with nesting does not throw");
      assert.doesNotThrow(function () { eval("let a; [[a]] = [[]];"); }, "Destructured let array assignment with nesting does not throw");

      assert.doesNotThrow(function () { eval("var [a, [b]] = [1, []];"); },       "Destructured var array declaration with some nesting does not throw");
      assert.doesNotThrow(function () { eval("let [a, [b]] = [1, []];"); },       "Destructured let array declaration with some nesting does not throw");
      assert.doesNotThrow(function () { eval("const [a, [b]] = [1, []];"); },     "Destructured const array declaration with some nesting does not throw");
      assert.doesNotThrow(function () { eval("var a, b; [a, [b]] = [1, []];"); }, "Destructured var array assignment with some nesting does not throw");
      assert.doesNotThrow(function () { eval("let a, b; [a, [b]] = [1, []];"); }, "Destructured let array assignment with some nesting does not throw");

      assert.throws(function () { eval("var [((a)] = [];"); },    SyntaxError, "Destructured var array declaration with a mismatched paren count throws",   "Expected ')'");
      assert.throws(function () { eval("let [((a)] = [];"); },    SyntaxError, "Destructured let array declaration with a mismatched paren count throws",   "Expected ')'");
      assert.throws(function () { eval("const [((a)] = [];"); },  SyntaxError, "Destructured const array declaration with a mismatched paren count throws", "Expected ')'");
      assert.throws(function () { eval("var a; [((a)] = [];"); }, SyntaxError, "Destructured var array assignment with a mismatched paren count throws",    "Expected ')'");
      assert.throws(function () { eval("let a; [((a)] = [];"); }, SyntaxError, "Destructured let array assignment with a mismatched paren count throws",    "Expected ')'");
      assert.throws(function () { eval("var [a)] = [];"); },      SyntaxError, "Destructured var array declaration with a mismatched paren count throws",   "Expected ')'");
      assert.throws(function () { eval("let [a)] = [];"); },      SyntaxError, "Destructured let array declaration with a mismatched paren count throws",   "Expected ')'");
      assert.throws(function () { eval("const [a)] = [];"); },    SyntaxError, "Destructured const array declaration with a mismatched paren count throws", "Expected ')'");
      assert.throws(function () { eval("var a; [a)] = [];"); },   SyntaxError, "Destructured var array assignment with a mismatched paren count throws",    "Expected ']'");
      assert.throws(function () { eval("let a; [a)] = [];"); },   SyntaxError, "Destructured let array assignment with a mismatched paren count throws",    "Expected ']'");
      assert.doesNotThrow(function () { eval("var [((((a)))), b] = [];"); },       "Destructured var array declaration with some nested parens does not throw");
      assert.doesNotThrow(function () { eval("let [((((a)))), b] = [];"); },       "Destructured let array declaration with some nested parens does not throw");
      assert.doesNotThrow(function () { eval("const [((((a)))), b] = [];"); },     "Destructured const array declaration with some nested parens does not throw");
      assert.doesNotThrow(function () { eval("var a, b; [((((a)))), b] = [];"); }, "Destructured var array assignment with some nested parens does not throw");
      assert.doesNotThrow(function () { eval("let a, b; [((((a)))), b] = [];"); }, "Destructured let array assignment with some nested parens does not throw");

      assert.doesNotThrow(function () { eval("var [[[...a]]] = [[[]]];"); },    "Destructured var array declaration with nested rest parameter does not throw");
      assert.doesNotThrow(function () { eval("let [[[...a]]] = [[[]]];"); },    "Destructured let array declaration with nested rest parameter does not throw");
      assert.doesNotThrow(function () { eval("const [[[...a]]] = [[[]]];"); },  "Destructured const array declaration with nested rest parameter does not throw");
      assert.doesNotThrow(function () { eval("var a; [[[...a]]] = [[[]]];"); }, "Destructured var array assignment with nested rest parameter does not throw");
      assert.doesNotThrow(function () { eval("let a; [[[...a]]] = [[[]]];"); }, "Destructured let array assignment with nested rest parameter does not throw");

      assert.doesNotThrow(function () { eval("var [[...a], ...b] = [[],];"); },       "Destructured var array declaration with valid nested rest parameters does not throw");
      assert.doesNotThrow(function () { eval("let [[...a], ...b] = [[],];"); },       "Destructured let array declaration with valid nested rest parameters does not throw");
      assert.doesNotThrow(function () { eval("const [[...a], ...b] = [[],];"); },     "Destructured const array declaration with valid nested rest parameters does not throw");
      assert.doesNotThrow(function () { eval("var a, b; [[...a], ...b] = [[],];"); }, "Destructured var array assignment with valid nested rest parameters does not throw");
      assert.doesNotThrow(function () { eval("let a, b; [[...a], ...b] = [[],];"); }, "Destructured let array assignment with valid nested rest parameters does not throw");

      assert.doesNotThrow(function () { eval("var [[(a)], ((((((([b])))))))] = [[],[]];"); },       "Destructured var array declaration with valid mixed paren and array nesting does not throw");
      assert.doesNotThrow(function () { eval("let [[(a)], ((((((([b])))))))] = [[],[]];"); },       "Destructured let array declaration with valid mixed paren and array nesting does not throw");
      assert.doesNotThrow(function () { eval("const [[(a)], ((((((([b])))))))] = [[],[]];"); },     "Destructured const array declaration with valid mixed paren and array nesting does not throw");
      assert.doesNotThrow(function () { eval("var a, b; [[(a)], ((((((([b])))))))] = [[],[]];"); }, "Destructured var array assignment with valid mixed paren and array nesting does not throw");
      assert.doesNotThrow(function () { eval("let a, b; [[(a)], ((((((([b])))))))] = [[],[]];"); }, "Destructured let array assignment with valid mixed paren and array nesting does not throw");

      // All the things
      // Currently disabled until default scoping is implemented.
//      assert.doesNotThrow(function () { eval("var [a = () => 1, [b, [c, d] = [1, 2], ([e])]] = [,[,,[]]];"); },                "Destructured var array declaration with mixed valid syntax does not throw");
//      assert.doesNotThrow(function () { eval("let [a = () => 1, [b, [c, d] = [1, 2], ([e])]] = [,[,,[]]];"); },                "Destructured let array declaration with mixed valid syntax does not throw");
//      assert.doesNotThrow(function () { eval("const [a = () => 1, [b, [c, d] = [1, 2], ([e])]] = [,[,,[]]];"); },              "Destructured const array declaration with mixed valid syntax does not throw");
//      assert.doesNotThrow(function () { eval("var a, b, c, d, e; [a = () => 1, [b, [c, d] = [1, 2], ([e])]] = [,[,,[]]];"); }, "Destructured var array declaration with mixed valid syntax does not throw");
//      assert.doesNotThrow(function () { eval("let a, b, c, d, e; [a = () => 1, [b, [c, d] = [1, 2], ([e])]] = [,[,,[]]];"); }, "Destructured let array declaration with mixed valid syntax does not throw");

      // Redeclarations
      assert.doesNotThrow(function () { eval("var [a, a] = [];"); },    "Destructured var array declaration with a repeated identifier reference does not throw");
      assert.throws(function () { eval("let [a, a] = [];"); },   SyntaxError, "Destructured let array declaration with a repeated identifier reference throws", "Let/Const redeclaration");
      assert.throws(function () { eval("const [a, a] = [];"); }, SyntaxError, "Destructured const array declaration with a repeated identifier reference throws", "Let/Const redeclaration");
      assert.doesNotThrow(function () { eval("var a; [a, a] = [];"); }, "Destructured var array assignment with a repeated identifier reference does not throw");
      assert.doesNotThrow(function () { eval("let a; [a, a] = [];"); }, "Destructured let array assignment with a repeated identifier reference does not throw");

      // Identifier property references
      assert.doesNotThrow(function () { eval("var a = {}; [a.x] = [];"); },      "Destructured var array assignment with an identifier property reference does not throw");
      assert.doesNotThrow(function () { eval("let a = {}; [a.x] = [];"); },      "Destructured let array assignment with an identifier property reference does not throw");
      assert.doesNotThrow(function () { eval("var a = {}; [a[\"x\"]] = [];"); }, "Destructured var array assignment with an identifier property reference does not throw");
      assert.doesNotThrow(function () { eval("let a = {}; [a[\"x\"]] = [];"); }, "Destructured let array assignment with an identifier property reference does not throw");

      // Call expression property references
      assert.throws(function () { eval("function foo() { return {}; }; var [foo()] = [];"); },       SyntaxError,    "Destructured var array declaration with a call expression throws",                      "Syntax error");
      assert.throws(function () { eval("function foo() { return {}; }; let [foo()] = [];"); },       SyntaxError,    "Destructured let array declaration with a call expression throws",                      "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() { return {}; }; const [foo()] = [];"); },     SyntaxError,    "Destructured const array declaration with a call expression throws",                    "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() { return {}; }; [foo()] = [];"); },           ReferenceError, "Destructured array assignment with a call expression throws",                           "Invalid left-hand side in assignment");
      assert.throws(function () { eval("function foo() { return {}; }; var [foo().x] = [];"); },     SyntaxError,    "Destructured var array declaration with a call expression property reference throws",   "Syntax error");
      assert.throws(function () { eval("function foo() { return {}; }; let [foo().x] = [];"); },     SyntaxError,    "Destructured let array declaration with a call expression property reference throws",   "Let/Const redeclaration");
      assert.throws(function () { eval("function foo() { return {}; }; const [foo().x] = [];"); },   SyntaxError,    "Destructured const array declaration with a call expression property reference throws", "Let/Const redeclaration");
      assert.doesNotThrow(function () { eval("function foo() { return {}; }; [foo().x] = [];"); },      "Destructured array assignment with super a property reference does not throw");
      assert.doesNotThrow(function () { eval("function foo() { return {}; }; [foo()[\"x\"]] = [];"); }, "Destructured array assignment with a call expression property reference does not throw");

      // Super property references
      assert.throws(function () { eval("class foo { method() { var [super()] = []; } }"); },     SyntaxError, "Destructured var array declaration with a super call throws",                         "The use of a keyword for an identifier is invalid");
      assert.throws(function () { eval("class foo { method() { let [super()] = []; } }"); },     SyntaxError, "Destructured let array declaration with a super call throws",                         "The use of a keyword for an identifier is invalid");
      assert.throws(function () { eval("class foo { method() { const [super()] = []; } }"); },   SyntaxError, "Destructured const array declaration with a super call throws",                       "The use of a keyword for an identifier is invalid");
      assert.throws(function () { eval("class foo { method() { [super()] = []; } }"); },         SyntaxError, "Destructured array assignment with a super call throws",                              "Invalid use of the 'super' keyword");
      assert.throws(function () { eval("class foo { method() { var [super.x] = []; } }"); },     SyntaxError, "Destructured var array declaration with a super property reference throws",           "The use of a keyword for an identifier is invalid");
      assert.throws(function () { eval("class foo { method() { let [super.x] = []; } }"); },     SyntaxError, "Destructured let array declaration with a super property reference does not throw",   "The use of a keyword for an identifier is invalid");
      assert.throws(function () { eval("class foo { method() { const [super.x] = []; } }"); },   SyntaxError, "Destructured const array declaration with a super property reference does not throw", "The use of a keyword for an identifier is invalid");
      assert.doesNotThrow(function () { eval("class foo { method() { [super.x] = []; } }"); },      "Destructured var array assignment with super a property reference does not throw");
      assert.doesNotThrow(function () { eval("class foo { method() { [super[\"x\"]] = []; } }"); }, "Destructured array assignment with a super property reference does not throw");

      // for in/of
      // Not yet implemented.
//      assert.doesNotThrow(function () { eval("for (var [a] in [1]) { }"); },    "Destructured var array declaration in a for in statement does not throw");
//      assert.doesNotThrow(function () { eval("for (let [a] in [1]) { }"); },    "Destructured let array declaration in a for in statement does not throw");
//      assert.doesNotThrow(function () { eval("for (const [a] in [1]) { }"); },  "Destructured const array declaration in a for in statement does not throw");
//      assert.doesNotThrow(function () { eval("var a; for ([a] in [1]) { }"); }, "Destructured var array assignment in a for in statement does not throw");
//      assert.doesNotThrow(function () { eval("let a; for ([a] in [1]) { }"); }, "Destructured let array assignment in a for in statement does not throw");
//      assert.doesNotThrow(function () { eval("for (var [a] of [1]) { }"); },    "Destructured var array declaration in a for in statement does not throw");
//      assert.doesNotThrow(function () { eval("for (let [a] of [1]) { }"); },    "Destructured let array declaration in a for in statement does not throw");
//      assert.doesNotThrow(function () { eval("for (const [a] of [1]) { }"); },  "Destructured const array declaration in a for in statement does not throw");
//      assert.doesNotThrow(function () { eval("var a; for ([a] of [1]) { }"); }, "Destructured var array assignment in a for in statement does not throw");
//      assert.doesNotThrow(function () { eval("let a; for ([a] of [1]) { }"); }, "Destructured let array assignment in a for in statement does not throw");

      // Destructured array expressions used in other constructs
      assert.doesNotThrow(function () { eval("var a; `${[a] = [1]}`"); }, "Destructured assignment does not throw inside a string template");

     /* Function and Catch bindings - not yet implemented
      assert.doesNotThrow(function () { eval("try { } catch ([a]) { } finally { }"); }, "x = function() { try { } catch ([...a]) { } finally { } }; // ");
      assert.throws(function () { eval("try { } catch (...a) { } finally { }"); }, SyntaxError, ""); // error
      assert.throws(function () { eval("try { } catch ([a], b) { } finally { }"); }, SyntaxError, ""); // error

      assert.doesNotThrow(function () { eval("function foo([a]) {}"); }, "x = function() { var obj = { method([a]) {} } }; // ");
      assert.doesNotThrow(function () { eval("class { method([a]) {} }"); }, "x = function() { ([a]) => a }; // ");
      assert.throws(function () { eval("[a] => a"); }, SyntaxError, ""); // error
      */

      // OS 597319: Parens before a default value should not throw
      assert.doesNotThrow(function () { eval("[((((vrh190 )))) = 5184] = []"); }, "Destructured array assignment with parens before a default expression should not throw");
    }
  },
  {
    name: "Array destructuring basic functionality",
    body : function () {
      // Single assignments & undefined
      {
        let a1; [a1] = [1];
        let     [a2] = [1];

        assert.areEqual(a1, a2, "Destructured array declaration and asssignment assigning single values match");
        assert.areEqual(a1, 1,  "Destructured array assignment assigns single value correctly");
        assert.areEqual(a2, 1,  "Destructured array declaration assigns single value correctly");
      }
      {
        let a1; [a1] = [];
        let     [a2] = [];

        assert.areEqual(a1, a2,        "Destructured array declaration and asssignment assigning an empty array match");
        assert.areEqual(a1, undefined, "Destructured array assignment assigning an empty array results in undefined");
        assert.areEqual(a2, undefined, "Destructured array declaration assigning an empty array results in undefined");

        let a3; [a3] = [,1];
        let     [a4] = [,1];

        assert.areEqual(a3, a4,        "Destructured array declaration and asssignment assigning an array with missing values match");
        assert.areEqual(a3, undefined, "Destructured array assignment assigning an array with missing values results in undefined");
        assert.areEqual(a4, undefined, "Destructured array declaration assigning an array with missing values in undefined");
      }

      // Multiple assignments
      {
        let a1, b1, c1, d1, e1;
        [a1, b1, c1, d1, e1]     = [1, 2, 3, 4, 5];

        let [a2, b2, c2, d2, e2] = [1, 2, 3, 4, 5];

        assert.areEqual([a1, b1, c1, d1, e1], [a2, b2, c2, d2, e2], "Destructured array assignment and declaration assigning multiple values match");
        assert.areEqual([1, 2, 3, 4, 5],      [a1, b1, c1, d1, e1], "Destructured array assignment assigns multiple values correctly");
        assert.areEqual([1, 2, 3, 4, 5],      [a2, b2, c2, d2, e2], "Destructured array declaration assigns multiple values correctly");
      }
      {
        let a1, b1, c1, d1, e1;
        [a1, b1, c1, d1, e1]     = [1, 2, 3];

        let [a2, b2, c2, d2, e2] = [1, 2, 3];

        assert.areEqual([a1, b1, c1, d1, e1],            [a2, b2, c2, d2, e2], "Destructured array assignment and declaration assigning a smaller array match");
        assert.areEqual([1, 2, 3, undefined, undefined], [a1, b1, c1, d1, e1], "Destructured array assignment assigns a smaller array correctly");
        assert.areEqual([1, 2, 3, undefined, undefined], [a2, b2, c2, d2, e2], "Destructured array declaration assigns a smaller array correctly");
      }
      {
        let a1, b1, c1, d1, e1;
        [a1, b1, c1, d1, e1]     = [1, , 3, , 5];

        let [a2, b2, c2, d2, e2] = [1, , 3, , 5];

        assert.areEqual([a1, b1, c1, d1, e1],            [a2, b2, c2, d2, e2], "Destructured array assignment and declaration assigning an array with some missing values match");
        assert.areEqual([1, undefined, 3, undefined, 5], [a1, b1, c1, d1, e1], "Destructured array assignment assigns an array with some missing values correctly");
        assert.areEqual([1, undefined, 3, undefined, 5], [a2, b2, c2, d2, e2], "Destructured array declaration assigns an array with some missing values correctly");
      }

      // Rest
      {
        let rest1;
        [...rest1]     = [1, 2, 3, 4, 5];

        let [...rest2] = [1, 2, 3, 4, 5];

        assert.areEqual(rest1,           rest2, "Destructured array assignment and declaration using only a rest parameter match");
        assert.areEqual([1, 2, 3, 4, 5], rest1, "Destructured array assignment uses only a rest parameter correctly");
        assert.areEqual([1, 2, 3, 4, 5], rest2, "Destructured array declaration uses only a rest parameter correctly");
      }
      {
        let a1, b1, c1, d1, e1;
        [a1, b1, c1, ...rest1]     = [1, 2, 3, 4, 5];

        let [a2, b2, c2, ...rest2] = [1, 2, 3, 4, 5];

        assert.areEqual([a1, b1, c1, rest1], [a2, b2, c2, rest2], "Destructured array assignment and declaration using a rest parameter match");
        assert.areEqual([1, 2, 3, [4, 5]],   [a1, b1, c1, rest1], "Destructured array assignment uses a rest parameter correctly");
        assert.areEqual([1, 2, 3, [4, 5]],   [a2, b2, c2, rest2], "Destructured array declaration uses a rest parameter correctly");

        let a3, b3, c3, d3, e3;
        [a3, b3, c3, ...rest3]     = [1, 2, 3];

        let [a4, b4, c4, ...rest4] = [1, 2, 3];

        assert.areEqual([a3, b3, c3, rest3], [a4, b4, c4, rest4], "Destructured array assignment and declaration using a rest parameter and a smaller array match");
        assert.areEqual([1, 2, 3, []],       [a3, b3, c3, rest3], "Destructured array assignment uses a rest parameter and a smaller array correctly");
        assert.areEqual([1, 2, 3, []],       [a4, b4, c4, rest4], "Destructured array declaration uses a rest parameter and a smaller array correctly");
      }
      {
        let a1, b1;
        [[...a1], ...b1]     = [[1, 2, 3, 4], 5, 6, 7, 8];

        let [[...a2], ...b2] = [[1, 2, 3, 4], 5, 6, 7, 8];

        assert.areEqual([a1, b1], [a2, b2],                     "Destructured array assignment and declaration with nested rest parameters match");
        assert.areEqual([a1, b1], [[1, 2, 3, 4], [5, 6, 7, 8]], "Destructured array assignment uses nested rest parameters correctly");
        assert.areEqual([a2, b2], [[1, 2, 3, 4], [5, 6, 7, 8]], "Destructured array declaration uses nested rest parameters correctly");
      }

      // Default values
      {
        let a1, b1, c1, d1, e1;
        [a1 = 1, b1 = 2, c1 = 3, d1 = 4, e1 = 5]     = [];

        let [a2 = 1, b2 = 2, c2 = 3, d2 = 4, e2 = 5] = [];

        assert.areEqual([a1, b1, c1, d1, e1], [a2, b2, c2, d2, e2], "Destructured array assignment and declaration with default values assigning an empty array match");
        assert.areEqual([1, 2, 3, 4, 5],      [a1, b1, c1, d1, e1], "Destructured array assignment assigns an array with default values assigning an empty array correctly");
        assert.areEqual([1, 2, 3, 4, 5],      [a2, b2, c2, d2, e2], "Destructured array declaration assigns an array with default values assigning an empty array correctly");
      }
      {
        let a1, b1, c1, d1, e1;
        [a1 = 1, b1 = 2, c1 = 3, d1 = 4, e1 = 5]     = [5, , 3, 2, ];

        let [a2 = 1, b2 = 2, c2 = 3, d2 = 4, e2 = 5] = [5, , 3, 2, ];

        assert.areEqual([a1, b1, c1, d1, e1], [a2, b2, c2, d2, e2], "Destructured array assignment and declaration assigning an array with some missing values match");
        assert.areEqual([5, 2, 3, 2, 5],      [a1, b1, c1, d1, e1], "Destructured array assignment assigns an array with some missing values correctly");
        assert.areEqual([5, 2, 3, 2, 5],      [a2, b2, c2, d2, e2], "Destructured array declaration assigns an array with some missing values correctly");
      }

      // Identifier references
      {
        let a    = {};
        [a.x]    = [10];
        assert.areEqual(10,   a.x,        "Destructured array with an object property assignment works correctly");
        [a["x"]] = [20];
        assert.areEqual(20,   a["x"],     "Destructured array with an object index assignment works correctly");

        var obj = { x: 10 };
        function foo() { return obj };

        [foo().x] = [20];
        assert.areEqual(20,   foo().x,    "Destructured array with a call result property assignment works correctly");

        [foo()["x"]] = [30];
        assert.areEqual(30,   foo()["x"], "Destructured array with a call result index assignment works correctly");

        [...foo().x] = [20];
        assert.areEqual([20], foo().x,    "Destructured array with a call result rest element works correctly");

        [...foo()["x"]] = [30];
        assert.areEqual([30], foo()["x"], "Destructured array with a call result rest element works correctly");

        class base {
          methodProp()      { return {}; }
          methodIndex()     { return {}; }
          methodRestProp()  { return {}; }
          methodRestIndex() { return {}; }
          get x()           { return this._x; }
          set x(v)          { this._x = v; }
        };
        class extended extends base {
          methodProp()      { return [super.x] = [10, 20, 30]; }
          methodIndex()     { return [super["x"]] = [40, 50, 60]; }
          methodRestProp()  { return [...super.x] = [10, 20, 30]; }
          methodRestIndex() { return [...super.x] = [40, 50, 60]; }
          getValue()        { return super.x;}
        };

        let c = new extended();
        assert.areEqual(undefined,    c.getValue(), "Super property before destructured assignment is undefined");

        c.methodProp();
        assert.areEqual(10,           c.getValue(), "Super property after destructured super property assignment");

        c.methodIndex();
        assert.areEqual(40,           c.getValue(), "Super property after destructured super index assignment");

        c.methodRestProp();
        assert.areEqual([10, 20, 30], c.getValue(), "Super property after destructured super property assignment");

        c.methodRestIndex();
        assert.areEqual([40, 50, 60], c.getValue(), "Super property after destructured super index assignment");
      }

      // Nesting
      {
        let a1; [[a1]] = [[1]];
        let     [[a2]] = [[1]];

        assert.areEqual(a1, a2, "Destructured array declaration and asssignment assigning single values match");
        assert.areEqual(a1, 1,  "Destructured array assignment assigns single value correctly");
        assert.areEqual(a2, 1,  "Destructured array declaration assigns single value correctly");
      }
      {
        let a1, b1, c1, d1, e1;
        [[a1, b1], c1, [d1, [e1]]]     = [[1, 2], 3, [4, [5]]];

        let [[a2, b2], c2, [d2, [e2]]] = [[1, 2], 3, [4, [5]]];

        assert.areEqual([a1, b1, c1, d1, e1], [a2, b2, c2, d2, e2], "Destructured array assignment and declaration assigning multiple values match");
        assert.areEqual([1, 2, 3, 4, 5],      [a1, b1, c1, d1, e1], "Destructured array assignment assigns multiple values correctly");
        assert.areEqual([1, 2, 3, 4, 5],      [a2, b2, c2, d2, e2], "Destructured array declaration assigns multiple values correctly");
      }
      {
        let a1; [[a1, b1] = [1, 2]] = [];
        let     [[a2, b2] = [1, 2]] = [];

        assert.areEqual([a1, b1], [a2, b2], "Destructured array declaration and asssignment using nested values match");
        assert.areEqual([1, 2],   [a1, b1], "Destructured array assignment assigns nested values correctly");
        assert.areEqual([1, 2],   [a2, b2], "Destructured array declaration assigns nested values correctly");
      }
      {
        let a1; [[[a1] = [1], [[b1]] = [[2]]] = [, undefined]] = [undefined, ];
        let     [[[a2] = [1], [[b2]] = [[2]]] = [, undefined]] = [undefined, ];

        assert.areEqual([a1, b1], [a2, b2], "Destructured array declaration and asssignment using nested default values match");
        assert.areEqual([1, 2],   [a1, b1], "Destructured array assignment assigns nested default values correctly");
        assert.areEqual([1, 2],   [a2, b2], "Destructured array declaration assigns nested default values correctly");
      }

      // Other iterators
      {
        let a1; [a1, b1, c1, d1, ...rest1] = "testing";
        let [a2, b2, c2, d2, ...rest2]     = "testing";

        assert.areEqual([a1, b1, c1, d1, rest1],               [a2, b2, c2, d2, rest2], "Destructured array declaration and asssignment using nested values match");
        assert.areEqual(["t", "e", "s", "t", ["i", "n", "g"]], [a1, b1, c1, d1, rest1], "Destructured array assignment assigns nested values correctly");
        assert.areEqual(["t", "e", "s", "t", ["i", "n", "g"]], [a2, b2, c2, d2, rest2], "Destructured array declaration assigns nested values correctly");
      }
      {
        let map = new Map();

        map.set(1, 6);
        map.set(2, 7);
        map.set(3, 8);
        map.set(4, 9);
        map.set(5, 10);

        let a1; [a1, b1, c1, d1, ...rest1] = map.entries();
        let [a2, b2, c2, d2, ...rest2]     = map.entries();

        assert.areEqual([a1, b1, c1, d1, rest1],                     [a2, b2, c2, d2, rest2], "Destructured array declaration and asssignment using nested values match");
        assert.areEqual([[1, 6], [2, 7], [3, 8], [4, 9], [[5, 10]]], [a1, b1, c1, d1, rest1], "Destructured array assignment assigns nested values correctly");
        assert.areEqual([[1, 6], [2, 7], [3, 8], [4, 9], [[5, 10]]], [a2, b2, c2, d2, rest2], "Destructured array declaration assigns nested values correctly");
      }
      {
        let getIteratorCalledCount = 0;
        let nextCalledCount = 0;
        let doneCalledCount = 0;
        let valueCalledCount = 0;

        let simpleIterator = {
            [Symbol.iterator]: function () {
            ++getIteratorCalledCount;
            return {
              i: 0,
              next: function () {
                ++nextCalledCount;
                var that = this;
                return {
                  get done() {
                    ++doneCalledCount;
                    return that.i == 1;
                  },
                  get value() {
                    ++valueCalledCount;
                    return that.i++;
                  }
                };
              }
            };
          }
        };

        let [a, b, c] = simpleIterator;
        assert.areEqual(1, getIteratorCalledCount, "GetIterator is called once per iterator");
        assert.areEqual(3, nextCalledCount,        "'.next()' is called once per destructuring reference");
        assert.areEqual(3, doneCalledCount,        "'done' is read once per destructuring reference");
        assert.areEqual(1, valueCalledCount,       "'value' is read once per .next() call until done is true");

        [getIteratorCalledCount, nextCalledCount, doneCalledCount, valueCalledCount] = [0, 0, 0, 0];

        let [...rest] = simpleIterator;
        assert.areEqual(1, getIteratorCalledCount, "GetIterator is called once per iterator");
        assert.areEqual(2, nextCalledCount,        "'.next()' is called until done is true when filling a rest element");
        assert.areEqual(2, doneCalledCount,        "'done' is read until it is true");
        assert.areEqual(1, valueCalledCount,       "'value' is read once for each .next() call until done is true");

        [getIteratorCalledCount, nextCalledCount, doneCalledCount, valueCalledCount] = [0, 0, 0, 0];

        [a, b, c, ...rest] = simpleIterator;
        assert.areEqual(1, getIteratorCalledCount, "GetIterator is called once per iterator");
        assert.areEqual(4, nextCalledCount,        "'.next()' is called once per destructuring reference");
        assert.areEqual(4, doneCalledCount,        "'done' is read once per destructuring reference");
        assert.areEqual(1, valueCalledCount,       "'value' is read once per .next() call until done is true");
      }

      // Runtime type errors
      assert.throws(function () { eval("let [a] = 1;") },       TypeError, "Non-object used as an iterator in a declaration throws",        "Object doesn't support property or method 'Symbol.iterator'");
      assert.throws(function () { eval("let a; [a] = 1;") },    TypeError, "Non-object used as an iterator an assignment throws"   ,        "Object doesn't support property or method 'Symbol.iterator'");
      assert.throws(function () { eval("let [[a]] = [];") },    TypeError, "Nested non-object used as an iterator in a declaration throws", "Unable to get property 'Symbol.iterator' of undefined or null reference");
      assert.throws(function () { eval("let a; [[a]] = [];") }, TypeError, "Nested non-object used as an iterator an assignment throws",    "Unable to get property 'Symbol.iterator' of undefined or null reference");

      // Array destructuring in various contexts
      {
        let a, b, c;
        function foo(x = [a, b = 2, ...c] = [1,,3,4,5,6,7]) {
          assert.areEqual([1, 2, [3,4,5,6,7]], [a, b, c], "Destructuring array assignment works correctly inside a default parameter expression");
          assert.areEqual([1,,3,4,5,6,7],      x,         "Destructuring array assignment evaluates to RHS in a default parameter expression");
        }
        foo();

        `${[a = 5, b, ...c] = [, 1, 3, 5, 7, 9]}`;
        assert.areEqual([5, 1, [3, 5, 7, 9]], [a, b, c], "Destructuring array assignment inside a string template works correctly");

        (() => [a,, b, c] = [1, 2, 3])();
        assert.areEqual([1, 3, undefined], [a, b, c], "Destructuring array assignment inside a lambda expression works correctly");
      }

          // nested destructuring
          {
        let [[a]=[1]] = [[2]];
                assert.areEqual(a, 2, "Nested destructuring - value is present");

                [[a]=[1]] = [[]];
                assert.areEqual(a, undefined, "Nested destructuring - value is present but undefined");

                [[a]=[1]] = [];
                assert.areEqual(a, 1, "Nested destructuring - value is not present - use defult");

                [[a]=1] = [[]];
                assert.areEqual(a, undefined, "Nested destructuring - value is present - default is incorrect - does not have @@iterator");
          }

       // Bug OSG : 4533495
       {
         function foo() {
            for(var [i, j, k] in { qux: 1 }) {
                return i === "q" && j === "u" && k === "x";
            }
        }
        assert.isTrue(foo(), "Array destructuring pattern on for..in is initialized correctly");
      }

    }
  }
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
