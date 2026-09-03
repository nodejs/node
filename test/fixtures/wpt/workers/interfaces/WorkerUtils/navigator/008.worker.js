"use strict";
importScripts("/resources/testharness.js");
test(function () {
  for (const x in navigator) {
    // skip functions, as they are settable
    if (typeof navigator[x] === "function") continue;
    assert_throws_js(TypeError, () => {
      navigator[x] = "";
    }, `navigator.${x} is read-only`);
  }
}, "navigator properties are read-only");
done();
