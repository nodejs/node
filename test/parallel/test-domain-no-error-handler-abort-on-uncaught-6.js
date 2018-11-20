'use strict';

const common = require('../common');
const domain = require('domain');

function test() {
  const d = domain.create();
  const d2 = domain.create();

  d.on('error', function errorHandler() {
  });

  d.run(function() {
    d2.run(function() {
      setTimeout(function() {
        throw new Error('boom!');
      }, 1);
    });
  });
}

if (process.argv[2] === 'child') {
  test();
} else {
  common.childShouldThrowAndAbort();
}
