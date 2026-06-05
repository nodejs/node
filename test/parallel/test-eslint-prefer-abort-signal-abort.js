'use strict';

const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}

common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/prefer-abort-signal-abort');

const message = 'Use AbortSignal.abort() instead of creating and aborting an AbortController.';

new RuleTester().run('prefer-abort-signal-abort', rule, {
  valid: [
    'const signal = AbortSignal.abort();',
    `
      const controller = new AbortController();
      controller.abort();
      controller.abort();
      fn(controller.signal);
    `,
    `
      const controller = new AbortController();
      controller.abort();
      console.log(controller);
      fn(controller.signal);
    `,
    `
      const controller = new AbortController();
      // This comment should not be removed.
      controller.abort();
      fn(controller.signal);
    `,
    `
      const controller = new AbortController();
      setImmediate(() => controller.abort());
      fn(controller.signal);
    `,
    `
      const controller = new AbortController();
      controller.abort('reason', 'extra');
      fn(controller.signal);
    `,
  ],
  invalid: [
    {
      code: `
        const controller = new AbortController();
        controller.abort();
        fn(controller.signal);
      `,
      errors: [{ message }],
      output: `
        fn(AbortSignal.abort());
      `,
    },
    {
      code: `
        const abortController = new AbortController();
        abortController.abort(new Error('aborted'));
        fn({ signal: abortController.signal });
      `,
      errors: [{ message }],
      output: `
        fn({ signal: AbortSignal.abort(new Error('aborted')) });
      `,
    },
    {
      code: `
        {
          const ac = new AbortController();
          ac.abort();
          await wait({ signal: ac.signal });
        }
      `,
      errors: [{ message }],
      output: `
        {
          await wait({ signal: AbortSignal.abort() });
        }
      `,
    },
    {
      code: `
        {
          const controller = new AbortController();
          controller.abort();
          fn(controller.signal, controller.signal);
        }
      `,
      errors: [{ message }],
      output: `
        {
          const controller = AbortSignal.abort();
          fn(controller, controller);
        }
      `,
    },
    {
      code: `
        {
          const controller = new AbortController();
          controller.abort("reason");
          fn(controller.signal, controller.signal);
        }
      `,
      errors: [{ message }],
      output: `
        {
          const controller = AbortSignal.abort("reason");
          fn(controller, controller);
        }
      `,
    },
  ]
});
