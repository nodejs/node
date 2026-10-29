function expect_navigation_preload_state(state, enabled, header, desc) {
  assert_equals(Object.keys(state).length, 2, desc + ': # of keys');
  assert_equals(state.enabled, enabled, desc + ': enabled');
  assert_equals(state.headerValue, header, desc + ': header');
}
