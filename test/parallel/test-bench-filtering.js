// Flags: --no-warnings
'use strict';

const common = require('../common');
const { completeSample } = require('../common/bench');
const assert = require('assert');
const { bench, run, suite } = require('node:bench');

const calls = [];

function complete(name) {
  return (b) => {
    calls.push(name);
    completeSample(b);
  };
}

suite('selected', { only: true }, () => {
  bench('included', { samples: 1 }, complete('included'));
  bench.skip('explicitly skipped', { samples: 1 },
             common.mustNotCall());
  bench('pattern filtered', { samples: 1 },
        common.mustNotCall());
});
bench('only filtered', { samples: 1 }, common.mustNotCall());

const results = [];
const plans = [];
const namePattern = /^selected (included|explicitly skipped)$/;
let patternMutated = false;
const stream = run({ namePattern });
stream.on('bench:plan', common.mustCall((plan) => {
  assert.deepStrictEqual(calls, []);
  plans.push(plan);
  if (!patternMutated) {
    patternMutated = true;
    namePattern.compile('only filtered');
  }
}, 4));
stream.on('bench:complete', (result) => results.push(result));
stream.on('end', common.mustCall(() => {
  assert.deepStrictEqual(calls, ['included']);
  assert.strictEqual(results.length, 4);

  const byName = new Map(results.map((result) => [result.name, result]));
  assert.strictEqual(byName.get('included').error, undefined);
  assert.strictEqual(byName.get('explicitly skipped').skip, true);
  assert.strictEqual(byName.get('pattern filtered').skip, 'name pattern');
  assert.strictEqual(byName.get('only filtered').skip, 'only');
  assert.deepStrictEqual(plans.map(({ name, selected, skip }) => ({
    name,
    selected,
    skip,
  })), [
    { name: 'included', selected: true, skip: undefined },
    { name: 'explicitly skipped', selected: false, skip: true },
    { name: 'pattern filtered', selected: false, skip: 'name pattern' },
    { name: 'only filtered', selected: false, skip: 'only' },
  ]);
}));
stream.resume();
