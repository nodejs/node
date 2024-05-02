importScripts('/resources/testharness.js');

test(() => {
  assert_false('FileReaderSync' in self);
}, '"FileReaderSync" should not be supported in service workers');
