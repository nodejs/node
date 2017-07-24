'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

const { MakeMirror, MakeMirrorSerializer } = vm.runInDebugContext('Debug');
const proxy = new Proxy({ x: 1, y: 2 }, { get: Reflect.get });
const mirror = MakeMirror(proxy, /* transient */ true);

assert.strictEqual(mirror.isProxy(), true);
assert.strictEqual(mirror.toText(), '#<Proxy>');
assert.strictEqual(mirror.value(), proxy);

const serializer = MakeMirrorSerializer(/* details */ true);
const serialized = serializer.serializeValue(mirror);
assert.deepStrictEqual(Object.keys(serialized).sort(), ['text', 'type']);
assert.strictEqual(serialized.type, 'proxy');
assert.strictEqual(serialized.text, '#<Proxy>');
