'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');

const input = [
  'const foo = {', // start object
  '};',            // end object
  'foo',            // evaluate variable
];

function runPromptTest(promptStr, { useColors }) {
  const inputStream = new ArrayStream();
  const outputStream = new ArrayStream();
  let output = '';

  outputStream.write = (data) => { output += data.replace('\r', ''); };

  const r = repl.start({
    prompt: '',
    input: inputStream,
    output: outputStream,
    terminal: true,
    useColors,
    multilinePrompt: promptStr,
  });

  r.on('exit', common.mustCall(() => {
    const lines = output.split('\n');

    // Validate REPL output
    assert.ok(lines[0].endsWith(input[0])); // first line
    assert.ok(lines[1].includes(promptStr)); // continuation line
    assert.ok(lines[1].endsWith(input[1])); // second line content
    assert.ok(lines[2].includes('undefined')); // first eval result
    assert.ok(lines[3].endsWith(input[2])); // final variable
    assert.ok(lines[4].includes('{}')); // printed object
  }));

  inputStream.run(input);
  r.close();
}

// Test with custom `... ` prompt
runPromptTest('... ', { useColors: true });
runPromptTest('... ', { useColors: false });

// Test with default `| ` prompt
runPromptTest('| ', { useColors: true });
runPromptTest('| ', { useColors: false });
