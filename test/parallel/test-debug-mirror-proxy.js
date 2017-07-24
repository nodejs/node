'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

const { MakeMirror } = vm.runInDebugContext('Debug');
const proxy = new Proxy({ x: 1, y: 2 }, { get: Reflect.get });
const mirror = MakeMirror(proxy, /* transient */ true);

assert.strictEqual(mirror.protoObject().value(), undefined);
assert.strictEqual(mirror.className(), 'Object');
assert.strictEqual(mirror.constructorFunction().value(), undefined);
assert.strictEqual(mirror.prototypeObject().value(), undefined);
assert.strictEqual(mirror.hasNamedInterceptor(), false);
assert.strictEqual(mirror.hasIndexedInterceptor(), false);
assert.strictEqual(mirror.referencedBy(1).length, 0);
assert.strictEqual(mirror.toText(), '#<Object>');

const propertyNames = mirror.propertyNames();
const DebugContextArray = propertyNames.constructor;
assert.deepStrictEqual(propertyNames, DebugContextArray.from(['x', 'y']));

const properties = mirror.properties();
assert.strictEqual(properties.length, 2);
// UndefinedMirror because V8 cannot retrieve the values without invoking
// the handler.  Could be turned a PropertyMirror but mirror.value() would
// still be an UndefinedMirror.  This seems Good Enough for now.
assert(properties[0].isUndefined());
assert(properties[1].isUndefined());
