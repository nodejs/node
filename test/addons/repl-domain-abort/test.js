'use strict';
require('../../common');
var assert = require('assert');
var repl = require('repl');
var stream = require('stream');
var buildType = process.config.target_defaults.default_configuration;
var buildPath = __dirname + '/build/' + buildType + '/binding';
var cb_ran = false;

process.on('exit', function() {
  assert(cb_ran);
  console.log('ok');
});

var lines = [
  // This line shouldn't cause an assertion error.
  'require(\'' + buildPath + '\')' +
  // Log output to double check callback ran.
  '.method(function() { console.log(\'cb_ran\'); });',
];

var dInput = new stream.Readable();
var dOutput = new stream.Writable();

dInput._read = function _read(size) {
  while (lines.length > 0 && this.push(lines.shift()));
  if (lines.length === 0)
    this.push(null);
};

dOutput._write = function _write(chunk, encoding, cb) {
  if (chunk.toString().indexOf('cb_ran') === 0)
    cb_ran = true;
  cb();
};

var options = {
  input: dInput,
  output: dOutput,
  terminal: false,
  ignoreUndefined: true
};

// Run commands from fake REPL.
var dummy = repl.start(options);
