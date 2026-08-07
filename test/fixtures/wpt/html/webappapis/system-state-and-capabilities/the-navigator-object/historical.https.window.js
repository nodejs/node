[
  "registerContentHandler",
  "isProtocolHandlerRegistered",
  "isContentHandlerRegistered",
  "unregisterContentHandler"
].forEach(method => {
  test(() => {
    assert_false(method in self.navigator);
  }, method + "() is removed");
});

test(() => {
  let called = false;
  self.navigator.registerProtocolHandler("web+test", "%s", { toString: () => called = true });
  assert_false(called);
}, "registerProtocolHandler has no third argument");
