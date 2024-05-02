const common = require('../../common');
common.skipIfWorker();

const assert = require('assert');

{
  assert.throws(() => {
    process.binding();
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
  }));
}

{
  assert.throws(() => {
    process.binding('async_wrap');
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
  }));
}

{
  assert.throws(() => {
    process.binding('fs');
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
  }));
}
