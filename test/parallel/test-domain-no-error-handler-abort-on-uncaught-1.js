'use strict';

const common = require('../common');
const domain = require('domain');

function test() {
  const d = domain.create();
  const d2 = domain.create();

  d.run(function() {
    d2.run(function() {
      throw new Error('boom!');
    });
  });
}

if (process.argv[2] === 'child') {
  test();
} else {
  common.childShouldThrowAndAbort();
}
