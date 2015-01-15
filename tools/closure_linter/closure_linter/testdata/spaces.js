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
 * @fileoverview Errors relating to whitespace.
 *
 * @author robbyw@google.com (Robby Walker)
 */

if(needs_space) { // MISSING_SPACE
}

if ( too_much_space) { // EXTRA_SPACE
}

if (different_extra_space ) { // EXTRA_SPACE
}

switch(needs_space) { // MISSING_SPACE
}

var x = 'if(not_an_error)';

var y = afjkljkl + ajklasdflj + ajkadfjkasdfklj + aadskfasdjklf + jkasdfa + (
    kasdfkjlasdfjkl / jklasdfjklasdfjkl);

x = 5+ 8; // MISSING_SPACE
x = 5 +8; // MISSING_SPACE
x= 5; // MISSING_SPACE
x =  6; // EXTRA_SPACE
x  = 7; // EXTRA_SPACE
x = 6  + 2; // EXTRA_SPACE
x += 10;

throw Error('Selector not supported yet('+ opt_selector + ')'); // MISSING_SPACE
throw Error('Selector not supported yet(' +opt_selector + ')'); // MISSING_SPACE
throw Error(
    'Selector not supported yet' +
    '(' +(opt_selector ? 'foo' : 'bar') + ')'); // MISSING_SPACE

x++;
x ++; // EXTRA_SPACE
x++ ; // EXTRA_SPACE
y = a + ++b;
for (var i = 0; i < 10; ++i) {
}

// We omit the update section of the for loop to test that a space is allowed
// in this special case.
for (var part; part = parts.shift(); ) {
}

if (x == y) {
}

x = 10;  // no error here
x = -1;
x++;
++x;

x = bool ? -1 : -1;

x = {a: 10};
x = {a:10}; // MISSING_SPACE

x = !!y;

x >>= 0;
x <<= 10;

x[100] = 10;
x[ 100] = 10; // EXTRA_SPACE
x[100 ] = 10; // EXTRA_SPACE
x [100] = 10; // EXTRA_SPACE
x[10]= 5; // MISSING_SPACE
var x = [];
x = [[]];
x = [[x]];
x = [[[x, y]]];
var craziness = ([1, 2, 3])[1];
var crazinessError = ([1, 2, 3]) [1]; // EXTRA_SPACE
var multiArray = x[1][2][3][4];
var multiArrayError = x[1] [2][3][4]; // EXTRA_SPACE

array[aReallyLooooooooooooooooooooooooooooongIndex1][
    anotherVeryLoooooooooooooooooooooooooooooooooooongIndex
] = 10;

if (x) {
  array[aReallyLooooooooooooooooooooooooooooongIndex1][
      anotherVeryLoooooooooooooooooooooooooooooooooooongIndex
  ] = 10;
}


/**
 * Docs.
 * @param {Number} x desc.
 * @return {boolean} Some boolean value.
 */
function functionName( x) { // EXTRA_SPACE
  return !!x;
}


/**
 * Docs.
 * @param {Number} x desc.
 */
function functionName(x ) { // EXTRA_SPACE
  return;
}


/**
 * Docs.
 * @param {Number} x desc.
 * @param {Number} y desc.
 */
function functionName(x,y) { // MISSING_SPACE
}


/**
 * Docs.
 * @param {Number} x desc.
 * @param {Number} y desc.
 */
function functionName(x, y) {
}


/**
 * Docs.
 */
function  functionName() { // EXTRA_SPACE
}


/**
 * Docs.
 */
function functionName(){ // MISSING_SPACE
}

functionName (); // EXTRA_SPACE


/**
 * Docs.
 */
function functionName () { // EXTRA_SPACE
}


/**
 * Docs.
 */
var foo = function () { // EXTRA_SPACE
};



/**
 * Missing a newline.
 * @constructor
 * @extends {a.b.c}
 */
x.y.z = function() {
};goog.inherits(x.y.z, a.b.c); // MISSING_LINE



