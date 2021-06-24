test(function () {
  self.performance.mark('mark');
  var mark_entry = self.performance.getEntriesByName('mark')[0];

  assert_equals(Object.prototype.toString.call(mark_entry), '[object PerformanceMark]', 'Class name of mark entry should be PerformanceMark.');
}, "Validate the user timing entry type PerformanceMark");

test(function () {
  self.performance.measure('measure');
  var measure_entry = self.performance.getEntriesByName('measure')[0];

  assert_equals(Object.prototype.toString.call(measure_entry), '[object PerformanceMeasure]', 'Class name of measure entry should be PerformanceMeasure.');
}, "Validate the user timing entry type PerformanceMeasure");
