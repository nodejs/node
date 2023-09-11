'use strict';

const common = require('../common');
const domain = require('domain');

function test() {
  const d = domain.create();

  d.run(function() {
    setTimeout(function() {
      throw new Error('boom!');
    }, 1);
  });
}

if (process.argv[2] === 'child') {
  test();
} else {
  common.childShouldThrowAndAbort();
}
