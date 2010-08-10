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


// Test keywords valid as property names in initializers and dot-access.
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
  "native",
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
  "with",
];

function testKeywordProperty(keyword) {
  try {
    // Sanity check that what we get is a keyword.
    eval("var " + keyword + " = 42;");
    assertUnreachable("Not a keyword: " + keyword);
  } catch (e) { }
  
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

// Test getter and setter properties with string/number literal names.

var obj = {get 42() { return 42; },
           get 3.14() { return "PI"; },
           get "PI"() { return 3.14; },
           readback: 0,
           set 37(v) { this.readback = v; },
           set 1.44(v) { this.readback = v; },
           set "Poo"(v) { this.readback = v; }}

assertEquals(42, obj[42]);
assertEquals("PI", obj[3.14]);
assertEquals(3.14, obj["PI"]);
obj[37] = "t1";
assertEquals("t1", obj.readback);
obj[1.44] = "t2";
assertEquals("t2", obj.readback);
obj["Poo"] = "t3";
assertEquals("t3", obj.readback);


