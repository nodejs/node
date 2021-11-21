'use strict';
const assert = require('assert');

(async () => {
  const ns = await import('../exports-cases.js');
  assert.deepEqual(Object.keys(ns), ['?invalid', 'default', 'invalid identifier', 'isObject', 'package', 'z', 'π', '\u{d83c}\u{df10}']);
  assert.strictEqual(ns.π, 'yes');
  assert.strictEqual(typeof ns.default.isObject, 'undefined');
  assert.strictEqual(ns.default.π, 'yes');
  assert.strictEqual(ns.default.z, 'yes');
  assert.strictEqual(ns.default.package, 10);
  assert.strictEqual(ns.default['invalid identifier'], 'yes');
  assert.strictEqual(ns.default['?invalid'], 'yes');

  const ns2 = await import('../exports-cases2.js');
  assert.strictEqual(typeof ns2, 'object');
  assert.strictEqual(ns2.default, 'the default');
  assert.strictEqual(ns2.__esModule, true);
  assert.strictEqual(ns2.name, 'name');
  assert.deepEqual(Object.keys(ns2), ['__esModule', 'case2', 'default', 'name', 'pi']);

  const ns3 = await import('../exports-cases3.js')
  assert.deepEqual(Object.keys(ns3), ['__esModule', 'case2', 'default', 'name', 'pi']);
  assert.strictEqual(ns3.default, 'the default');
  assert.strictEqual(ns3.__esModule, true);
  assert.strictEqual(ns3.name, 'name');
  assert.strictEqual(ns3.case2, 'case2');

  console.log('ok');
})();
