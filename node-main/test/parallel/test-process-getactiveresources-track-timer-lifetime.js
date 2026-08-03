'use strict';

const common = require('../common');

const assert = require('assert');

{
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Timeout').length, 0);

  const timeout = setTimeout(common.mustCall(() => {
    assert.strictEqual(process.getActiveResourcesInfo().filter(
      (type) => type === 'Timeout').length, 1);
    clearTimeout(timeout);
    assert.strictEqual(process.getActiveResourcesInfo().filter(
      (type) => type === 'Timeout').length, 0);
  }), 0);

  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Timeout').length, 1);
}

{
  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Immediate').length, 0);

  const immediate = setImmediate(common.mustCall(() => {
    // TODO(RaisinTen): Change this test to the following when the Immediate is
    // destroyed and unrefed after the callback gets executed.
    // assert.strictEqual(process.getActiveResourcesInfo().filter(
    //   (type) => type === 'Immediate').length, 1);
    assert.strictEqual(process.getActiveResourcesInfo().filter(
      (type) => type === 'Immediate').length, 0);
    clearImmediate(immediate);
    assert.strictEqual(process.getActiveResourcesInfo().filter(
      (type) => type === 'Immediate').length, 0);
  }));

  assert.strictEqual(process.getActiveResourcesInfo().filter(
    (type) => type === 'Immediate').length, 1);
}
