test(function() {
  assert_true((self.performance !== undefined), "self.performance exists");
  assert_equals(typeof self.performance, "object", "self.performance is an object");
  assert_equals((typeof self.performance.now), "function", "self.performance.now() is a function");
  assert_equals(typeof self.performance.now(), "number", "self.performance.now() returns a number");
}, "self.performance.now() is a function that returns a number");

test(function() {
  assert_true(self.performance.now() > 0);
}, "self.performance.now() returns a positive number");

test(function() {
    var now1 = self.performance.now();
    var now2 = self.performance.now();
    assert_true((now2-now1) >= 0);
  }, "self.performance.now() difference is not negative");

async_test(function() {
  // Check whether the performance.now() method is close to Date() within 30ms (due to inaccuracies)
  var initial_hrt = self.performance.now();
  var initial_date = Date.now();
  this.step_timeout(function() {
    var final_hrt = self.performance.now();
    var final_date = Date.now();
    assert_approx_equals(final_hrt - initial_hrt, final_date - initial_date, 30, 'High resolution time value increased by approximately the same amount as time from date object');
    this.done();
  }, 2000);
}, 'High resolution time has approximately the right relative magnitude');

test(function() {
  var didHandle = false;
  self.performance.addEventListener("testEvent", function() {
    didHandle = true;
  }, { once: true} );
  self.performance.dispatchEvent(new Event("testEvent"));
  assert_true(didHandle, "Performance extends EventTarget, so event dispatching should work.");
}, "Performance interface extends EventTarget.");
