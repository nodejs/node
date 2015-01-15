// Copyright 2010 The Closure Linter Authors. All Rights Reserved.
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
 * @fileoverview Test file for indentation.
 * @author robbyw@google.com (Robert Walker)
 */

goog.provide('goog.editor.SeamlessField');
goog.provide('goog.something');

goog.require('goog.events.KeyCodes');
goog.require('goog.userAgent');

// Some good indentation examples.

var x = 10;
var y = 'some really really really really really really really long string',
    z = 14;
if (x == 10) {
  x = 12;
}
if (x == 10 ||
    x == 12) {
  x = 14;
}
if (x == 14) {
  if (z >= x) {
    y = 'test';
  }
}
x = x +
    10 + (
        14
    );
something =
    5;
var arr = [
  1, 2, 3];
var arr2 = [
  1,
  2,
  3];
var obj = {
  a: 10,
  b: 20
};
callAFunction(10, [100, 200],
    300);
callAFunction([
  100,
  200
],
300);
callAFunction('abc' +
              'def' +
              'ghi');

x.reallyReallyReallyReallyReallyReallyReallyReallyReallyReallyReallyLongName
    .someMember = 10;


// confused on allowed indentation in continued function assignments vs overlong
// wrapped function calls.
some.sample()  // LINE_ENDS_WITH_DOT
    .then(function(response) {
      return 1;
    });


/**
 * Some function.
 * @return {number} The number ten.
 */
goog.something.x = function() {
  return 10 +
         20;
};


/**
 * Some function.
 * @param {number} longParameterName1 Some number.
 * @param {number} longParameterName2 Some number.
 * @param {number} longParameterName3 Some number.
 * @return {number} Sum number.
 */
goog.something.y = function(longParameterName1, longParameterName2,
    longParameterName3) {
  return longParameterName1 + longParameterName2 + longParameterName3;
};


/**
 * Some function.
 * @param {number} longParameterName1 Some number.
 * @param {number} longParameterName2 Some number.
 * @param {number} longParameterName3 Some number.
 * @return {number} Sum number.
 */
goog.something.z = function(longParameterName1, longParameterName2,
                            longParameterName3) {
  return longParameterName1 + longParameterName2 + longParameterName3;
};

if (opt_rootTagName) {
  doc.appendChild(doc.createNode(3,
                                 opt_rootTagName,
                                 opt_namespaceUri || ''));
}


/**
 * For a while this errored because the function call parens were overriding
 * the other opening paren.
 */
goog.something.q = function() {
  goog.something.x(a.getStartNode(),
      a.getStartOffset(), a.getEndNode(), a.getEndOffset());
};

function doSomething() {
  var titleElement = goog.something(x, // UNUSED_LOCAL_VARIABLE
      y);
}

switch (x) {
  case 10:
    y = 100;
    break;

  // This should be allowed.
  case 20:
    if (y) {
      z = 0;
    }
    break;

  // This should be allowed,
  // even with mutliple lines.
  case 30:
    if (y) {
      z = 0;
    }
    break;

  case SadThatYouSwitch
      .onSomethingLikeThis:
    z = 10;

  case 40:
    z = 20;

  default:
    break;
}

// Description of if case.
if (x) {

// Description of else case should be allowed at this indent.
// Multiple lines is ok.
} else {

}


/** @inheritDoc */
goog.editor.SeamlessField.prototype.setupMutationEventHandlersGecko =
    function() {
  var x = 10;
  x++;
};


// Regression test for '.' at the end confusing the indentation checker if it is
// not considered to be part of the identifier.
/** @inheritDoc */
goog.editor.SeamlessField.prototype
    .setupMutationEventHandlersGecko = function() {
  // -2: LINE_ENDS_WITH_DOT
  var x = 10;
  x++;
};

var someReallyReallyLongVariableName =
    y ? /veryVeryVeryVeryVeryVeryVeryVeryLongRegex1/gi :
        /slightlyLessLongRegex2/gi;

var somethingOrOther = z ?
                       a :
                       b;

var z = x ? y :
    'bar';

var z = x ?
    y :
    a;

var z = z ?
    a ? b : c :
    d ? e : f;

var z = z ?
    a ? b :
        c :
    d ? e : f;

var z = z ?
    a ?
        b :
        c :
    d ? e : f;

var z = z ?
    a ? b : c :
    d ? e :
        f ? g : h;

var z = z ?
    a +
        i ?
            b +
                j : c :
    d ? e :
        f ? g : h;


if (x) {
  var block =
      // some comment
      // and some more comment
      (e.keyCode == goog.events.KeyCodes.TAB && !this.dispatchBeforeTab_(e)) ||
      // #2: to block a Firefox-specific bug where Macs try to navigate
      // back a page when you hit command+left arrow or comamnd-right arrow.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=341886
      // get Firefox to fix this.
      (goog.userAgent.GECKO && e.metaKey &&
       (e.keyCode == goog.events.KeyCodes.LEFT ||
        e.keyCode == goog.events.KeyCodes.RIGHT));
}

