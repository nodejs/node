'use strict';

// Flags: --expose-internals

require('../common');
const { WPTRunner } = require('../common/wpt');
const { internalBinding } = require('internal/test/binding');
const { DOMException } = internalBinding('messaging');
const runner = new WPTRunner('url');

// Copy global descriptors from the global object
runner.copyGlobalsFromObject(global, ['URL', 'URLSearchParams']);
// Needed by urlsearchparams-constructor.any.js
runner.defineGlobal('DOMException', {
  get() {
    return DOMException;
  }
});

runner.runJsTests();
