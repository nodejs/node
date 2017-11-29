'use strict';

var equal = require('../index');
var tests = require('./tests');
var assert = require('assert');


describe('equal', function() {
  tests.forEach(function (suite) {
    describe(suite.description, function() {
      suite.tests.forEach(function (test) {
        it(test.description, function() {
          assert.strictEqual(equal(test.value1, test.value2), test.equal);
        });
      });
    });
  });
});
