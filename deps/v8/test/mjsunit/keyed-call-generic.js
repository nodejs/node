// Copyright 2010 the V8 project authors. All rights reserved.
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
// 'AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// A test for keyed call ICs with a mix of smi and string keys.

function testOne(receiver, key, result) {
  for(var i = 0; i != 10; i++ ) {
    assertEquals(result, receiver[key]());
  }
}

function testMany(receiver, keys, results) {
  for (var i = 0; i != 10; i++) {
    for (var k = 0; k != keys.length; k++) {
      assertEquals(results[k], receiver[keys[k]]());
    }
  }
}

var toStringNonSymbol = 'to';
toStringNonSymbol += 'String';

function TypeOfThis() { return typeof this; }

Number.prototype.square = function() { return this * this; }
Number.prototype.power4 = function() { return this.square().square(); }

Number.prototype.type = TypeOfThis;
String.prototype.type = TypeOfThis;
Boolean.prototype.type = TypeOfThis;

// Use a non-symbol key to force inline cache to generic case.
testOne(0, toStringNonSymbol, '0');

testOne(1, 'toString', '1');
testOne('1', 'toString', '1');
testOne(1.0, 'toString', '1');

testOne(1, 'type', 'object');
testOne(2.3, 'type', 'object');
testOne('x', 'type', 'object');
testOne(true, 'type', 'object');
testOne(false, 'type', 'object');

testOne(2, 'square', 4);
testOne(2, 'power4', 16);

function zero  () { return 0; }
function one   () { return 1; }
function two   () { return 2; }

var fixed_array = [zero, one, two];

var dict_array = [ zero, one, two ];
dict_array[100000] = 1;

var fast_prop = { zero: zero, one: one, two: two };

var normal_prop = { zero: zero, one: one, two: two };
normal_prop.x = 0;
delete normal_prop.x;

var first3num = [0, 1, 2];
var first3str = ['zero', 'one', 'two'];

// Use a non-symbol key to force inline cache to generic case.
testMany('123', [toStringNonSymbol, 'charAt', 'charCodeAt'], ['123', '1', 49]);

testMany(fixed_array, first3num, first3num);
testMany(dict_array, first3num, first3num);
testMany(fast_prop, first3str, first3num);
testMany(normal_prop, first3str, first3num);


function testException(receiver, keys, exceptions) {
  for (var i = 0; i != 10; i++) {
    for (var k = 0; k != keys.length; k++) {
      var thrown = false;
      try {
        var result = receiver[keys[k]]();
      } catch (e) {
        thrown = true;
      }
      assertEquals(exceptions[k], thrown);
    }
  }
}

testException([zero, one, /* hole */ ], [0, 1, 2], [false, false, true]);
