test(function() {
  assert_throws_js(TypeError, function() { self.performance.mark("mark1", 123); }, "Number passed as a dict argument should cause type-error.")
}, "Number should be rejected as the mark-options.")

test(function() {
  assert_throws_js(TypeError, function() { self.performance.mark("mark1", NaN); }, "NaN passed as a dict argument should cause type-error.")
}, "NaN should be rejected as the mark-options.")

test(function() {
  assert_throws_js(TypeError, function() { self.performance.mark("mark1", Infinity); }, "Infinity passed as a dict argument should cause type-error.")
}, "Infinity should be rejected as the mark-options.")

test(function() {
  assert_throws_js(TypeError, function() { self.performance.mark("mark1", "string"); }, "String passed as a dict argument should cause type-error.")
}, "String should be rejected as the mark-options.")
