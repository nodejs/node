'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

if (process.title === '') {
  console.log('skipping test -- not implemented for the host platform');
  //return;
}

// disabled because of two things
// - not tested on linux (can ps show process title on Linux?)
// - unable to verify effect on Darwin/OS X (only avail through GUI tools AFAIK)

function verifyProcessName(str, callback) {
  process.title = str;
  var buf = '';
  ps = spawn('ps');
  ps.stdout.setEncoding('utf8');
  ps.stdout.on('data', function(s) { buf += s; });
  ps.on('exit', function(c) {
    try {
      assert.equal(0, c);
      assert.ok(new RegExp(process.pid + ' ', 'm').test(buf));
      assert.ok(new RegExp(str, 'm').test(buf));
      callback();
    } catch (err) {
      callback(err);
    }
  });
}

verifyProcessName('3kd023mslkfp--unique-string--sksdf', function(err) {
  if (err) throw err;
  console.log('title is now %j', process.title);
  verifyProcessName('3kd023mslxxx--unique-string--xxx', function(err) {
    if (err) throw err;
    console.log('title is now %j', process.title);
  });
});
