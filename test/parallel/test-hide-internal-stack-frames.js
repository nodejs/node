'use strict';

// Tests that --hide-internal-stack-frames collapses node:internal frames into
// a placeholder in every stack Node.js formats, while keeping user and public
// builtin frames.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');

const kPlaceholder = /^ {4}at <node:internal> \(\d+ frames? hidden\)$/m;
const kInternalFrame = /node:internal\/|\]-wrapper:/;

const script = `
  const fs = require('fs');
  try {
    fs.readFileSync('/does-not-exist-' + process.pid);
  } catch (err) {
    console.log(err.stack);
  }
`;

// Without the flag, the CJS loader frames are present.
spawnSyncAndAssert(process.execPath, ['-e', script], {
  stdout(output) {
    assert.match(output, kInternalFrame);
    assert.doesNotMatch(output, kPlaceholder);
  },
});

const checkHidden = {
  stdout: common.mustCall((output) => {
    assert.doesNotMatch(output, kInternalFrame);
    assert.match(output, kPlaceholder);
    assert.match(output, /at Object\.readFileSync \(node:fs:/);
    assert.match(output, /\[eval\]:4:/);
    // The generated [eval]-wrapper frame is folded in with the internals.
    assert.match(output, /\[eval\]:4:\d+\)?\n {4}at <node:internal> \(\d+ frames hidden\)\n/);
  }, 3),
};

spawnSyncAndAssert(process.execPath,
                   ['--hide-internal-stack-frames', '-e', script],
                   checkHidden);

spawnSyncAndAssert(process.execPath, ['-e', script], {
  env: { ...process.env, NODE_OPTIONS: '--hide-internal-stack-frames' },
}, checkHidden);

// Works together with --enable-source-maps.
spawnSyncAndAssert(process.execPath,
                   ['--hide-internal-stack-frames', '--enable-source-maps',
                    '-e', script],
                   checkHidden);

// Consecutive internal frames are collapsed into a single placeholder.
spawnSyncAndAssert(process.execPath, [
  '--hide-internal-stack-frames',
  '-e',
  'console.log(new Error().stack)',
], {
  stdout(output) {
    const lines = output.trimEnd().split('\n').slice(1);
    for (let i = 1; i < lines.length; i++) {
      assert.ok(!(kPlaceholder.test(lines[i]) && kPlaceholder.test(lines[i - 1])),
                `adjacent placeholders in:\n${output}`);
    }
  },
});

// A user-provided Error.prepareStackTrace still receives every frame.
spawnSyncAndAssert(process.execPath, [
  '--hide-internal-stack-frames',
  '-e',
  `Error.prepareStackTrace = (err, trace) =>
     trace.some((cs) => cs.getFileName()?.startsWith('node:internal/'));
   console.log(new Error().stack);`,
], {
  stdout: 'true\n',
});

// Stacks printed from C++ are filtered as well.
const checkNativeStderr = {
  stderr: common.mustCall((output) => {
    assert.doesNotMatch(output, kInternalFrame);
    assert.match(output, kPlaceholder);
  }, 2),
};

spawnSyncAndAssert(process.execPath, [
  '--hide-internal-stack-frames', '--trace-exit', '-e', 'process.exit(0)',
], checkNativeStderr);

spawnSyncAndAssert(process.execPath, [
  '--hide-internal-stack-frames', '--trace-uncaught', '-e', 'throw 1',
], { status: 1, ...checkNativeStderr });

spawnSyncAndAssert(process.execPath, [
  '--hide-internal-stack-frames', '--process-timeout=100ms',
  '-e', 'for (;;);',
], {
  status: 124,
  stderr(output) {
    assert.match(output, /Main thread was executing JavaScript:/);
    assert.doesNotMatch(output, kInternalFrame);
    assert.match(output, kPlaceholder);
  },
});

// Diagnostic report JavaScript stacks.
spawnSyncAndAssert(process.execPath, [
  '--hide-internal-stack-frames',
  '-e',
  'console.log(JSON.stringify(process.report.getReport().javascriptStack.stack))',
], {
  stdout(output) {
    const stack = JSON.parse(output);
    assert.ok(stack.length > 0);
    for (const line of stack) {
      assert.doesNotMatch(line, kInternalFrame);
    }
    assert.ok(stack.some((line) => /^at <node:internal> \(\d+ frames? hidden\)$/.test(line)),
              `no placeholder in ${output}`);
  },
});
