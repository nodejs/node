// Copyright 2007 The Closure Linter Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @fileoverview Miscellaneous style errors.
 *
 * @author robbyw@google.com (Robby Walker)
 */

goog.provide('goog.dom');

goog.require('goog.events.EventHandler');

var this_is_a_really_long_line = 100000000000000000000000000000000000000000000000; // LINE_TOO_LONG

// Declaration in multiple lines.
// Regression test for b/3009648
var
    a,
    b = 10;

// http://this.comment.should.be.allowed/because/it/is/a/URL/that/can't/be/broken/up


/**
 * Types are allowed to be long even though they contain spaces.
 * @type {function(ReallyReallyReallyReallyLongType, AnotherExtremelyLongType) : LongReturnType}
 */
x.z = 1000;


/**
 * Params are also allowed to be long even though they contain spaces.
 * @param {function(ReallyReallyReallyReallyLongType, AnotherExtremelyLongType) : LongReturnType} fn
 *     The function to call.
 */
x.z = function(fn) {
};


/**
 * Visibility tags are allowed to have type, therefore they allowed to be long.
 * @private {function(ReallyReallyReallyReallyLongType, AnotherExtremelyLongType) : LongReturnType}
 */
x.z_ = 1000;


/**
 * Visibility tags are allowed to have type, therefore they allowed to be long.
 * @public {function(ReallyReallyReallyReallyLongType, AnotherExtremelyLongType) : LongReturnType}
 */
x.z = 1000;


/**
 * Visibility tags are allowed to have type, therefore they allowed to be long.
 * @protected {function(ReallyReallyReallyReallyLongType, AnotherExtremelyLongType) : LongReturnType}
 */
x.z = 1000;


/**
 * Visibility tags are allowed to have type, therefore they allowed to be long.
 * @package {function(ReallyReallyReallyReallyLongType,AnotherExtremelyLongType):LongReturnType}
 */
x.z = 1000;

// +2: LINE_TOO_LONG
var x =
    a.b.c.d.a.b.c.d.a.b.c.d.a.b.c.d.a.b.c.d.a.b.c.d.a.b.c.d.a.b.c.d.a.b.c.d.a.b.c.d.a.b.c.d.tooLongEvenThoughNoSpaces;

// +1: LINE_TOO_LONG
getSomeExtremelyLongNamedFunctionWowThisNameIsSoLongItIsAlmostUnbelievable().dispose();


/**
 * @param {number|string|Object|Element|Array.<Object>|null} aReallyReallyReallyStrangeParameter
 * @param {number|string|Object|Element|goog.a.really.really.really.really.really.really.really.really.long.Type|null} shouldThisParameterWrap
 * @return {goog.a.really.really.really.really.really.really.really.really.long.Type}
 */
x.y = function(aReallyReallyReallyStrangeParameter, shouldThisParameterWrap) {
  return something;
};


/**
 * @type {goog.a.really.really.really.really.really.really.really.really.long.Type?}
 */
x.y = null;

function doesEndWithSemicolon() {
}; // ILLEGAL_SEMICOLON_AFTER_FUNCTION

function doesNotEndWithSemicolon() {
}

doesEndWithSemicolon = function() {
  // +1: UNUSED_LOCAL_VARIABLE
  var shouldEndWithSemicolon = function() {
  } // MISSING_SEMICOLON_AFTER_FUNCTION
};

doesNotEndWithSemicolon = function() {
} // MISSING_SEMICOLON_AFTER_FUNCTION

doesEndWithSemicolon['100'] = function() {
};

doesNotEndWithSemicolon['100'] = function() {
} // MISSING_SEMICOLON_AFTER_FUNCTION

if (some_flag) {
  function doesEndWithSemicolon() {
  }; // ILLEGAL_SEMICOLON_AFTER_FUNCTION

  function doesNotEndWithSemicolon() {
  }

  doesEndWithSemicolon = function() {
  };

  doesNotEndWithSemicolon = function() {
  } // MISSING_SEMICOLON_AFTER_FUNCTION
}

