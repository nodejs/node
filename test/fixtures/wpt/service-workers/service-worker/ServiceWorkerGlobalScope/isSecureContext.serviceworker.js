importScripts("/resources/testharness.js");

test(() => {
    assert_true(self.isSecureContext, true);
}, "isSecureContext");
