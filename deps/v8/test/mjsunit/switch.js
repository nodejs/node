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

// Flags: --allow-natives-syntax

function f0() {
  switch (0) {
    // switch deliberately left empty
  }
}

f0();  // no errors

function f1(x) {
  switch (x) {
    default:      return "f1";
  }
  return "foo";
}

assertEquals("f1", f1(0), "default-switch.0");
assertEquals("f1", f1(1), "default-switch.1");

function f2(x) {
  var r;
  switch (x) {
    case 0:
      r = "zero";
      break;
    case 1:
      r = "one";
      break;
    case 2:
      r = "two";
      break;
    case 3:
      r = "three";
      break;
    default:
      r = "default";
  }
  return r;
}

assertEquals("zero", f2(0), "0-1-switch.0");
assertEquals("one", f2(1), "0-1-switch.1");
assertEquals("default", f2(7), "0-1-switch.2");
assertEquals("default", f2(-1), "0-1-switch.-1");
assertEquals("default", f2(NaN), "0-1-switch.NaN");
assertEquals("default", f2(Math.pow(2,34)), "0-1-switch.largeNum");
assertEquals("default", f2("0"), "0-1-switch.string");
assertEquals("default", f2(false), "0-1-switch.bool");
assertEquals("default", f2(null), "0-1-switch.null");
assertEquals("default", f2(undefined), "0-1-switch.undef");
assertEquals("default", f2(new Number(2)), "0-1-switch.undef");
assertEquals("default", f2({valueOf: function(){return 2; }}), "0-1-switch.obj");


function f3(x, c) {
  var r = 0;
  switch (x) {
    default:
      r = "default";
      break;
    case  c:
      r = "value is c = " + c;
      break;
    case  2:
      r = "two";
      break;
    case -5:
      r = "minus 5";
      break;
    case  9:
      r = "nine";
      break;
  }
  return r;
}

assertEquals("two", f3(2,0), "value-switch.2-0");
assertEquals("minus 5", f3(-5,0), "value-switch.-5-0");
assertEquals("nine", f3(9,0), "value-switch.9-0");
assertEquals("value is c = 0", f3(0,0), "value-switch.0-0");
assertEquals("value is c = 2", f3(2,2), "value-switch.2-2");
assertEquals("default", f3(7,0), "value-switch.7-0");


function f4(x) {
  switch(x) {
    case 0:
      x++;
    default:
      x++;
    case 2:
      x++;
  }
  return x;
}


assertEquals(3, f4(0), "fallthrough-switch.0");
assertEquals(3, f4(1), "fallthrough-switch.1");
assertEquals(3, f4(2), "fallthrough-switch.2");
assertEquals(5, f4(3), "fallthrough-switch.3");

function f4_string(tag, x) {
  switch(tag) {
    case 'zero':
      x++;
    case 'two':
      x++;
  }
  return x;
}

// Symbols
assertEquals(2, f4_string('zero', 0), "fallthrough-string-switch.0");
assertEquals(1, f4_string('one', 1), "fallthrough-string-switch.1");
assertEquals(3, f4_string('two', 2), "fallthrough-string-switch.2");

// Strings
assertEquals(2, f4_string('_zero'.slice(1), 0), "fallthrough-string-switch.3");
assertEquals(1, f4_string('_one'.slice(1), 1), "fallthrough-string-switch.4");
assertEquals(3, f4_string('_two'.slice(1), 2), "fallthrough-string-switch.5");

// Oddball
assertEquals(3, f4_string(null, 3), "fallthrough-string-switch.6");

// Test for regression
function regress_string(value) {
  var json = 1;
  switch (typeof value) {
    case 'object':
      break;

    default:

  }
  return json;
};
assertEquals(1, regress_string('object'), 'regression-string');

function f5(x) {
  switch(x) {
     case -2: return true;
     case -1: return false;
     case 0: return true;
     case 2: return false;
     default: return 42;
  }
}

assertTrue(f5(-2), "negcase.-2");
assertFalse(f5(-1), "negcase.-1");
assertTrue(f5(0), "negcase.-0");
assertEquals(42, f5(1), "negcase.1");
assertFalse(f5(2), "negcase.2");

