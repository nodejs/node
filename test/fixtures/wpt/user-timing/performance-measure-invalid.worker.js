importScripts("/resources/testharness.js");

test(() => {
  assert_throws_js(TypeError, () => {
    performance.measure('name', 'navigationStart', 'navigationStart');
  });
}, "When converting 'navigationStart' to a timestamp, the global object has to be a Window object.");

done();
