'use strict';

const common = require('../common');
common.requireFlags(['--expose-internals']);

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('url');

// Copy global descriptors from the global object
runner.copyGlobalsFromObject(global, ['URL', 'URLSearchParams']);
// Needed by urlsearchparams-constructor.any.js
runner.defineGlobal('DOMException', {
  get() {
    return require('internal/domexception');
  }
});

runner.runJsTests();
