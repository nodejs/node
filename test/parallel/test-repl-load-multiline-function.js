'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const join = require('path').join;
const fs = require('fs');

common.skipIfDumbTerminal();

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const terminalCode = '(\u001b[1G\u001b[0J \u001b[1G)';
const terminalCodeRegex = new RegExp(terminalCode.replace(/\[/g, '\\['), 'g');

const repl = require('repl');

const inputStream = new ArrayStream();
const outputStream = new ArrayStream();

const r = repl.start({
  prompt: '',
  input: inputStream,
  output: outputStream,
  terminal: true,
  useColors: false
});

const testFile = 'function a(b) {\n  return b }\na(1)\n';
const testFileName = join(tmpdir.path, 'foo.js');
fs.writeFileSync(testFileName, testFile);

const command = `.load ${testFileName}\n`;
let accum = '';
outputStream.write = (data) => accum += data.replace('\r', '');


r.write(command);

const expected = command +
'function a(b) {\n' +
'  return b }\n' +
'a(1)\n' +
'\n' +
'1\n';
assert.strictEqual(accum.replace(terminalCodeRegex, ''), expected);
r.close();
