'use strict';
const common = require('../common');
const assert = require('assert');

process.nextTick(function() {
  process.nextTick(function() {
    process.nextTick(function() {
      process.nextTick(function() {
        undefined_reference_error_maker;
      });
    });
  });
});
