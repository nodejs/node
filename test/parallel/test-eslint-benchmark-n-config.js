'use strict';

const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}

common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/benchmark-n-config');

const message = '`n` is configured but not read by the benchmark main ' +
                'function (destructure `{ n }` or read `conf.n`)';

new RuleTester().run('benchmark-n-config', rule, {
  valid: [
    `
      const bench = common.createBenchmark(main, { len: [1], n: [100] });
      function main({ len, n }) { bench.end(n); }
    `,
    `
      const bench = common.createBenchmark(main, { n: [100] });
      function main(conf) { bench.end(conf.n); }
    `,
    `
      const bench = common.createBenchmark(main, { n: [100] });
      function main(conf) { const { n } = conf; bench.end(n); }
    `,
    `
      const bench = common.createBenchmark(({ n }) => bench.end(n), { n: [1] });
    `,
    `
      const configs = { n: [100] };
      const bench = common.createBenchmark(main, configs);
      function main({ n }) { bench.end(n); }
    `,
    // Configs without `n` (e.g. duration-driven benchmarks) are not checked.
    `
      const bench = common.createBenchmark(main, { duration: [5] });
      function main({ duration }) {}
    `,
    // Non-static configs are skipped.
    `
      const bench = common.createBenchmark(main, { ...base, n: [100] });
      function main({ len }) {}
    `,
  ],
  invalid: [
    // nodejs/node#59872: the config key was renamed to `n`, but main() kept
    // reading `num`.
    {
      code: `
        const bench = common.createBenchmark(main, { len: [1], n: [100] });
        function main({ len, num: n }) { bench.end(n); }
      `,
      errors: [{ message }],
    },
    {
      code: `
        const bench = common.createBenchmark(main, { n: [1e6], duration: [5] });
        function main({ duration }) {}
      `,
      errors: [{ message }],
    },
    {
      code: `
        const bench = common.createBenchmark(main, { n: [100] });
        function main(conf) { bench.end(conf.num); }
      `,
      errors: [{ message }],
    },
    {
      code: `
        const configs = { n: [100] };
        const bench = common.createBenchmark(main, configs);
        function main({ count }) { bench.end(count); }
      `,
      errors: [{ message }],
    },
  ],
});