// No semicolon for expressions that are immediately called.
var immediatelyCalledFunctionReturnValue = function() {
}();


/**
 * Regression test for function expressions treating semicolons wrong.
 * @bug 1044052
 */
goog.now = Date.now || function() {
  //...
};


/**
 * Regression test for function expressions treating semicolons wrong.
 * @bug 1044052
 */
goog.now = Date.now || function() {
  //...
} // MISSING_SEMICOLON_AFTER_FUNCTION


/**
 * Function defined in ternary operator
 * @bug 1413743
 * @param {string} id The ID of the element.
 * @return {Element} The matching element.
 */
goog.dom.$ = document.getElementById ?
    function(id) {
      return document.getElementById(id);
    } :
    function(id) {
      return document.all[id];
    };


/**
 * Test function in object literal needs no semicolon.
 * @type {Object}
 */
x.y = {
  /**
   * @return {number} Doc the inner function too.
   */
  a: function() {
    return 10;
  }
};

// Semicolon required at end of object literal.
var throwObjectLiteral = function() {
  throw {
    x: 0,
    y: 1
  } // MISSING_SEMICOLON
};

var testRegex = /(\([^\)]*\))|(\[[^\]]*\])|({[^}]*})|(&lt;[^&]*&gt;)/g;
var testRegex2 = /abc/gimsx;

var x = 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100
    + 20; // LINE_STARTS_WITH_OPERATOR

var x = 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100 + 100 +
    -20; // unary minus is ok

var x = z++
    + 20; // LINE_STARTS_WITH_OPERATOR

var x = z. // LINE_ENDS_WITH_DOT
    y();

// Regression test: This line was incorrectly not reporting an error
var marginHeight = x.layout.getSpacing_(elem, 'marginTop')
    + x.layout.getSpacing_(elem, 'marginBottom');
// -1: LINE_STARTS_WITH_OPERATOR

// Regression test: This line was correctly reporting an error
x.layout.setHeight(elem, totalHeight - paddingHeight - borderHeight
    - marginHeight); // LINE_STARTS_WITH_OPERATOR

// Regression test: This line was incorrectly reporting spacing and binary
// operator errors
if (i == index) {
}
++i;

var twoSemicolons = 10;; // REDUNDANT_SEMICOLON

if (i == index) {
} else; // REDUNDANT_SEMICOLON
i++;

do; // REDUNDANT_SEMICOLON
{
} while (i == index);

twoSemicolons = 10;
// A more interesting example of two semicolons
    ; // EXTRA_SPACE, WRONG_INDENTATION, REDUNDANT_SEMICOLON


/** @bug 1598895 */
for (;;) {
  // Do nothing.
}

for (var x = 0, foo = blah(), bar = {};; x = update(x)) {
  // A ridiculous case that should probably never happen, but I suppose is
  // valid.
}

var x = "allow'd double quoted string";
var x = "unnecessary double quotes string"; // UNNECESSARY_DOUBLE_QUOTED_STRING
// +1: MULTI_LINE_STRING, UNNECESSARY_DOUBLE_QUOTED_STRING,
var x = "multi-line unnecessary double quoted \
         string.";


// Regression test: incorrectly reported missing doc for variable used in global
// scope.
/**
 * Whether the "Your browser isn't fully supported..." warning should be shown
 * to the user; defaults to false.
 * @type {boolean}
 * @private
 */
init.browserWarning_ = false;

init.browserWarning_ = true;

if (someCondition) {
  delete this.foo_[bar];
}

// Commas at the end of literals used to be forbidden.
x = [1, 2, 3,];
x = [1, 2, 3, /* A comment */];
x = [
  1,
  2,
  3,
];
x = {
  a: 1,
};

// Make sure we don't screw up typing for Lvalues and think b:c is a type value
// pair.
x = a ? b : c = 34;
x = a ? b:c; // MISSING_SPACE, MISSING_SPACE
x = (a ? b:c = 34); // MISSING_SPACE, MISSING_SPACE

if (x) {
  x += 10;
}; // REDUNDANT_SEMICOLON


