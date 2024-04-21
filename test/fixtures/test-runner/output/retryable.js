const common = require('../../../common');
const test = require('node:test');

{
  // Expect: Success
  let count = 0;
  test.attempt({
    retries: 5,
  }, common.mustCallAtLeast(() => {
    count++;
    if (count < 3) {
      throw new Error('fail');
    }
  }, 3));
}

{
  // Expect: Failure
  test.attempt({
    retries: 5,
  }, common.mustCallAtLeast(() => {
    throw new Error('fail');
  }, 5));
}