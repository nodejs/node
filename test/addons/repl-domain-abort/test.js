'use strict';
const common = require('../../common');
const assert = require('assert');
const repl = require('repl');
const stream = require('stream');
const path = require('path');
let buildPath = path.join(__dirname, 'build', common.buildType, 'binding');
// On Windows, escape backslashes in the path before passing it to REPL.
if (common.isWindows)
  buildPath = buildPath.replace(/\\/g, '/');
let cb_ran = false;

process.on('exit', function() {
  assert(cb_ran);
  console.log('ok');
});

const lines = [
  // This line shouldn't cause an assertion error.
  `require('${buildPath}')` +
  // Log output to double check callback ran.
  '.method(function() { console.log(\'cb_ran\'); });',
];

const dInput = new stream.Readable();
const dOutput = new stream.Writable();

dInput._read = function _read() {
  while (lines.length > 0 && this.push(lines.shift()));
  if (lines.length === 0)
    this.push(null);
};

dOutput._write = function _write(chunk, encoding, cb) {
  if (chunk.toString().startsWith('cb_ran'))
    cb_ran = true;
  cb();
};

const options = {
  input: dInput,
  output: dOutput,
  terminal: false,
  ignoreUndefined: true
};

// Run commands from fake REPL.
repl.start(options);
