test(() => {
  assert_equals(window.clientInformation, window.navigator);
}, "window.clientInformation exists and equals window.navigator");

test(() => {
  window.clientInformation = 1;
  assert_equals(window.clientInformation, 1);
}, "window.clientInformation is Replaceable");
