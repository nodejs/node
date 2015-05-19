'use strict';
const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;
const EOL = require('os').EOL;
var depmod = require.resolve('../fixtures/printA.js');
var node = process.execPath;

var debug = ['--debug', depmod];
var debugPort = ['--debug=5859', depmod];

function handle(port) {
  return function(er, stdout, stderr) {
    assert.equal(er, null);
    assert.equal(stderr, `Debugger listening on port ${port}${EOL}`);
  };
}

execFile(node, debug, handle(5858));
execFile(node, debugPort, handle(5859));
