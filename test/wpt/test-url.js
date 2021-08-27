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

runner.setScriptModifier((obj) => {
  if (obj.filename.includes('toascii.window.js')) {
    // `a` and `area` in `toascii.window.js` is for testing `Element` that
    // created via `document.createElement`. So we need to ignore them and just
    // test `URL`.
    obj.code = obj.code.replace(/\["url", "a", "area"\]/, '[ "url" ]');
  }
});
runner.pretendGlobalThisAs('Window');
runner.runJsTests();
