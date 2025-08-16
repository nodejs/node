'use strict';

// Flags: --experimental-vm-modules --js-source-phase-imports

require('../common');
const assert = require('node:assert');
const {
  SourceTextModule,
} = require('node:vm');
const test = require('node:test');

test('SourceTextModule.moduleRequests should return module requests', (t) => {
  const m = new SourceTextModule(`
    import { foo } from './foo.js';
    import * as FooDuplicate from './foo.js';
    import { bar } from './bar.json' with { type: 'json' };
    import * as BarDuplicate from './bar.json' with { type: 'json' };
    import { quz } from './quz.js' with { attr1: 'quz' };
    import * as QuzDuplicate from './quz.js' with { attr1: 'quz' };
    import { quz as quz2 } from './quz.js' with { attr2: 'quark', attr3: 'baz' };
    import * as Quz2Duplicate from './quz.js' with { attr2: 'quark', attr3: 'baz' };
    import source Module from './source-module';
    import source Module2 from './source-module';
    import * as SourceModule from './source-module';
    export { foo, bar, quz, quz2 };
  `);

  const requests = m.moduleRequests;
  assert.strictEqual(requests.length, 6);
  assert.deepStrictEqual(requests[0], {
    __proto__: null,
    specifier: './foo.js',
    attributes: {
      __proto__: null,
    },
    phase: 'evaluation',
  });
  assert.deepStrictEqual(requests[1], {
    __proto__: null,
    specifier: './bar.json',
    attributes: {
      __proto__: null,
      type: 'json'
    },
    phase: 'evaluation',
  });
  assert.deepStrictEqual(requests[2], {
    __proto__: null,
    specifier: './quz.js',
    attributes: {
      __proto__: null,
      attr1: 'quz',
    },
    phase: 'evaluation',
  });
  assert.deepStrictEqual(requests[3], {
    __proto__: null,
    specifier: './quz.js',
    attributes: {
      __proto__: null,
      attr2: 'quark',
      attr3: 'baz',
    },
    phase: 'evaluation',
  });
  assert.deepStrictEqual(requests[4], {
    __proto__: null,
    specifier: './source-module',
    attributes: {
      __proto__: null,
    },
    phase: 'source',
  });
  assert.deepStrictEqual(requests[5], {
    __proto__: null,
    specifier: './source-module',
    attributes: {
      __proto__: null,
    },
    phase: 'evaluation',
  });

  // Check the deprecated dependencySpecifiers property.
  // The dependencySpecifiers items are not unique.
  assert.deepStrictEqual(m.dependencySpecifiers, [
    './foo.js',
    './bar.json',
    './quz.js',
    './quz.js',
    './source-module',
    './source-module',
  ]);
});

test('SourceTextModule.moduleRequests items are frozen', (t) => {
  const m = new SourceTextModule(`
    import { foo } from './foo.js';
  `);

  const requests = m.moduleRequests;
  assert.strictEqual(requests.length, 1);

  const propertyNames = ['specifier', 'attributes', 'phase'];
  for (const propertyName of propertyNames) {
    assert.throws(() => {
      requests[0][propertyName] = 'bar.js';
    }, {
      name: 'TypeError',
    });
  }
  assert.throws(() => {
    requests[0].attributes.type = 'json';
  }, {
    name: 'TypeError',
  });
});
