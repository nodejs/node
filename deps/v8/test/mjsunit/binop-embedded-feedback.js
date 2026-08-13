// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Each binary op is driven through the interpreter to populate the
// 1-byte embedded feedback operand in the BytecodeArray; %GetFeedback then
// decodes the operand as 'BinaryOp:<hint>'.

function check(op, lhs, rhs, expect) {
  const f = new Function('a', 'b', `return a ${op} b;`);
  for (let i = 0; i < 20; i++) f(lhs, rhs);
  const feedback = %GetFeedback(f);
  if (feedback === undefined) return;
  const entry = feedback.find(e => e[0] === 'BinaryOp');
  assertEquals('BinaryOp', entry[0], `${op}: expected BinaryOp entry`);
  assertTrue(entry[1].startsWith('BinaryOp:'), `${op}: prefix`);
  assertTrue(entry[1].includes(expect),
      `${op}: ${entry[1]} should include ${expect}`);
}

// --- Arithmetic ops ---

// Smi priming yields SignedSmall.
check('+', 1, 2, 'SignedSmall');
check('-', 1, 2, 'SignedSmall');
check('*', 1, 2, 'SignedSmall');
check('/', 4, 2, 'SignedSmall');
check('%', 5, 2, 'SignedSmall');
// `**` on Smi inputs produces a double result, so feedback transitions to
// Number even when both inputs are Smi.
check('**', 2, 3, 'Number');

// HeapNumber priming yields Number.
function checkNumber(op) {
  const f = new Function('a', 'b', `return a ${op} b;`);
  for (let i = 0; i < 20; i++) f(1.5, 2.5);
  const feedback = %GetFeedback(f);
  if (feedback === undefined) return;
  const entry = feedback.find(e => e[0] === 'BinaryOp');
  assertTrue(entry[1].includes('Number'),
      `${op}: ${entry[1]} should include Number`);
}
checkNumber('+');
checkNumber('-');
checkNumber('*');
checkNumber('/');
checkNumber('%');
checkNumber('**');

// --- Bitwise ops ---

// Bitwise ops on Smis produce SignedSmall feedback.
check('|', 1, 2, 'SignedSmall');
check('^', 1, 2, 'SignedSmall');
check('&', 1, 2, 'SignedSmall');

// --- Shift ops ---

check('<<', 1, 2, 'SignedSmall');
check('>>', 1, 2, 'SignedSmall');
check('>>>', 1, 2, 'SignedSmall');

// --- Smi literal ops (XxxSmi bytecodes) ---
// These use the *Smi bytecodes (e.g. AddSmi) which have an immediate Smi
// operand and their own embedded feedback byte.

function checkSmi(op, lhs, expect) {
  const f = new Function('a', `return a ${op} 2;`);
  for (let i = 0; i < 20; i++) f(lhs);
  const feedback = %GetFeedback(f);
  if (feedback === undefined) return;
  const entry = feedback.find(e => e[0] === 'BinaryOp');
  assertEquals('BinaryOp', entry[0], `${op} Smi: expected BinaryOp entry`);
  assertTrue(entry[1].includes(expect),
      `${op} Smi: ${entry[1]} should include ${expect}`);
}

// Arithmetic Smi ops.
checkSmi('+', 1, 'SignedSmall');
checkSmi('-', 1, 'SignedSmall');
checkSmi('*', 1, 'SignedSmall');
checkSmi('/', 4, 'SignedSmall');
checkSmi('%', 5, 'SignedSmall');
checkSmi('**', 2, 'Number');

// Bitwise Smi ops.
checkSmi('|', 1, 'SignedSmall');
checkSmi('^', 1, 'SignedSmall');
checkSmi('&', 1, 'SignedSmall');

// Shift Smi ops.
checkSmi('<<', 1, 'SignedSmall');
checkSmi('>>', 1, 'SignedSmall');
checkSmi('>>>', 1, 'SignedSmall');

// Smi ops with HeapNumber input transition to Number.
function checkSmiNumber(op) {
  const f = new Function('a', `return a ${op} 2;`);
  for (let i = 0; i < 20; i++) f(1.5);
  const feedback = %GetFeedback(f);
  if (feedback === undefined) return;
  const entry = feedback.find(e => e[0] === 'BinaryOp');
  assertTrue(entry[1].includes('Number'),
      `${op} Smi (Number): ${entry[1]} should include Number`);
}
checkSmiNumber('+');
checkSmiNumber('-');
checkSmiNumber('*');
checkSmiNumber('/');
checkSmiNumber('%');
checkSmiNumber('**');
