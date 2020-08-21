'use strict';
const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

const beforeSetupHandler =
    common.mustNotCall('called before v8.emitCodeGenFromStringEvent()');

process.on('codeGenerationFromString', beforeSetupHandler);

const item = { foo: 0 };
eval('++item.foo');
process.off('codeGenerationFromString', beforeSetupHandler);

assert.strictEqual(item.foo, 1);

v8.emitCodeGenFromStringEvent();
process.on('codeGenerationFromString', common.mustCall((code) => {
  assert.strictEqual(code, 'item.foo++');
}));

eval('item.foo++');
assert.strictEqual(item.foo, 2);
