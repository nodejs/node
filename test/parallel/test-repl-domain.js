'use strict';
var assert = require('assert');
var common = require('../common');

var util   = require('util');
var repl   = require('repl');

const putIn = new common.ArrayStream();
var testMe = repl.start('', putIn);

putIn.write = function(data) {
  // Don't use assert for this because the domain might catch it, and
  // give a false negative.  Don't throw, just print and exit.
  if (data === 'OK\n') {
    console.log('ok');
  }
  else {
    console.error(data);
    process.exit(1);
  }
};

putIn.run([
  'require("domain").create().on("error", function() { console.log("OK") })'
  + '.run(function() { throw new Error("threw") })'
]);
