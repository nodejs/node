// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { createRunner } = require('node:bench');

const runner = createRunner({ yieldBetweenSamples: false });

runner.bench('done during warmup', { samples: 1, warmup: 1 }, (b) => {
  completeSample(b);
  b.done();
});
runner.bench('invalid record', { samples: 1 }, (b) => b.record(null));
runner.bench('invalid duration type', { samples: 1 }, (b) => b.record({
  duration_ns: 1,
  operations: 1,
}));
runner.bench('invalid duration value', { samples: 1 }, (b) => b.record({
  duration_ns: 0n,
  operations: 1,
}));
runner.bench('duration too large', { samples: 1 }, (b) => b.record({
  duration_ns: 9_007_199_254_740_992n,
  operations: 1,
}));
runner.bench('invalid operations', { samples: 1 }, (b) => b.record({
  duration_ns: 1n,
  operations: 0,
}));
runner.bench('mixed timing', { samples: 1 }, (b) => {
  b.start();
  b.record({ duration_ns: 1n, operations: 1 });
});
runner.bench('start after record', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
  b.start();
});
runner.bench('end after record', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
  b.end(1);
});
runner.bench('duplicate record', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
  b.record({ duration_ns: 1n, operations: 1 });
});
runner.bench('reentrant record', { samples: 1 }, (b) => {
  const sample = { duration_ns: 1n };
  Object.defineProperty(sample, 'operations', {
    get() {
      b.record({ duration_ns: 1n, operations: 1 });
      return 1;
    },
  });
  b.record(sample);
});
runner.bench('uncloneable detail', { samples: 1 }, (b) => {
  b.record({ duration_ns: 1n, operations: 1, detail: () => {} });
});
runner.bench('uncloneable diagnostic message', { samples: 1 }, (b) => {
  b.diagnostic(() => {});
});
runner.bench('invalid diagnostic level', { samples: 1 }, (b) => {
  b.diagnostic('invalid', { level: 'error' });
});
runner.bench('uncloneable diagnostic detail', { samples: 1 }, (b) => {
  b.diagnostic('invalid', { detail: () => {} });
});
runner.bench('caught contract violation', { samples: 1 },
             common.mustCall((b) => {
               b.start();
               assert.throws(() => b.start(), { code: 'ERR_INVALID_STATE' });
               b.end(1);
             }));
runner.bench('caught diagnostic violation', { samples: 1 },
             common.mustCall((b) => {
               assert.throws(() => b.diagnostic('invalid', {
                 level: 'error',
               }), { code: 'ERR_INVALID_ARG_VALUE' });
               b.record({ duration_ns: 1n, operations: 1 });
             }));

(async () => {
  const records = await runner.run().toArray();
  const completions = records
    .filter(({ type }) => type === 'bench:complete')
    .map(({ data }) => data);
  const byName = new Map(completions.map((result) => [result.name, result]));

  assert.strictEqual(byName.get('done during warmup').error.code,
                     'ERR_INVALID_STATE');
  assert.strictEqual(byName.get('invalid record').error.code,
                     'ERR_INVALID_ARG_TYPE');
  assert.strictEqual(byName.get('invalid duration type').error.code,
                     'ERR_INVALID_ARG_TYPE');
  assert.strictEqual(byName.get('invalid duration value').error.code,
                     'ERR_OUT_OF_RANGE');
  assert.strictEqual(byName.get('duration too large').error.code,
                     'ERR_OUT_OF_RANGE');
  assert.strictEqual(byName.get('invalid operations').error.code,
                     'ERR_OUT_OF_RANGE');
  assert.strictEqual(byName.get('mixed timing').error.code,
                     'ERR_INVALID_STATE');
  assert.strictEqual(byName.get('start after record').error.code,
                     'ERR_INVALID_STATE');
  assert.strictEqual(byName.get('end after record').error.code,
                     'ERR_INVALID_STATE');
  assert.strictEqual(byName.get('duplicate record').error.code,
                     'ERR_INVALID_STATE');
  assert.strictEqual(byName.get('reentrant record').error.code,
                     'ERR_INVALID_STATE');
  assert.strictEqual(byName.get('uncloneable detail').error.name,
                     'DataCloneError');
  assert.strictEqual(byName.get('uncloneable diagnostic message').error.name,
                     'DataCloneError');
  assert.strictEqual(byName.get('invalid diagnostic level').error.code,
                     'ERR_INVALID_ARG_VALUE');
  assert.strictEqual(byName.get('uncloneable diagnostic detail').error.name,
                     'DataCloneError');
  assert.match(byName.get('caught contract violation').error.message,
               /violated the context method contract/);
  assert.match(byName.get('caught diagnostic violation').error.message,
               /violated the context method contract/);
})().then(common.mustCall());
