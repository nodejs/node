'use strict';
var common = require('../common'),
    assert = require('assert'),
    spawn = require('child_process').spawn,
    os = require('os'),
    util = require('util');

var args = [
  '-e',
  'var e = new (require("repl")).REPLServer("foo.. "); e.context.e = e;',
];

var p = 'bar.. ';

var child = spawn(process.execPath, args);

child.stdout.setEncoding('utf8');

var data = '';
child.stdout.on('data', function(d) { data += d; });

child.stdin.end(util.format("e.setPrompt('%s');%s", p, os.EOL));

child.on('close', function(code, signal) {
  assert.strictEqual(code, 0);
  assert.ok(!signal);
  var lines = data.split(/\n/);
  assert.strictEqual(lines.pop(), p);
});
