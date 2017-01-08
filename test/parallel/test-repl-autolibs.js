'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('util');
const repl = require('repl');

// This test adds global variables
common.globalCheck = false;

const putIn = new common.ArrayStream();
repl.start('', putIn, null, true);

test1();

function test1() {
  let gotWrite = false;
  putIn.write = function(data) {
    gotWrite = true;
    if (data.length) {

      // inspect output matches repl output
      assert.strictEqual(data, util.inspect(require('fs'), null, 2, false) +
                         '\n');
      // globally added lib matches required lib
      assert.strictEqual(global.fs, require('fs'));
      test2();
    }
  };
  assert(!gotWrite);
  putIn.run(['fs']);
  assert(gotWrite);
}

function test2() {
  let gotWrite = false;
  putIn.write = function(data) {
    gotWrite = true;
    if (data.length) {
      // repl response error message
      assert.strictEqual(data, '{}\n');
      // original value wasn't overwritten
      assert.strictEqual(val, global.url);
    }
  };
  let val = {};
  global.url = val;
  assert(!gotWrite);
  putIn.run(['url']);
  assert(gotWrite);
}
