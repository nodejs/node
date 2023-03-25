'use strict';

const { ObjectDefineProperty } = primordials;

let dot;
let spec;
let tap;

ObjectDefineProperty(module.exports, 'dot', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    if (dot === undefined) {
      dot = require('internal/test_runner/reporter/dot');
    }

    return dot;
  },
});
ObjectDefineProperty(module.exports, 'spec', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    if (spec === undefined) {
      spec = require('internal/test_runner/reporter/spec');
    }

    return spec;
  },
});
ObjectDefineProperty(module.exports, 'tap', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    if (tap === undefined) {
      tap = require('internal/test_runner/reporter/tap');
    }

    return tap;
  },
});
