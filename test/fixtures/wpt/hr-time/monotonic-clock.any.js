// The time values returned when calling the now method MUST be monotonically increasing and not subject to system clock adjustments or system clock skew.
test(function() {
  assert_true(self.performance.now() > 0, "self.performance.now() returns positive numbers");
}, "self.performance.now() returns a positive number");

// The difference between any two chronologically recorded time values returned from the now method MUST never be negative.
test(function() {
    var now1 = self.performance.now();
    var now2 = self.performance.now();
    assert_true((now2-now1) >= 0, "self.performance.now() difference is not negative");
  },
  "self.performance.now() difference is not negative"
);
