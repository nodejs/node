'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');
const input = ['const foo = {', '};', 'foo'];

function run({ useColors }) {
  const inputStream = new ArrayStream();
  const outputStream = new ArrayStream();
  let output = '';

  outputStream.write = (data) => { output += data.replace('\r', ''); };

  const r = repl.start({
    prompt: '',
    input: inputStream,
    output: outputStream,
    terminal: true,
    useColors
  });

  r.on('exit', common.mustCall(() => {
    const actual = output.split('\n');

    // Validate the output, which contains terminal escape codes.
    assert.strictEqual(actual.length, 6);
    assert.ok(actual[0].endsWith(input[0]));
    assert.ok(actual[1].includes('... '));
    assert.ok(actual[1].endsWith(input[1]));
    assert.ok(actual[2].includes('undefined'));
    assert.ok(actual[3].endsWith(input[2]));
    assert.strictEqual(actual[4], '{}');
  }));

  inputStream.run(input);
  r.close();
}

run({ useColors: true });
run({ useColors: false });
