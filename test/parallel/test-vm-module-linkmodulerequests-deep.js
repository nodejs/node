'use strict';

// Flags: --experimental-vm-modules --js-source-phase-imports

require('../common');

const assert = require('assert');

const { SourceTextModule } = require('vm');
const test = require('node:test');

test('deep linking', async function depth() {
  const foo = new SourceTextModule('export default 5');
  foo.linkRequests([]);
  foo.instantiate();

  function getProxy(parentName, parentModule) {
    const mod = new SourceTextModule(`
      import ${parentName} from '${parentName}';
      export default ${parentName};
    `);
    mod.linkRequests([parentModule]);
    mod.instantiate();
    return mod;
  }

  const bar = getProxy('foo', foo);
  const baz = getProxy('bar', bar);
  const barz = getProxy('baz', baz);

  await barz.evaluate();

  assert.strictEqual(barz.namespace.default, 5);
});
