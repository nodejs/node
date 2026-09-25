'use strict';
require('../../common');

process.nextTick(function() {
  process.nextTick(function() {
    process.nextTick(function() {
      process.nextTick(function() {
        // eslint-disable-next-line no-undef,no-unused-expressions
        undefined_reference_error_maker;
      });
    });
  });
});
