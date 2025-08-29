'use strict';

// Flags: --experimental-vm-modules

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
    export { foo, bar, quz, quz2 };
  `);

  const requests = m.moduleRequests;
  assert.strictEqual(requests.length, 4);
  assert.deepStrictEqual(requests[0], {
    __proto__: null,
    specifier: './foo.js',
    attributes: {
      __proto__: null,
    },
  });
  assert.deepStrictEqual(requests[1], {
    __proto__: null,
    specifier: './bar.json',
    attributes: {
      __proto__: null,
      type: 'json'
    },
  });
  assert.deepStrictEqual(requests[2], {
    __proto__: null,
    specifier: './quz.js',
    attributes: {
      __proto__: null,
      attr1: 'quz',
    },
  });
  assert.deepStrictEqual(requests[3], {
    __proto__: null,
    specifier: './quz.js',
    attributes: {
      __proto__: null,
      attr2: 'quark',
      attr3: 'baz',
    },
  });

  // Check the deprecated dependencySpecifiers property.
  // The dependencySpecifiers items are not unique.
  assert.deepStrictEqual(m.dependencySpecifiers, [
    './foo.js',
    './bar.json',
    './quz.js',
    './quz.js',
  ]);
});

test('SourceTextModule.moduleRequests items are frozen', (t) => {
  const m = new SourceTextModule(`
    import { foo } from './foo.js';
  `);

  const requests = m.moduleRequests;
  assert.strictEqual(requests.length, 1);

  const propertyNames = ['specifier', 'attributes'];
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
