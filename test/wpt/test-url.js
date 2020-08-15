'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('url');

// Needed to access to DOMException.
runner.setFlags(['--expose-internals']);

// DOMException is needed by urlsearchparams-constructor.any.js
runner.setInitScript(`
  const { internalBinding } = require('internal/test/binding');
  const { DOMException } = internalBinding('messaging');
  global.DOMException = DOMException;
`);

runner.runJsTests();