/**
 * Bad assignment of array to prototype.
 * @type {Array}
 */
x.prototype.badArray = []; // ILLEGAL_PROTOTYPE_MEMBER_VALUE


/**
 * Bad assignment of object to prototype.
 * @type {Object}
 */
x.prototype.badObject = {}; // ILLEGAL_PROTOTYPE_MEMBER_VALUE


/**
 * Bad assignment of class instance to prototype.
 * @type {goog.events.EventHandler}
 */
x.prototype.badInstance = new goog.events.EventHandler();
// -1: ILLEGAL_PROTOTYPE_MEMBER_VALUE

// Check that some basic structures cause no errors.
x = function() {
  try {
  } finally {
    y = 10;
  }
};

switch (x) {
  case 10:
    break;
  case 20:
    // Fallthrough.
  case 30:
    break;
  case 40: {
    break;
  }
  default:
    break;
}

do {
  x += 10;
} while (x < 100);

do {
  x += 10;
} while (x < 100) // MISSING_SEMICOLON

// Missing semicolon checks.
x = 10 // MISSING_SEMICOLON
x = someOtherVariable // MISSING_SEMICOLON
x = fnCall() // MISSING_SEMICOLON
x = {a: 10, b: 20} // MISSING_SEMICOLON
x = [10, 20, 30] // MISSING_SEMICOLON
x = (1 + 2) // MISSING_SEMICOLON
x = {
  a: [
    10, 20, (30 +
        40)
  ]
} // MISSING_SEMICOLON
x = a
    .b
    .c(). // LINE_ENDS_WITH_DOT
    d;

// Test that blocks without braces don't generate incorrect semicolon and
// indentation errors.  TODO: consider disallowing blocks without braces.
if (x)
  y = 10;

if (x)
  y = 8 // MISSING_SEMICOLON

// Regression test for bug 2973408, bad missing semi-colon error when else
// is not followed by an opening brace.
if (x)
  y = 3;
else
  z = 4;

// We used to erroneously report a missing semicolon error.
if (x)
{
}

while (x)
  y = 10;

for (x = 0; x < 10; x++)
  y += 10;
  z += 10; // WRONG_INDENTATION

var x = 100 // MISSING_SEMICOLON

// Also regression test for bug 2973407 Parse error on nested ternary statments.
foo = bar ? baz ? 1 : 2 : 3 // MISSING_SEMICOLON
foo = bar ? 1 : baz ? 2 : 3;
bar ? 1 : baz ? 2 : bat ? 3 : 4;
bar ? 1 : baz ? bat ? 3 : 4 : baq ? 5 : 6;
foo = bar ? 1 : 2;

foo = {
  str: bar ? baz ? blah ? 1 : 2 : 3 : 4
} // MISSING_SEMICOLON


// Regression tests for bug 2969408 GJsLint doesn't like labeled statements.
mainLoop: while (!y) {
}

myLabel1: myLabel2: var x;

for (var i = 0; i < n; i++) {
  myLabel3:
  while (true) {
    break myLabel3;
  }
}

myLabelA :  myLabelB : x > y ? 0 : 1; // EXTRA_SPACE, EXTRA_SPACE, EXTRA_SPACE

// Regression test for bug 4269466.
var a = new Scheme({default: 0});
switch (foo) {
  default:
    var a = new Scheme({default: 0});
    break;
}


/** @private Some text is allowed after tag */
x.y_ = function() {
};


/** @private Some text is allowed after tag but not the long oneeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee.*/ // LINE_TOO_LONG
x.y_ = function() {
};


/** @private {number} Some text is allowed after tag */
x.z_ = 200;


/** @private {number} Some text is allowed after tag but not the long oneeeeeeeeeeeeeeee. */ // LINE_TOO_LONG
x.z_ = 200;

// Regression tests for b/16298424.
var z = function() {}.bind();
window.alert(function() {}.bind());
function() {
}.bind();
var y = function() {
}.bind();
var y = function() {
        }
        .bind();

/* comment not closed  // FILE_MISSING_NEWLINE, FILE_IN_BLOCK