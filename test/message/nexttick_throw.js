'use strict';
require('../common');

process.nextTick(function() {
  process.nextTick(function() {
    process.nextTick(function() {
      process.nextTick(function() {
        // eslint-disable-next-line
        undefined_reference_error_maker;
      });
    });
  });
});
