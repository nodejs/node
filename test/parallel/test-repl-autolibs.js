'use strict';
const common = require('../common');
var assert = require('assert');
var util = require('util');
var repl = require('repl');

// This test adds global variables
common.globalCheck = false;

const putIn = new common.ArrayStream();
repl.start('', putIn, null, true);

test1();

function test1() {
  var gotWrite = false;
  putIn.write = function(data) {
    gotWrite = true;
    if (data.length) {

      // inspect output matches repl output
      assert.equal(data, util.inspect(require('fs'), null, 2, false) + '\r\n');
      // globally added lib matches required lib
      assert.equal(global.fs, require('fs'));
      test2();
    }
  };
  assert(!gotWrite);
  putIn.run(['fs']);
  assert(gotWrite);
}

function test2() {
  var gotWrite = false;
  putIn.write = function(data) {
    gotWrite = true;
    if (data.length) {
      // repl response error message
      assert.equal(data, '{}\r\n');
      // original value wasn't overwritten
      assert.equal(val, global.url);
    }
  };
  var val = {};
  global.url = val;
  assert(!gotWrite);
  putIn.run(['url']);
  assert(gotWrite);
}
