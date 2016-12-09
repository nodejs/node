// Copyright 2009 the V8 project authors. All rights reserved.
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

var obj = {
    a: 7,
    b: { x: 12, y: 24 },
    c: 'Zebra'
}

assertEquals(7, obj.a);
assertEquals(12, obj.b.x);
assertEquals(24, obj.b.y);
assertEquals('Zebra', obj.c);

var z = 24;

var obj2 = {
    a: 7,
    b: { x: 12, y: z },
    c: 'Zebra'
}

assertEquals(7, obj2.a);
assertEquals(12, obj2.b.x);
assertEquals(24, obj2.b.y);
assertEquals('Zebra', obj2.c);

var arr = [];
for (var i = 0; i < 2; i++) {
  arr[i] = {
      a: 7,
      b: { x: 12, y: 24 },
      c: 'Zebra'
  }
}

arr[0].b.x = 2;
assertEquals(2, arr[0].b.x);
assertEquals(12, arr[1].b.x);


function makeSparseArray() {
  return {
    '0': { x: 12, y: 24 },
    '1000000': { x: 0, y: 0 }
  };
}

var sa1 = makeSparseArray();
sa1[0].x = 0;
var sa2 = makeSparseArray();
assertEquals(12, sa2[0].x);

// Test that non-constant literals work.
var n = new Object();

function makeNonConstantArray() { return [ [ n ] ]; }

var a = makeNonConstantArray();
a[0][0].foo = "bar";
assertEquals("bar", n.foo);

function makeNonConstantObject() { return { a: { b: n } }; }

a = makeNonConstantObject();
a.a.b.bar = "foo";
assertEquals("foo", n.bar);

// Test that exceptions for regexps still hold.
function makeRegexpInArray() { return [ [ /a*/, {} ] ]; }

a = makeRegexpInArray();
var b = makeRegexpInArray();
assertFalse(a[0][0] === b[0][0]);
assertFalse(a[0][1] === b[0][1]);

function makeRegexpInObject() { return { a: { b: /b*/, c: {} } }; }
a = makeRegexpInObject();
b = makeRegexpInObject();
assertFalse(a.a.b === b.a.b);
assertFalse(a.a.c === b.a.c);


// Test keywords are valid as property names in initializers and dot-access.
var keywords = [
  "break",
  "case",
  "catch",
  "const",
  "continue",
  "debugger",
  "default",
  "delete",
  "do",
  "else",
  "false",
  "finally",
  "for",
  "function",
  "if",
  "in",
  "instanceof",
  "new",
  "null",
  "return",
  "switch",
  "this",
  "throw",
  "true",
  "try",
  "typeof",
  "var",
  "void",
  "while",
  "with"
];

function testKeywordProperty(keyword) {
  var exception = false;
  try {
    // Sanity check that what we get is a keyword.
    eval("var " + keyword + " = 42;");
  } catch (e) {
    exception = true;
  }
  assertTrue(exception);

  // Simple property, read and write.
  var x = eval("({" + keyword + ": 42})");
  assertEquals(42, x[keyword]);
  assertEquals(42, eval("x." + keyword));
  eval("x." + keyword + " = 37");
  assertEquals(37, x[keyword]);
  assertEquals(37, eval("x." + keyword));

  // Getter/setter property, read and write.
  var y = eval("({value : 42, get " + keyword + "(){return this.value}," +
               " set " + keyword + "(v) { this.value = v; }})");
  assertEquals(42, y[keyword]);
  assertEquals(42, eval("y." + keyword));
  eval("y." + keyword + " = 37");
  assertEquals(37, y[keyword]);
  assertEquals(37, eval("y." + keyword));

  // Quoted keyword works is read back by unquoted as well.
  var z = eval("({\"" + keyword + "\": 42})");
  assertEquals(42, z[keyword]);
  assertEquals(42, eval("z." + keyword));

  // Function property, called.
  var was_called;
  function test_call() { this.was_called = true; was_called = true; }
  var w = eval("({" + keyword + ": test_call, was_called: false})");
  eval("w." + keyword + "();");
  assertTrue(was_called);
  assertTrue(w.was_called);

  // Function property, constructed.
  function construct() { this.constructed = true; }
  var v = eval("({" + keyword + ": construct})");
  var vo = eval("new v." + keyword + "()");
  assertTrue(vo instanceof construct);
  assertTrue(vo.constructed);
}

for (var i = 0; i < keywords.length; i++) {
  testKeywordProperty(keywords[i]);
}


(function TestNumericNames() {
  var o = {
    1: 1,
    2.: 2,
    3.0: 3,
    4e0: 4,
    5E0: 5,
    6e-0: 6,
    7E-0: 7,
    0x8: 8,
    0X9: 9,
  }
  assertEquals(['1', '2', '3', '4', '5', '6', '7', '8', '9'], Object.keys(o));

  o = {
    1.2: 1.2,
    1.30: 1.3
  };
  assertEquals(['1.2', '1.3'], Object.keys(o));
})();


function TestNumericNamesGetter(expectedKeys, object) {
  assertEquals(expectedKeys, Object.keys(object));
  expectedKeys.forEach(function(key) {
    var descr = Object.getOwnPropertyDescriptor(object, key);
    assertEquals('get ' + key, descr.get.name);
  });
}
TestNumericNamesGetter(['1', '2', '3', '4', '5', '6', '7', '8', '9'], {
  get 1() {},
  get 2.() {},
  get 3.0() {},
  get 4e0() {},
  get 5E0() {},
  get 6e-0() {},
  get 7E-0() {},
  get 0x8() {},
  get 0X9() {},
});
TestNumericNamesGetter(['1.2', '1.3'], {
  get 1.2() {},
  get 1.30() {}
});


function TestNumericNamesSetter(expectedKeys, object) {
  assertEquals(expectedKeys, Object.keys(object));
  expectedKeys.forEach(function(key) {
    var descr = Object.getOwnPropertyDescriptor(object, key);
    assertEquals('set ' + key, descr.set.name);
  });
}
TestNumericNamesSetter(['1', '2', '3', '4', '5', '6', '7', '8', '9'], {
  set 1(_) {},
  set 2.(_) {},
  set 3.0(_) {},
  set 4e0(_) {},
  set 5E0(_) {},
  set 6e-0(_) {},
  set 7E-0(_) {},
  set 0x8(_) {},
  set 0X9(_) {},
});
TestNumericNamesSetter(['1.2', '1.3'], {
  set 1.2(_) {; },
  set 1.30(_) {; }
});
