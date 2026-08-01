// META: global=worker

test(function() {
  assert_equals(self, self);
}, 'self === self');

test(function() {
  assert_true(self instanceof WorkerGlobalScope);
}, 'self instanceof WorkerGlobalScope');

test(function() {
  assert_true('self' in self);
}, '\'self\' in self');

test(function() {
  var x = self;
  self = 1;
  assert_equals(self, x);
}, 'self = 1');
