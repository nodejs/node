'use strict';

const smalloc = require('internal/smalloc');
const deprecate = require('util').deprecate;

exports.alloc =
    deprecate(smalloc.alloc, 'smalloc.alloc: Deprecated, use typed arrays');

exports.copyOnto =
    deprecate(smalloc.copyOnto,
              'smalloc.copyOnto: Deprecated, use typed arrays');

exports.dispose =
    deprecate(smalloc.dispose,
              'smalloc.dispose: Deprecated, use typed arrays');

exports.hasExternalData =
    deprecate(smalloc.hasExternalData,
              'smalloc.hasExternalData: Deprecated, use typed arrays');

Object.defineProperty(exports, 'kMaxLength', {
  enumerable: true, value: smalloc.kMaxLength, writable: false
});

Object.defineProperty(exports, 'Types', {
  enumerable: true, value: Object.freeze(smalloc.Types), writable: false
});
