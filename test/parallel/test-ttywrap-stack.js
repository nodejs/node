'use strict';
const common = require('../common');

// This test ensures that console.log
// will not crash the process if there
// is not enough space on the V8 stack

const done = common.mustCall(() => {});

async function test() {
  await test();
}

(async () => {
  try {
    await test();
  } catch (err) {
    console.log(err);
  }
})().then(done, done);
