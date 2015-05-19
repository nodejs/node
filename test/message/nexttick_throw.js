'use strict';
var common = require('../common');
var assert = require('assert');

process.nextTick(function() {
  process.nextTick(function() {
    process.nextTick(function() {
      process.nextTick(function() {
        undefined_reference_error_maker;
      });
    });
  });
});