function f6(N) {
  // long enough case that code buffer grows during code-generation
  var res = 0;
  for(var i = 0; i < N; i++) {
    switch(i & 0x3f) {
    case 0: res += 0; break;
    case 1: res += 1; break;
    case 2: res += 2; break;
    case 3: res += 3; break;
    case 4: res += 4; break;
    case 5: res += 5; break;
    case 6: res += 6; break;
    case 7: res += 7; break;
    case 8: res += 8; break;
    case 9: res += 9; break;
    case 10: res += 10; break;
    case 11: res += 11; break;
    case 12: res += 12; break;
    case 13: res += 13; break;
    case 14: res += 14; break;
    case 15: res += 15; break;
    case 16: res += 16; break;
    case 17: res += 17; break;
    case 18: res += 18; break;
    case 19: res += 19; break;
    case 20: res += 20; break;
    case 21: res += 21; break;
    case 22: res += 22; break;
    case 23: res += 23; break;
    case 24: res += 24; break;
    case 25: res += 25; break;
    case 26: res += 26; break;
    case 27: res += 27; break;
    case 28: res += 28; break;
    case 29: res += 29; break;
    case 30: res += 30; break;
    case 31: res += 31; break;
    case 32: res += 32; break;
    case 33: res += 33; break;
    case 34: res += 34; break;
    case 35: res += 35; break;
    case 36: res += 36; break;
    case 37: res += 37; break;
    case 38: res += 38; break;
    case 39: res += 39; break;
    case 40: res += 40; break;
    case 41: res += 41; break;
    case 42: res += 42; break;
    case 43: res += 43; break;
    case 44: res += 44; break;
    case 45: res += 45; break;
    case 46: res += 46; break;
    case 47: res += 47; break;
    case 48: res += 48; break;
    case 49: res += 49; break;
    case 50: res += 50; break;
    case 51: res += 51; break;
    case 52: res += 52; break;
    case 53: res += 53; break;
    case 54: res += 54; break;
    case 55: res += 55; break;
    case 56: res += 56; break;
    case 57: res += 57; break;
    case 58: res += 58; break;
    case 59: res += 59; break;
    case 60: res += 60; break;
    case 61: res += 61; break;
    case 62: res += 62; break;
    case 63: res += 63; break;
    case 64: break;
    default: break;
    }
  }
  return res;
}

assertEquals(190, f6(20), "largeSwitch.20");
assertEquals(2016, f6(64), "largeSwitch.64");
assertEquals(4032, f6(128), "largeSwitch.128");
assertEquals(4222, f6(148), "largeSwitch.148");

function fhole(x) {
  switch(x){
    case 0:
      x = 2;
    case 2:
      x = 3;
    case 3:
      x = 4;
    case 4:
      x = 5;
      break;
    case 5:
      x = 6;
      break;
  }
  return x;
}

assertEquals(1, fhole(1), "fhole.jumptablehole");

function f7(value) {
  switch (value) {
  case 0: return "0";
  case -0: return "-0";
  case 1: case 2: case 3: case 4:  // Dummy fillers.
  }
  switch (value) {
  case 0x3fffffff: return "MaxSmi";
  case 0x3ffffffe:
  case 0x3ffffffd:
  case 0x3ffffffc:
  case 0x3ffffffb:
  case 0x3ffffffa:  // Dummy fillers
  }
  switch (value) {
  case -0x40000000: return "MinSmi";
  case -0x3fffffff:
  case -0x3ffffffe:
  case -0x3ffffffd:
  case -0x3ffffffc:
  case -0x3ffffffb:  // Dummy fillers
  }
  switch (value) {
  case 10: return "A";
  case 11:
  case 12:
  case 13:
  case 14:
  case 15:  // Dummy fillers
  }
  return "default";
}


assertEquals("default", f7(0.1), "0-1-switch.double-0.1");
assertEquals("0", f7(-0), "0-1-switch.double-neg0");
assertEquals("MaxSmi", f7((1<<30)-1), "0-1-switch.maxsmi");
assertEquals("MinSmi", f7(-(1<<30)), "0-1-switch.minsmi");
assertEquals("default", f7(1<<30), "0-1-switch.maxsmi++");
assertEquals("default", f7(-(1<<30)-1), "0-1-switch.minsmi--");
assertEquals("A", f7((170/16)-(170%16/16)), "0-1-switch.heapnum");

