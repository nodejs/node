// Flags: --expose-internals --harmony-shadow-realm
'use strict';
require('../common');
const assert = require('assert');

// Test internal/primordials_staging module
// Mutating globals must not affect builtins stored there.

const throwify = (name) => new Proxy({}, {
  get(_, prop) {
    assert.fail(`Tried to get ${prop} on mutated ${name}`);
  },
  set(_, prop) {
    assert.fail(`Tried to set ${prop} on mutated ${name}`);
  },
  apply() {
    assert.fail(`Tried to apply mutated ${name}`);
  },
  construct() {
    assert.fail(`Tried to construct mutated ${name}`);
  },
});

const { primordials } = require('internal/test/binding');
const primordialsStaging = require('internal/primordials_staging');
const originalGlobals = {};
const notGlobals = new Set([
  'Uint8ArrayFromBase64',
  'Uint8ArrayFromHex',
]);

for (const [ name, primordialStaging ] of Object.entries(primordialsStaging)) {
  // Sometimes conditional primordial might exist depending on environment
  // If they always exist, internals should get them from primordials, and they should be removed from staging
  if (name in primordials) {
    console.log(`${name} is already in primordials. Consider removing it from 'internal/primordials_staging'`);
  }

  // Do not treat missing builtins as error, because they might be disabled in this build
  if (primordialStaging === undefined) {
    console.log(`${name} is expected to exist. If it's new and not intentinally disabled, the test requires runtime flag.`);
    continue;
  }

  if (!notGlobals.has(name)) {
    originalGlobals[name] = globalThis[name];
    globalThis[name] = throwify(name);
    assert.strictEqual(originalGlobals[name], primordialStaging);
    assert.notStrictEqual(originalGlobals[name], primordials.globalThis[name]);
  }
}

if (primordialsStaging.Temporal !== undefined || globalThis.Temporal !== undefined) {
  // Safe Temporal must work
  {
    const { Temporal: { Instant, Now } } = primordialsStaging;
    assert.ok(Now.instant() instanceof Instant);
    assert.strictEqual(new Instant(123456789n).epochMilliseconds, 123);
  }

  // Global Temporal must break
  {
    assert.throws(() => {
      // eslint-disable-next-line no-unused-vars
      const { Temporal: { Instant, Now } } = globalThis;
    }, {
      code: 'ERR_ASSERTION',
    });
    assert.throws(() => {
      // eslint-disable-next-line no-unused-expressions
      Temporal.Now.instant() instanceof Temporal.Instant;
    }, {
      code: 'ERR_ASSERTION',
    });
    assert.throws(() => {
      // eslint-disable-next-line no-unused-expressions
      new Temporal.Instant(123456789n).epochMilliseconds;
    }, {
      code: 'ERR_ASSERTION',
    });
  }
}

if (primordialsStaging.Float16Array !== undefined || globalThis.Float16Array !== undefined) {
  // Safe Float16Array must work
  {
    const { Float16Array } = primordialsStaging;
    const buffer = new Float16Array([ 1, 2, 3 ]);
    assert.ok(buffer instanceof Float16Array);
    assert.strictEqual(buffer.byteLength, 6);
  }

  // Global Float16Array must not work
  {
    const { Float16Array } = globalThis;
    assert.throws(() => {
      new Float16Array([ 1, 2, 3 ]);
    }, {
      name: 'TypeError',
    });
  }
}
