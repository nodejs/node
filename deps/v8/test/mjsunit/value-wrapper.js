// Copyright 2008 the V8 project authors. All rights reserved.
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

// When calling user-defined functions on strings, booleans or
// numbers, we should create a wrapper object.

// When running the tests use loops to ensure that the call site moves through
// the different IC states and that both the runtime system and the generated
// IC code is tested.
function RunTests() {
  for (var i = 0; i < 10; i++) {
    assertEquals('object', 'xxx'.TypeOfThis());
    assertEquals('object', true.TypeOfThis(2,3));
    assertEquals('object', false.TypeOfThis());
    assertEquals('object', (42).TypeOfThis());
    assertEquals('object', (3.14).TypeOfThis());
  }
  
  for (var i = 0; i < 10; i++) {
    assertEquals('object', 'xxx'['TypeOfThis']());
    assertEquals('object', true['TypeOfThis']());
    assertEquals('object', false['TypeOfThis']());
    assertEquals('object', (42)['TypeOfThis']());
    assertEquals('object', (3.14)['TypeOfThis']());
  }
  
  function CallTypeOfThis(obj) {
    assertEquals('object', obj.TypeOfThis());
  }
  
  for (var i = 0; i < 10; i++) {
    CallTypeOfThis('xxx');
    CallTypeOfThis(true);
    CallTypeOfThis(false);
    CallTypeOfThis(42);
    CallTypeOfThis(3.14);
  }
  
  function TestWithWith(obj) {
    with (obj) {
      for (var i = 0; i < 10; i++) {
        assertEquals('object', TypeOfThis());
      }
    }
  }
  
  TestWithWith('xxx');
  TestWithWith(true);
  TestWithWith(false);
  TestWithWith(42);
  TestWithWith(3.14);
  
  for (var i = 0; i < 10; i++) {
    assertEquals('object', true[7]());
    assertEquals('object', false[7]());
    assertEquals('object', (42)[7]());
    assertEquals('object', (3.14)[7]());
  }

  for (var i = 0; i < 10; i++) {
    assertEquals('object', typeof 'xxx'.ObjectValueOf());
    assertEquals('object', typeof true.ObjectValueOf());
    assertEquals('object', typeof false.ObjectValueOf());
    assertEquals('object', typeof (42).ObjectValueOf());
    assertEquals('object', typeof (3.14).ObjectValueOf());
  }

  for (var i = 0; i < 10; i++) {
    assertEquals('[object String]', 'xxx'.ObjectToString());
    assertEquals('[object Boolean]', true.ObjectToString());
    assertEquals('[object Boolean]', false.ObjectToString());
    assertEquals('[object Number]', (42).ObjectToString());
    assertEquals('[object Number]', (3.14).ObjectToString());
  }
}

function TypeOfThis() { return typeof this; }

// Test with normal setup of prototype. 
String.prototype.TypeOfThis = TypeOfThis;
Boolean.prototype.TypeOfThis = TypeOfThis;
Number.prototype.TypeOfThis = TypeOfThis;
Boolean.prototype[7] = TypeOfThis;
Number.prototype[7] = TypeOfThis;

String.prototype.ObjectValueOf = Object.prototype.valueOf;
Boolean.prototype.ObjectValueOf = Object.prototype.valueOf;
Number.prototype.ObjectValueOf = Object.prototype.valueOf;

String.prototype.ObjectToString = Object.prototype.toString;
Boolean.prototype.ObjectToString = Object.prototype.toString;
Number.prototype.ObjectToString = Object.prototype.toString;

RunTests();

// Run test after properties have been set to a different value.
String.prototype.TypeOfThis = 'x';
Boolean.prototype.TypeOfThis = 'x';
Number.prototype.TypeOfThis = 'x';
Boolean.prototype[7] = 'x';
Number.prototype[7] = 'x';

String.prototype.TypeOfThis = TypeOfThis;
Boolean.prototype.TypeOfThis = TypeOfThis;
Number.prototype.TypeOfThis = TypeOfThis;
Boolean.prototype[7] = TypeOfThis;
Number.prototype[7] = TypeOfThis;

RunTests();

// Force the prototype into slow case and run the test again.
delete String.prototype.TypeOfThis;
delete Boolean.prototype.TypeOfThis;
delete Number.prototype.TypeOfThis;
Boolean.prototype[7];
Number.prototype[7];

String.prototype.TypeOfThis = TypeOfThis;
Boolean.prototype.TypeOfThis = TypeOfThis;
Number.prototype.TypeOfThis = TypeOfThis;
Boolean.prototype[7] = TypeOfThis;
Number.prototype[7] = TypeOfThis;

RunTests();

// According to ES3 15.3.4.3 the this value passed to Function.prototyle.apply
// should wrapped. According to ES5 it should not.
assertEquals('object', TypeOfThis.apply('xxx', []));
assertEquals('object', TypeOfThis.apply(true, []));
assertEquals('object', TypeOfThis.apply(false, []));
assertEquals('object', TypeOfThis.apply(42, []));
assertEquals('object', TypeOfThis.apply(3.14, []));

// According to ES3 15.3.4.3 the this value passed to Function.prototyle.call
// should wrapped. According to ES5 it should not.
assertEquals('object', TypeOfThis.call('xxx'));
assertEquals('object', TypeOfThis.call(true));
assertEquals('object', TypeOfThis.call(false));
assertEquals('object', TypeOfThis.call(42));
assertEquals('object', TypeOfThis.call(3.14));
