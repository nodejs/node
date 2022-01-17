'use strict';

const common = require('../common');
const {
  pipeline,
  PassThrough
} = require('stream');


async function runTest() {
  await pipeline(
    '',
    new PassThrough({ objectMode: true }),
    common.mustCall(() => { })
  );
}

runTest().then(common.mustCall(() => {}));
