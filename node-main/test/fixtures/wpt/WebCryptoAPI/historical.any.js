// META: global=window,dedicatedworker,sharedworker
//
// Do not run this in a service worker as that's always in a secure context

test(() => {
  assert_equals(self.crypto.subtle, undefined);
  assert_false("subtle" in self.crypto);
}, "Non-secure context window does not have access to crypto.subtle");

test(() => {
  assert_equals(self.SubtleCrypto, undefined);
  assert_false("SubtleCrypto" in self);
}, "Non-secure context window does not have access to SubtleCrypto")

test(() => {
  assert_equals(self.CryptoKey, undefined);
  assert_false("CryptoKey" in self);
}, "Non-secure context window does not have access to CryptoKey")
