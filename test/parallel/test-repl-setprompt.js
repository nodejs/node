'use strict';
require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const os = require('os');

var args = [
  '-e',
  'var e = new (require("repl")).REPLServer("foo.. "); e.context.e = e;',
];

var p = 'bar.. ';

var child = spawn(process.execPath, args);

child.stdout.setEncoding('utf8');

var data = '';
child.stdout.on('data', function(d) { data += d; });

child.stdin.end(`e.setPrompt("${p}");${os.EOL}`);

child.on('close', function(code, signal) {
  assert.strictEqual(code, 0);
  assert.ok(!signal);
  var lines = data.split(/\n/);
  assert.strictEqual(lines.pop(), p);
});
