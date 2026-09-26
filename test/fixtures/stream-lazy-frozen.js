'use strict';

// Public assert initializes stdio, which can load Duplex before it is tested.
const assert = require('internal/assert');
const stream = require('stream');

assert(!process.moduleLoadList.includes('NativeModule internal/streams/duplex'));
Object.freeze(stream);
Object.freeze(stream.Readable);
Object.freeze(stream.Readable.prototype);
Object.freeze(stream.Writable);
Object.freeze(stream.Writable.prototype);

const { Duplex } = stream;
assert(Object.getPrototypeOf(Duplex) === stream.Readable);
assert(Object.getPrototypeOf(Duplex.prototype) === stream.Readable.prototype);
assert(Duplex.prototype.destroy === stream.Writable.prototype.destroy);
Object.freeze(Duplex);
Object.freeze(Duplex.prototype);

const { Transform } = stream;
assert(Object.getPrototypeOf(Transform) === Duplex);
assert(Object.getPrototypeOf(Transform.prototype) === Duplex.prototype);
assert(Object.hasOwn(Transform.prototype, '_read'));
assert(Object.hasOwn(Transform.prototype, '_write'));
Object.freeze(Transform);
Object.freeze(Transform.prototype);

const { PassThrough } = stream;
assert(Object.getPrototypeOf(PassThrough) === Transform);
assert(Object.getPrototypeOf(PassThrough.prototype) === Transform.prototype);
assert(Object.hasOwn(PassThrough.prototype, '_transform'));

const descriptor = Object.getOwnPropertyDescriptor(stream.Readable.prototype, 'map');
assert(descriptor.value.name === 'map');
assert(descriptor.writable === false);
assert(descriptor.configurable === false);