function zeroCheck1(value){
  switch(value){
    case -0:
      return 313;
    case -5:
      return 5;
    case -4:
      return 4;
    case -3:
      return 3;
    case -2:
      return 1;
    case -1:
    case 0:
      return 291949;
  }
  return 0;
}

assertEquals(313, zeroCheck1(0), "zero-check-1.1");
assertEquals(313, zeroCheck1(0.0), "zero-check-1.2");
assertEquals(313, zeroCheck1(-0), "zero-check-1.3");
assertEquals(291949, zeroCheck1(-1), "zero-check-1.4");

function zeroCheck2(value){
  switch(value){
    case 0:
      return 291;
    case -5:
      return 5;
    case -4:
    case -0:
      return 313;
    case -3:
      return 3;
    case -2:
      return 1;
    case -1:
      return 10;
  }
  return 0;
}

assertEquals(291, zeroCheck2(0), "zero-check-2.1");
assertEquals(291, zeroCheck2(0.0), "zero-check-2.2");
assertEquals(291, zeroCheck2(-0), "zero-check-2.3");
assertEquals(313, zeroCheck2(-4), "zero-check-2.4");

function duplicateCaseCheck(value){
  switch(value){
    case 1:
      return 291;
    case 2.0:
      return 5;
    case 3:
    case 1.0:
      return 324;
    case 4:
    case 2:
      return 15;
    case 5:
    case 6:
      return 18;
  }
  return 0;
}

assertEquals(291, duplicateCaseCheck(1.0), "duplicate-check.1");
assertEquals(324, duplicateCaseCheck(3), "duplicate-check.2");
assertEquals(15, duplicateCaseCheck(4), "duplicate-check.3");
assertEquals(5, duplicateCaseCheck(2), "duplicate-check.4");

function jumpTableHoleAliasCheck(value){
  let y = 4;
  switch(value){
    case 0: return 10;
    case 1: return 20;
    case 2: return 30;
    case 3: return 40;
    case 5: return 60;
    case 6: return 70;
    case y: return 50;
  }
  return 0;
}

assertEquals(50, jumpTableHoleAliasCheck(4), "jump-table-hole-alias.1");

function caseSideEffects(value){
  let y = 2;
  switch(value){
    case y--:
      return 'first!';
    case 3:
      return 'const1';
    case 4:
      return 'const2';
    case 5:
      return 'const3';
    case 6:
      return 'const4';
    case 7:
      return 'const5';
    case 8:
      return 'const6';
    case y:
      return 'wow';
    case 1:
      return 'ouch';
  }
  return '';
}

assertEquals('wow', caseSideEffects(1), "case-side-effects.1");

function makeVeryLong(length) {
  var res = "(function () {\n" +
            "  var res = 0;\n" +
            "  for (var i = 0; i <= " + length + "; i++) {\n" +
            "    switch(i) {\n";
  for (var i = 0; i < length; i++) {
    res += "    case " + i + ": res += 2; break;\n";
  }
  res += "    default: res += 1;\n" +
         "    }\n" +
         "  }\n" +
         "  return res;\n" +
         "})";
  return eval(res);
}
var verylong_size = 1000;
var verylong = makeVeryLong(verylong_size);

assertEquals(verylong_size * 2 + 1, verylong());

//
// Test suite below aims to cover all possible combinations of following:
//
//  clauses  |   tags   |   type feedback   |  optimization
// =========================================================
//  strings  |  symbol  |     all           |      on
//  smis     |  string  |     target        |      off
//  mixed    |  oddball |     non-target    |
//           |  smis    |     none          |
//           |  heapnum |                   |
// =========================================================

// Function-with-switch generator
var test_id = 0,
    clause_values = {
      string: ['abc', 'def', 'ghi', 'jkl'],
      smi: [1, 2, 3, 4],
      mixed: ['abc', 1, 'def', 2, 'ghi', 3, 'jkl', 4]
    };

