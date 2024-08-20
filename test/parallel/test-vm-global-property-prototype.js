'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const outerProto = {
  onOuterProto: 'onOuterProto',
  bothProto: 'onOuterProto',
};
function onOuterProtoGetter() {
  return 'onOuterProtoGetter';
}
Object.defineProperties(outerProto, {
  onOuterProtoGetter: {
    get: onOuterProtoGetter,
  },
  bothProtoGetter: {
    get: onOuterProtoGetter,
  },
  0: {
    value: 'onOuterProtoIndexed',
    writable: false,
    enumerable: false,
    configurable: true,
  },
  3: {
    value: 'onOuterProtoIndexed',
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

// Creating a new intermediate proto to mimic the
// window -> Window.prototype -> EventTarget.prototype chain in JSDom.
const sandboxProto = {
  __proto__: outerProto,
};

const sandbox = {
  __proto__: sandboxProto,
  onSelf: 'onSelf',
};

function onSelfGetter() {
  return 'onSelfGetter';
}
Object.defineProperties(sandbox, {
  onSelfGetter: {
    get: onSelfGetter,
  },
  1: {
    value: 'onSelfIndexed',
    writable: false,
    enumerable: false,
    configurable: true,
  }
});

const ctx = vm.createContext(sandbox);

const result = vm.runInContext(`
Object.prototype.onInnerProto = 'onInnerProto';
Object.defineProperties(Object.prototype, {
  onInnerProtoGetter: {
    get() {
      return 'onInnerProtoGetter';
    },
  },
  2: {
    value: 'onInnerProtoIndexed',
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

// Override outer proto properties
Object.prototype.bothProto = 'onInnerProto';
Object.defineProperties(Object.prototype, {
  bothProtoGetter: {
    get() {
      return 'onInnerProtoGetter';
    },
  },
  3: {
    value: 'onInnerProtoIndexed',
    writable: false,
    enumerable: false,
    configurable: true,
  },
});

const resultHasOwn = {
  onSelf: Object.hasOwn(this, 'onSelf'),
  onSelfGetter: Object.hasOwn(this, 'onSelfGetter'),
  onSelfIndexed: Object.hasOwn(this, 1),
  onOuterProto: Object.hasOwn(this, 'onOuterProto'),
  onOuterProtoGetter: Object.hasOwn(this, 'onOuterProtoGetter'),
  onOuterProtoIndexed: Object.hasOwn(this, 0),
  onInnerProto: Object.hasOwn(this, 'onInnerProto'),
  onInnerProtoGetter: Object.hasOwn(this, 'onInnerProtoGetter'),
  onInnerProtoIndexed: Object.hasOwn(this, 2),
  bothProto: Object.hasOwn(this, 'bothProto'),
  bothProtoGetter: Object.hasOwn(this, 'bothProtoGetter'),
  bothProtoIndexed: Object.hasOwn(this, 3),
};

const getDesc = (prop) => Object.getOwnPropertyDescriptor(this, prop);
const resultDesc = {
  onSelf: getDesc('onSelf'),
  onSelfGetter: getDesc('onSelfGetter'),
  onSelfIndexed: getDesc(1),
  onOuterProto: getDesc('onOuterProto'),
  onOuterProtoGetter: getDesc('onOuterProtoGetter'),
  onOuterProtoIndexed: getDesc(0),
  onInnerProto: getDesc('onInnerProto'),
  onInnerProtoGetter: getDesc('onInnerProtoGetter'),
  onInnerProtoIndexed: getDesc(2),
  bothProto: getDesc('bothProto'),
  bothProtoGetter: getDesc('bothProtoGetter'),
  bothProtoIndexed: getDesc(3),
};
const resultIn = {
  onSelf: 'onSelf' in this,
  onSelfGetter: 'onSelfGetter' in this,
  onSelfIndexed: 1 in this,
  onOuterProto: 'onOuterProto' in this,
  onOuterProtoGetter: 'onOuterProtoGetter' in this,
  onOuterProtoIndexed: 0 in this,
  onInnerProto: 'onInnerProto' in this,
  onInnerProtoGetter: 'onInnerProtoGetter' in this,
  onInnerProtoIndexed: 2 in this,
  bothProto: 'bothProto' in this,
  bothProtoGetter: 'bothProtoGetter' in this,
  bothProtoIndexed: 3 in this,
};
const resultValue = {
  onSelf: this.onSelf,
  onSelfGetter: this.onSelfGetter,
  onSelfIndexed: this[1],
  onOuterProto: this.onOuterProto,
  onOuterProtoGetter: this.onOuterProtoGetter,
  onOuterProtoIndexed: this[0],
  onInnerProto: this.onInnerProto,
  onInnerProtoGetter: this.onInnerProtoGetter,
  onInnerProtoIndexed: this[2],
  bothProto: this.bothProto,
  bothProtoGetter: this.bothProtoGetter,
  bothProtoIndexed: this[3],
};
({
  resultHasOwn,
  resultDesc,
  resultIn,
  resultValue,
});
`, ctx);

// eslint-disable-next-line no-restricted-properties
assert.deepEqual(result, {
  resultHasOwn: {
    onSelf: true,
    onSelfGetter: true,
    onSelfIndexed: true,

    // The following results should be false in terms of "normal" JavaScript
    // objects. However, `in` operator only looks up properties on the inner
    // prototype chain, the interceptor has to lookup the outer prototype chain
    // to maintain compatibility.
    // FIXME(legendecas): https://github.com/nodejs/node/issues/54436
    onOuterProto: true,
    onOuterProtoGetter: true,
    onOuterProtoIndexed: true,
    onInnerProto: true,
    onInnerProtoGetter: true,
    onInnerProtoIndexed: true,
    bothProto: true,
    bothProtoGetter: true,
    bothProtoIndexed: true,
  },
  resultDesc: {
    onSelf: { value: 'onSelf', writable: true, enumerable: true, configurable: true },
    onSelfGetter: { get: onSelfGetter, set: undefined, enumerable: false, configurable: false },
    onSelfIndexed: { value: 'onSelfIndexed', writable: false, enumerable: false, configurable: true },
    onOuterProto: undefined,
    onOuterProtoGetter: undefined,
    onOuterProtoIndexed: undefined,
    onInnerProto: undefined,
    onInnerProtoGetter: undefined,
    onInnerProtoIndexed: undefined,
    bothProto: undefined,
    bothProtoGetter: undefined,
    bothProtoIndexed: undefined,
  },
  resultIn: {
    onSelf: true,
    onSelfGetter: true,
    onSelfIndexed: true,
    onOuterProto: true,
    onOuterProtoGetter: true,
    onOuterProtoIndexed: true,
    onInnerProto: true,
    onInnerProtoGetter: true,
    onInnerProtoIndexed: true,
    bothProto: true,
    bothProtoGetter: true,
    bothProtoIndexed: true,
  },
  resultValue: {
    onSelf: 'onSelf',
    onSelfGetter: 'onSelfGetter',
    onSelfIndexed: 'onSelfIndexed',
    onOuterProto: 'onOuterProto',
    onOuterProtoGetter: 'onOuterProtoGetter',
    onOuterProtoIndexed: 'onOuterProtoIndexed',
    onInnerProto: 'onInnerProto',
    onInnerProtoGetter: 'onInnerProtoGetter',
    onInnerProtoIndexed: 'onInnerProtoIndexed',
    bothProto: 'onOuterProto',
    bothProtoGetter: 'onOuterProtoGetter',
    bothProtoIndexed: 'onOuterProtoIndexed',
  }
});
