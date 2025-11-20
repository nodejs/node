// META: global=shadowrealm

test(t => {
  assert_not_own_property(AbortSignal, "timeout", "AbortSignal does not have a 'timeout' property");
}, "AbortSignal.timeout() is not exposed in ShadowRealm");
