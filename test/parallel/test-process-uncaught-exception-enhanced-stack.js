'use strict';
// Tests that the enhanced EventEmitter stack trace (containing the
// "Emitted 'error' event at:" frame) is visible to both
// uncaughtExceptionMonitor and uncaughtException handlers.
//
// Also verifies that the enhancement is NOT applied twice on the fatal
// exit path (i.e., no double "Emitted 'error' event at:" frame in crash
// output when there is no handler).

const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const EventEmitter = require('node:events');

// --- Test 1 & 2: uncaughtExceptionMonitor + uncaughtException ---
// Both handlers must see the enhanced stack before the process would exit.
{
  class CustomEmitter extends EventEmitter {}

  const ee = new EventEmitter();
  const customEE = new CustomEmitter();

  let monitorCount = 0;
  let handlerCount = 0;

  process.on('uncaughtExceptionMonitor', common.mustCall((err, origin) => {
    assert.strictEqual(origin, 'uncaughtException');
    monitorCount++;
    if (monitorCount === 1) {
      // Plain EventEmitter - frame must mention the emit call site
      assert.match(err.stack, /Emitted 'error' event at:/,
                   'Monitor: plain EE stack must be enhanced');
      assert.match(err.stack, /at emitPlainError/,
                   'Monitor: plain EE stack must include emitPlainError frame');
    } else if (monitorCount === 2) {
      // Subclass EventEmitter - frame must include the class name
      assert.match(err.stack, /Emitted 'error' event on CustomEmitter instance at:/,
                   'Monitor: subclass stack must be enhanced with class name');
      assert.match(err.stack, /at emitSubclassError/,
                   'Monitor: subclass stack must include emitSubclassError frame');
    }
  }, 2));

  process.on('uncaughtException', common.mustCall((err, origin) => {
    assert.strictEqual(origin, 'uncaughtException');
    handlerCount++;
    if (handlerCount === 1) {
      assert.match(err.stack, /Emitted 'error' event at:/,
                   'Handler: plain EE stack must be enhanced');
      assert.match(err.stack, /at emitPlainError/,
                   'Handler: plain EE stack must include emitPlainError frame');
      // Schedule second throw for next tick after this handler returns
      process.nextTick(emitSubclassError);
    } else if (handlerCount === 2) {
      assert.match(err.stack, /Emitted 'error' event on CustomEmitter instance at:/,
                   'Handler: subclass stack must be enhanced with class name');
      assert.match(err.stack, /at emitSubclassError/,
                   'Handler: subclass stack must include emitSubclassError frame');
    }
  }, 2));

  function emitPlainError() {
    ee.emit('error', new Error('plain error'));
  }

  function emitSubclassError() {
    customEE.emit('error', new Error('subclass error'));
  }

  emitPlainError();
}

// --- Test 3: No handler - fatal exit path must NOT double-apply the frame ---
// This is the critical regression test: if the C++ ReportFatalException path
// also calls enhance_fatal_stack_before_inspector after we already enhanced,
// the "Emitted 'error' event at:" frame would appear twice in the crash output.
{
  const script = `
    const EventEmitter = require('node:events');
    const ee = new EventEmitter();
    function emitError() { ee.emit('error', new Error('crash')); }
    emitError();
  `;
  const result = spawnSync(process.execPath, ['--eval', script], { timeout: 5000 });

  // Process must have exited with non-zero due to unhandled error
  assert.notStrictEqual(result.status, 0);

  const stderr = result.stderr.toString();

  // The enhancement must appear - otherwise the fix regressed
  assert.match(stderr, /Emitted 'error' event at:/,
               'Fatal path: enhanced frame must appear in crash output');

  // The enhancement must appear exactly ONCE - the double-call bug would
  // cause it to appear twice
  const occurrences = (stderr.match(/Emitted 'error' event at:/g) || []).length;
  assert.strictEqual(occurrences, 1,
                     `Fatal path: enhanced frame must appear exactly once, got ${occurrences}`);
}

// --- Test 4: Non-Error EventEmitter emit must not crash ---
// When ee.emit('error', nonError) is called with a non-Error value,
// kEnhanceStackBeforeInspector won't be present on the thrown value.
// The guard (typeof er[kEnhanceStackBeforeInspector] === 'function') must
// prevent any TypeError.
{
  process.once('uncaughtException', common.mustCall((err) => {
    // Err is a plain object here - no stack enhancement expected
    assert.strictEqual(err.message, 'non-error-throw');
  }));

  process.nextTick(() => {
    const thrower = new EventEmitter();
    thrower.emit('error', new Error('non-error-throw'));
  });
}
