'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');

function runTest({ subprompt, expectedSubprompt }) {
  const inputStream = new ArrayStream();
  const outputStream = new ArrayStream();
  let output = '';

  outputStream.write = (data) => { output += data.replace('\r', ''); };

  const r = repl.start({
    prompt: '> ',
    input: inputStream,
    output: outputStream,
    terminal: true,
    useColors: false,
    subprompt,
  });

  r.on('exit', common.mustCall(() => {
    const actual = output.split('\n');

    // Validate that the correct subprompt is used for multiline input
    assert.ok(actual.some((line) => line.includes(expectedSubprompt)),
              `Expected to find "${expectedSubprompt}" in output`);
  }));

  // Input multiline code to trigger the subprompt
  inputStream.run([
    'const obj = {',
    '  a: 1,',
    '  b: 2',
    '};',
    'obj',
  ]);

  r.close();
}

// Test default subprompt
runTest({
  subprompt: undefined,
  expectedSubprompt: '| ',
});

// Test custom subprompt
runTest({
  subprompt: '... ',
  expectedSubprompt: '... ',
});

// Test empty string subprompt
runTest({
  subprompt: '',
  expectedSubprompt: '',
});

// Test long subprompt
runTest({
  subprompt: '  -> ',
  expectedSubprompt: '  -> ',
});

// Test non-string subprompt should be ignored
runTest({
  subprompt: 123,
  expectedSubprompt: '| ',
});

// Test null subprompt should be ignored
runTest({
  subprompt: null,
  expectedSubprompt: '| ',
}); 