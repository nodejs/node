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

    const firstLine = useColors ?
      '\x1B[1G\x1B[0J \x1B[1Gco\x1B[90mn\x1B[39m\x1B[3G\x1B[0Knst ' +
        'fo\x1B[90mr\x1B[39m\x1B[9G\x1B[0Ko = {' :
      '\x1B[1G\x1B[0J \x1B[1Gco // n\x1B[3G\x1B[0Knst ' +
        'fo // r\x1B[9G\x1B[0Ko = {';

    // Validate the output, which contains terminal escape codes.
    assert.strictEqual(actual.length, 6 + process.features.inspector);
    assert.strictEqual(actual[0], firstLine);
    assert.ok(actual[1].includes('... '));
    assert.ok(actual[1].endsWith(input[1]));
    assert.ok(actual[2].includes('undefined'));
    if (process.features.inspector) {
      assert.ok(
        actual[3].endsWith(input[2]),
        `"${actual[3]}" should end with "${input[2]}"`
      );
      assert.ok(actual[4].includes(actual[5]));
      assert.strictEqual(actual[4].includes('//'), !useColors);
    }
    assert.strictEqual(actual[4 + process.features.inspector], '{}');
    // Ignore the last line, which is nothing but escape codes.
  }));

  inputStream.run(input);
  r.close();
}

run({ useColors: true });
run({ useColors: false });