if (x) {
}

var somethingElse = {
  HAS_W3C_RANGES: goog.userAgent.GECKO || goog.userAgent.WEBKIT ||
      goog.userAgent.OPERA,

  // A reasonably placed comment.
  SOME_KEY: goog.userAgent.IE
};

var x = {
  ySomethingReallyReallyLong:
      'foo',
  z: 'bar'
};

// Some bad indentation.

var a = 10; // WRONG_INDENTATION
var b = 10,
    c = 12; // WRONG_INDENTATION
x = x +
    10; // WRONG_INDENTATION
if (x == 14) {
  x = 15; // WRONG_INDENTATION
  x = 16; // WRONG_INDENTATION
}

var longFunctionName = function(opt_element) {
  return opt_element ?
      new z(q(opt_element)) : 100;
  // -1: WRONG_INDENTATION
};

longFunctionName(a, b, c,
    d, e, f); // WRONG_INDENTATION
longFunctionName(a, b,
    c, // WRONG_INDENTATION
    d); // WRONG_INDENTATION

x = a ? b :
    c; // WRONG_INDENTATION
y = a ?
    b : c; // WRONG_INDENTATION

switch (x) {
  case 10:
    break; // WRONG_INDENTATION
  case 20: // WRONG_INDENTATION
    break;
  default: // WRONG_INDENTATION
    break;
}

while (true) {
  x = 10; // WRONG_INDENTATION
  break; // WRONG_INDENTATION
}

function foo() {
  return entryUrlTemplate
      .replace(
          '${authorResourceId}',
          this.sanitizer_.sanitize(authorResourceId));
}

return [new x(
    10)];
return [
  new x(10)];

return [new x(
    10)]; // WRONG_INDENTATION
return [new x(
    10)]; // WRONG_INDENTATION

return {x: y(
    z)};
return {
  x: y(z)
};

return {x: y(
    z)}; // WRONG_INDENTATION
return {x: y(
    z)}; // WRONG_INDENTATION

return /** @type {Window} */ (x(
    'javascript:"' + encodeURI(loadingMessage) + '"')); // WRONG_INDENTATION

x = {
  y: function() {}
};

x = {
  y: foo,
  z: bar +
      baz // WRONG_INDENTATION
};

x({
  a: b
},
10);

z = function(arr, f, val, opt_obj) {
  x(arr, function(val, index) {
    rval = f.call(opt_obj, rval, val, index, arr);
  });
};

var xyz = [100,
           200,
           300];

var def = [100,
  200]; // WRONG_INDENTATION

var ghi = [100,
  200]; // WRONG_INDENTATION

var abcdefg = ('a' +
               'b');

var x9 = z('7: ' +
    x(x)); // WRONG_INDENTATION

function abc() {
  var z = d('div', // UNUSED_LOCAL_VARIABLE
      {
        a: 'b'
      });
}

abcdefg('p', {x: 10},
        'Para 1');

function bar1() {
  return 3 +
      4; // WRONG_INDENTATION
}

function bar2() {
  return 3 + // WRONG_INDENTATION
      4; // WRONG_INDENTATION
}

function bar3() {
  return 3 + // WRONG_INDENTATION
         4;
}

// Regression test for unfiled bug. Wrongly going into implied block after else
// when there was an explicit block (was an else if) caused false positive
// indentation errors.
if (true) {
} else if (doc.foo(
    doc.getBar(baz))) {
  var x = 3;
}

// Regression tests for function indent + 4.
// (The first example is from the styleguide.)
if (veryLongFunctionNameA(
        veryLongArgumentName) ||
    veryLongFunctionNameB(
    veryLongArgumentName)) {
  veryLongFunctionNameC(veryLongFunctionNameD(
      veryLongFunctioNameE(
          veryLongFunctionNameF)));
}

if (outer(middle(
        inner(first)))) {}
if (outer(middle(
              inner(second)),
        outer_second)) {}
if (nested.outer(
        first)) {}
if (nested.outer(nested.middle(
                     first))) {}
if (nested
    .outer(nested.middle(
        first))) {}
if (nested.outer(first
                     .middle(
                         second),
        third)) {}

// goog.scope should not increase indentation.
goog.scope(function() {
var x = 5;
while (x > 0) {
  --x;
}
});  // goog.scope


goog.scope(function() { // EXTRA_GOOG_SCOPE_USAGE
// +1: UNUSED_LOCAL_VARIABLE
var x = 5; // WRONG_INDENTATION
});  // goog.scope

goog.scope(function() { // EXTRA_GOOG_SCOPE_USAGE
var x = 5; // UNUSED_LOCAL_VARIABLE
});  // goog.scope

goog.scope(function() { // EXTRA_GOOG_SCOPE_USAGE
var x = 5; // UNUSED_LOCAL_VARIABLE
});  // goog.scope