function switch_gen(clause_type, feedback, optimize) {
  var values = clause_values[clause_type];

  function opt(fn) {
    if (optimize) %PrepareFunctionForOptimization(fn);
    if (feedback === 'all') {
      values.forEach(fn);
    } else if (Array.isArray(feedback)) {
      // Non-target
      values.filter(function(v) {
        return feedback.indexOf(v) === -1;
      }).forEach(fn);
    } else if (feedback !== undefined) {
      // Target
      fn(feedback);
    } else {
      // None
    }

    if (optimize) %OptimizeFunctionOnNextCall(fn);

    return fn;
  };

  return opt(new Function(
      'tag',
      '"' + (test_id++) + '";' +
      'switch(tag) {' +
      values.map(function(value) {
        return 'case ' + JSON.stringify(value) + ': return' +
               JSON.stringify('ok-' + value);
      }).join(';') +
      '}'
  ));
};

function test_switch(clause_type, test_type, feedback, optimize) {
  var pairs = [],
      fn = switch_gen(clause_type, feedback, optimize);

  if (Array.isArray(test_type)) {
    pairs = test_type.map(function(v) {
      return {
        value: v,
        expected: 'ok-' + v
      };
    });
  } else if (test_type === 'symbols') {
    pairs = clause_values.string.map(function(v) {
      return {
        value: v,
        expected: clause_type !== 'smi' ? 'ok-' + v : undefined
      };
    });
  } else if (test_type === 'strings') {
    pairs = clause_values.string.map(function(v) {
      return {
        value: ('%%' + v).slice(2),
        expected: clause_type !== 'smi' ? 'ok-' + v : undefined
      };
    });
  } else if (test_type === 'oddball') {
    pairs = [
      { value: null, expected: undefined },
      { value: NaN, expected: undefined },
      { value: undefined, expected: undefined }
    ];
  } else if (test_type === 'smi') {
    pairs = clause_values.smi.map(function(v) {
      return {
        value: v,
        expected: clause_type !== 'string' ? 'ok-' + v : undefined
      };
    });
  } else if (test_type === 'heapnum') {
    pairs = clause_values.smi.map(function(v) {
      return {
        value: ((v * 17)/16) - ((v*17)%16/16),
        expected: clause_type !== 'string' ? 'ok-' + v : undefined
      };
    });
  }

  pairs.forEach(function(pair) {
    assertEquals(fn(pair.value), pair.expected);
  });
};

// test_switch(clause_type, test_type, feedback, optimize);

function test_switches(opt) {
  var test_types = ['symbols', 'strings', 'oddball', 'smi', 'heapnum'];

  function test(clause_type) {
    var values = clause_values[clause_type];

    test_types.forEach(function(test_type) {
      test_switch(clause_type, test_type, 'all', opt);
      test_switch(clause_type, test_type, 'none', opt);

      // Targeting specific clause feedback
      values.forEach(function(value) {
        test_switch(clause_type, test_type, [value], value, opt);
        test_switch(clause_type, test_type, value, value, opt);
      });
    });
  };

  test('string');
  test('smi');
  test('mixed');
};

test_switches(false);
test_switches(true);


// Test labeled and anonymous breaks in switch statements
(function test_switch_break() {
  A: for (var i = 1; i < 10; i++) {
    switch (i) {
      case 1:
        break A;
    }
  }
  assertEquals(1, i);

  for (var i = 1; i < 10; i++) {
    B: switch (i) {
      case 1:
        break B;
    }
  }
  assertEquals(10, i);

  for (var i = 1; i < 10; i++) {
    switch (i) {
      case 1:
        break;
    }
  }
  assertEquals(10, i);

  switch (1) {
    case 1:
      C: for (var i = 1; i < 10; i++) {
        break C;
      }
      i = 2;
  }
  assertEquals(2, i);

  switch (1) {
    case 1:
      for (var i = 1; i < 10; i++) {
        break;
      }
      i = 2;
  }
  assertEquals(2, i);

  D: switch (1) {
    case 1:
      for (var i = 1; i < 10; i++) {
        break D;
      }
      i = 2;
  }
  assertEquals(1, i);
})();

assertThrows(function() {
  function f(){}
  // The f()-- unconditionally throws a ReferenceError.
  switch(f()--) {
    // This label is dead.
    default:
      break;
  }
}, ReferenceError);
