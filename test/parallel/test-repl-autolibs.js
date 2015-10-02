/* eslint-disable required-modules */
'use strict';
var assert = require('assert');
var util = require('util');
var repl = require('repl');

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    var self = this;
    data.forEach(function(line) {
      self.emit('data', line + '\n');
    });
  };
}
util.inherits(ArrayStream, require('stream').Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function() {};

var putIn = new ArrayStream();
var testMe = repl.start('', putIn, null, true);

test1();

function test1() {
  var gotWrite = false;
  putIn.write = function(data) {
    gotWrite = true;
    if (data.length) {

      // inspect output matches repl output
      assert.equal(data, util.inspect(require('fs'), null, 2, false) + '\n');
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
      assert.equal(data, '{}\n');
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
