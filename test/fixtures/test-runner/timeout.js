'use strict';
const test = require('node:test');
const common = require('../../common');
const fixtures = require('../../common/fixtures');

test('should support timeout', async () => {
  const stream = test.run({files: [
    fixtures.path('test-runner', 'never_ending_sync.js'),
    fixtures.path('test-runner', 'never_ending_async.js'),
  ] });
  stream.on('test:fail', common.mustCall(2));
  stream.on('test:pass', common.mustNotCall());
  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
});