/**
 * Extra space.
 * @constructor
 * @extends {a.b.c}
 */
x.y.z = function() {
};
   goog.inherits(x.y.z, a.b.c); // WRONG_INDENTATION



/**
 * Extra blank line.
 * @constructor
 * @extends {a.b.c}
 */
x.y.z = function() {
};

goog.inherits(x.y.z, a.b.c); // -1: EXTRA_LINE



/**
 * Perfect!
 * @constructor
 * @extends {a.b.c}
 */
x.y.z = function() {
};
goog.inherits(x.y.z, a.b.c);

if (flag) {
  /**
   * Also ok!
   * @constructor
   * @extends {a.b.c}
   */
  x.y.z = function() {
  };
  goog.inherits(x.y.z, a.b.c);
}


/**
 * Docs.
 */
x.finally = function() {
};

x.finally();
x
    .finally();
x.finally (); // EXTRA_SPACE
x
    .finally (); // EXTRA_SPACE
try {
} finally (e) {
}
try {
} finally(e) { // MISSING_SPACE
}

functionName(x , y); // EXTRA_SPACE
functionName(x,y); // MISSING_SPACE
functionName(x, y);

var really_really_really_really_really_really_really_really_really_long_name =
    2;

var current = arr[cursorRead++];

var x = -(y + z);

// Tab before +
var foo	+ 3; // ILLEGAL_TAB
if (something) {
	var x = 4; // ILLEGAL_TAB
}

// +1: ILLEGAL_TAB
// Tab	<-- in a comment.


// +3: ILLEGAL_TAB
// +3: ILLEGAL_TAB
/**
 * An inline flag with a tab {@code	asdfasd}.
 * @return {string} Illegal	<-- tab in a doc description.
 */
function x() {
  return '';
}


// +2: ILLEGAL_TAB
/**
 * @type	{tabBeforeMe}
 */

// +1: EXTRA_SPACE
var whitespaceAtEndOfLine; 

// +1: EXTRA_SPACE
// Whitespace at end of comment. 


// +4: EXTRA_SPACE
// +4: EXTRA_SPACE
// +4: EXTRA_SPACE
// +4: EXTRA_SPACE
/* 
 * Whitespace at EOL. 
 * @type {string} 
 * @param {string} Description with whitespace at EOL. 
 */
x = 10;


/**
 * @param {?{foo, bar: number}} x This is a valid annotation.
 * @return {{baz}} This is also a valid annotation.
 */
function recordTypeFunction(x) {
  return x;
}

if (y) {
  // Colons are difficult.
  y = x ? 1 : 2;
  y = x ? 1: 2; // MISSING_SPACE

  x = {
    b: 'Good',
    d : 'Space before colon is bad', // EXTRA_SPACE
    f: abc ? def : ghi // These colons should be treated differently
  };

  x = {language:  langCode}; // EXTRA_SPACE
}

// 1094445 - should produce missing space error before +.
// +1: MISSING_SPACE
throw Error('Selector not supported yet ('+ opt_selector + ')');

// This code is ok.
for (i = 0; i < len; ++i) {
}

for (i = 0;i < 10; i++) { // MISSING_SPACE
}
for (i = 0; i < 10;i++) { // MISSING_SPACE
}
for ( i = 0; i < 10; i++) { // EXTRA_SPACE
}
for (i = 0 ; i < 10; i++) { // EXTRA_SPACE
}
for (i = 0; i < 10 ; i++) { // EXTRA_SPACE
}
for (i = 0; i < 10; i++ ) { // EXTRA_SPACE
}
for (i = 0;  i < 10; i++) { // EXTRA_SPACE
}
for (i = 0; i < 10;  i++) { // EXTRA_SPACE
}
for (i = 0 ;i < 10; i++) { // EXTRA_SPACE, MISSING_SPACE
}

// Regression test for bug 3508480, parse error when tab as last token.
// +1: ILLEGAL_TAB, EXTRA_SPACE
	
