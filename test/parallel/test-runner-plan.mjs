import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it, run } from 'node:test';
import { join } from 'node:path';

const testFixtures = fixtures.path('test-runner', 'plan');

describe('input validation', () => {
  it('throws if options is not an object', (t) => {
    t.assert.throws(() => {
      t.plan(1, null);
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options" argument must be of type object/,
    });
  });

  it('throws if options.wait is not a number or a boolean', (t) => {
    t.assert.throws(() => {
      t.plan(1, { wait: 'foo' });
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "options\.wait" property must be one of type boolean or number\. Received type string/,
    });
  });

  it('throws if count is not a number', (t) => {
    t.assert.throws(() => {
      t.plan('foo');
    }, {
      code: 'ERR_INVALID_ARG_TYPE',
      message: /The "count" argument must be of type number/,
    });
  });
});

describe('test planning', () => {
  it('should pass when assertions match plan', async () => {
    const stream = run({
      files: [join(testFixtures, 'match.mjs')]
    });

    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustCall(1));

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should fail when less assertions than planned', async () => {
    const stream = run({
      files: [join(testFixtures, 'less.mjs')]
    });

    stream.on('test:fail', common.mustCall(1));
    stream.on('test:pass', common.mustNotCall());

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should fail when more assertions than planned', async () => {
    const stream = run({
      files: [join(testFixtures, 'more.mjs')]
    });

    stream.on('test:fail', common.mustCall(1));
    stream.on('test:pass', common.mustNotCall());

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should handle plan with subtests correctly', async () => {
    const stream = run({
      files: [join(testFixtures, 'subtest.mjs')]
    });

    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustCall(2)); // Parent + child test

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should handle plan via options', async () => {
    const stream = run({
      files: [join(testFixtures, 'plan-via-options.mjs')]
    });

    stream.on('test:fail', common.mustCall(1));
    stream.on('test:pass', common.mustCall(1));

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should handle streaming with plan', async () => {
    const stream = run({
      files: [join(testFixtures, 'streaming.mjs')]
    });

    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustCall(1));

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  it('should handle nested subtests with plan', async () => {
    const stream = run({
      files: [join(testFixtures, 'nested-subtests.mjs')]
    });

    stream.on('test:fail', common.mustNotCall());
    stream.on('test:pass', common.mustCall(3)); // Parent + 2 levels of nesting

    // eslint-disable-next-line no-unused-vars
    for await (const _ of stream);
  });

  describe('with timeout', () => {
    it('should handle basic timeout scenarios', async () => {
      const stream = run({
        files: [join(testFixtures, 'timeout-basic.mjs')]
      });

      stream.on('test:fail', common.mustCall(1));
      stream.on('test:pass', common.mustCall(1));

      // eslint-disable-next-line no-unused-vars
      for await (const _ of stream);
    });

    it('should fail when timeout expires before plan is met', async (t) => {
      const stream = run({
        files: [join(testFixtures, 'timeout-expired.mjs')]
      });

      stream.on('test:fail', common.mustCall(1));
      stream.on('test:pass', common.mustNotCall());

      // eslint-disable-next-line no-unused-vars
      for await (const _ of stream);
    });

    it('should handle wait:true option specifically', async () => {
      const stream = run({
        files: [join(testFixtures, 'timeout-wait-true.mjs')]
      });

      stream.on('test:fail', common.mustCall(1));
      stream.on('test:pass', common.mustCall(1));

      // eslint-disable-next-line no-unused-vars
      for await (const _ of stream);
    });

    it('should handle wait:false option (should not wait)', async () => {
      const stream = run({
        files: [join(testFixtures, 'timeout-wait-false.mjs')]
      });

      stream.on('test:fail', common.mustCall(1)); // Fails because plan is not met immediately
      stream.on('test:pass', common.mustNotCall());

      // eslint-disable-next-line no-unused-vars
      for await (const _ of stream);
    });
  });
});
