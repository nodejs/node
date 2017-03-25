'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

const { MakeMirror } = vm.runInDebugContext('Debug');
const proxy = new Proxy({ x: 1, y: 2 }, { get: Reflect.get });
const mirror = MakeMirror(proxy);

assert.ok(mirror.protoObject().value());
assert.strictEqual(mirror.className(), 'Object');
assert.strictEqual(mirror.constructorFunction().value(), undefined);
assert.strictEqual(mirror.prototypeObject().value(), undefined);
assert.strictEqual(mirror.hasNamedInterceptor(), false);
assert.strictEqual(mirror.hasIndexedInterceptor(), false);
assert.strictEqual(mirror.referencedBy(1).length, 0);
assert.strictEqual(mirror.toText(), '#<Object>');

const propertyNames = mirror.propertyNames();
const ThatArray = propertyNames.constructor;
assert.deepStrictEqual(propertyNames, ThatArray.from(['x', 'y']));

const properties = mirror.properties();
assert.strictEqual(properties.length, 2);
assert.strictEqual(properties[0].name(), 'x');
// |undefined| is not quite right but V8 cannot retrieve the correct values
// without invoking the handler and that wouldn't be safe when the script is
// currently being debugged.  Yes, it's odd that it does return property names
// but it retrieves them without calling the handler.
assert.strictEqual(properties[0].value().value(), undefined);
assert.strictEqual(properties[1].name(), 'y');
assert.strictEqual(properties[1].value().value(), undefined);
