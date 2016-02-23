'use strict';
require('../common');

process.nextTick(function() {
  process.nextTick(function() {
    process.nextTick(function() {
      process.nextTick(function() {
        undefined_reference_error_maker;
      });
    });
  });
});
