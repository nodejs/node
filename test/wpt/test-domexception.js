'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('webidl/ecmascript-binding/es-exceptions');

runner.setFlags(['--expose-internals']);
runner.setInitScript(`
  const { internalBinding } = require('internal/test/binding');
  const { DOMException } = internalBinding('messaging');
  Object.defineProperty(global, 'DOMException', {
    writable: true,
    configurable: true,
    value: DOMException,
  });
`);

runner.runJsTests();
