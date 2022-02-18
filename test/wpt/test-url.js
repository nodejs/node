'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('url');

runner.setScriptModifier((obj) => {
  if (obj.filename.includes('toascii.window.js')) {
    // `a` and `area` in `toascii.window.js` is for testing `Element` that
    // created via `document.createElement`. So we need to ignore them and just
    // test `URL`.
    obj.code = obj.code.replace(/\["url", "a", "area"\]/, '[ "url" ]');
  } else if (typeof FormData === 'undefined' &&
      obj.filename.includes('urlsearchparams-constructor.any.js')) {
    // TODO(XadillaX): Remove this `else if` after `FormData` is supported.

    // Ignore test named `URLSearchParams constructor, FormData.` because we do
    // not have `FormData`.
    obj.code = obj.code.replace(
      /('URLSearchParams constructor, object\.'\);[\w\W]+)test\(function\(\) {[\w\W]*?}, 'URLSearchParams constructor, FormData\.'\);/,
      '$1');
  }
});
runner.pretendGlobalThisAs('Window');
runner.runJsTests();
