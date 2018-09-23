'use strict';
var common = require('../common');
var assert = require('assert');

var path = require('path'),
    fs = require('fs'),
    fn = path.join(common.fixturesDir, 'empty.txt');

fs.readFile(fn, function(err, data) {
  assert.ok(data);
});

fs.readFile(fn, 'utf8', function(err, data) {
  assert.strictEqual('', data);
});

assert.ok(fs.readFileSync(fn));
assert.strictEqual('', fs.readFileSync(fn, 'utf8'));
