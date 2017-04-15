'use strict';
// Refs: https://github.com/nodejs/node/issues/7788
const common = require('../common');
const assert = require('assert');
const path = require('path');
const repl = require('repl');
const stream = require('stream');
const inputStream = new stream.PassThrough();
const outputStream = new stream.PassThrough();
const fixture = path.join(common.fixturesDir, 'is-object.js');
const r = repl.start({
  input: inputStream,
  output: outputStream,
  useGlobal: false
});

let output = '';
outputStream.setEncoding('utf8');
outputStream.on('data', (data) => output += data);

r.on('exit', common.mustCall(() => {
  const results = output.replace(/^> /mg, '').split('\n');

  assert.deepStrictEqual(results, ['undefined', 'true', 'true', '']);
}));

inputStream.write('const isObject = (obj) => obj.constructor === Object;\n');
inputStream.write('isObject({});\n');
inputStream.write(`require('${fixture}').isObject({});\n`);
r.close();
