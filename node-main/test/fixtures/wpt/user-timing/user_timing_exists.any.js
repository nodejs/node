test(function() {
    assert_not_equals(self.performance.mark, undefined);
}, "self.performance.mark is defined.");
test(function() {
    assert_not_equals(self.performance.clearMarks, undefined);
}, "self.performance.clearMarks is defined.");
test(function() {
    assert_not_equals(self.performance.measure, undefined);
}, "self.performance.measure is defined.");
test(function() {
    assert_not_equals(self.performance.clearMeasures, undefined);
}, "self.performance.clearMeasures is defined.");
