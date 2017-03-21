// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --validate-asm

var filename = '(?:[^ ]+/)?test/mjsunit/wasm/asm-wasm-exception-in-tonumber.js';
filename = filename.replace(/\//g, '[/\\\\]');

function verifyStack(frames, expected) {
  assertTrue(frames.length >= expected.length, 'too few frames');
  print('frames on detailed stack (' + frames.length + '):');
  frames.forEach((fr, i) => print('[' + i + '] ' + fr));
  expected.forEach(function(exp, i) {
    assertEquals(
        exp[0], frames[i].getFunctionName(), '[' + i + '].getFunctionName()');
    assertEquals(
        exp[1], frames[i].getColumnNumber(), '[' + i + '].getColumnNumber()');
  });
}

function verifyPreformattedStack(e, expected_lines) {
  print('preformatted stack: ' + e.stack);
  var lines = e.stack.split('\n');
  assertTrue(lines.length >= expected_lines.length, 'too few lines');
  for (var i = 0; i < expected_lines.length; ++i) {
    assertMatches(expected_lines[i], lines[i], 'line ' + i);
  }
}

function sym(return_sym) {
  if (return_sym) return Symbol();
  throw Error("user-thrown");
}

function generateAsmJs(stdlib, foreign) {
  'use asm';
  var sym = foreign.sym;
  function callSym(i) {
    i=i|0;
    return sym(i|0) | 0;
  }
  return callSym;
}

function testHelper(use_asm_js, check_detailed, expected, input) {
  if (check_detailed) {
    Error.prepareStackTrace = (error, frames) => frames;
  } else {
    delete Error.prepareStackTrace;
  }

  var fn_code = '(' + generateAsmJs.toString() + ')({}, {sym: sym})';
  if (!use_asm_js) fn_code = fn_code.replace('use asm', '');
  //print('executing:\n' + fn_code);
  var asm_js_fn = eval(fn_code);
  try {
    asm_js_fn(input);
  } catch (e) {
    if (check_detailed) {
      verifyStack(e.stack, expected);
    } else {
      verifyPreformattedStack(e, expected);
    }
  }
}

function testAll(expected_stack, expected_frames, input) {
  for (use_asm_js = 0; use_asm_js <= 1; ++use_asm_js) {
    for (test_detailed = 0; test_detailed <= 1; ++test_detailed) {
      print('\nConfig: asm ' + use_asm_js + '; detailed ' + test_detailed);
      testHelper(
          use_asm_js, test_detailed,
          test_detailed ? expected_frames : expected_stack, input);
    }
  }
}

(function testStackForThrowAtCall() {
  var expected_stack = [
    '^Error: user-thrown$',
    '^ *at sym \\(' + filename + ':\\d+:9\\)$',
    '^ *at callSym \\(eval at testHelper \\(' + filename +
        ':\\d+:19\\), <anonymous>:\\d+:12\\)$',
  ];
  var expected_frames = [
      //  function   pos
      [      "sym",    9],
      [  "callSym",   12],
  ];

  testAll(expected_stack, expected_frames, 0);
})();

(function testStackForThrowAtConversion() {
  var expected_stack = [
    '^TypeError: Cannot convert a Symbol value to a number$',
    '^ *at callSym \\(eval at testHelper \\(' + filename +
        ':\\d+:19\\), <anonymous>:\\d+:21\\)$',
  ];
  var expected_frames = [
      //  function   pos
      [  "callSym",   21],
  ];

  testAll(expected_stack, expected_frames, 1);
})();
