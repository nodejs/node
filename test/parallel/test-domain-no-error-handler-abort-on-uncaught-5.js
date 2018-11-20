'use strict';

const common = require('../common');
const domain = require('domain');

function test() {
  const d = domain.create();

  d.run(function() {
    const fs = require('fs');
    fs.exists('/non/existing/file', function onExists() {
      throw new Error('boom!');
    });
  });
}

if (process.argv[2] === 'child') {
  test();
} else {
  common.childShouldThrowAndAbort();
}
