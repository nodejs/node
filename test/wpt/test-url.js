'use strict';

const { skip } = require('../common');

if (process.config.variables.node_shared_ada) {
  skip('Different versions of Ada affect the WPT tests');
}

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('url');

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
