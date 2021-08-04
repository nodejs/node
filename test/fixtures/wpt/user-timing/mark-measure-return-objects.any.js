async_test(function (t) {
  self.performance.clearMeasures();
  const measure = self.performance.measure("measure1");
  assert_true(measure instanceof PerformanceMeasure);
  t.done();
}, "L3: performance.measure(name) should return an entry.");

async_test(function (t) {
  self.performance.clearMeasures();
  const measure = self.performance.measure("measure2",
      { start: 12, end: 23 });
  assert_true(measure instanceof PerformanceMeasure);
  t.done();
}, "L3: performance.measure(name, param1) should return an entry.");

async_test(function (t) {
  self.performance.clearMeasures();
  self.performance.mark("1");
  self.performance.mark("2");
  const measure = self.performance.measure("measure3", "1", "2");
  assert_true(measure instanceof PerformanceMeasure);
  t.done();
}, "L3: performance.measure(name, param1, param2) should return an entry.");

async_test(function (t) {
  self.performance.clearMarks();
  const mark = self.performance.mark("mark1");
  assert_true(mark instanceof PerformanceMark);
  t.done();
}, "L3: performance.mark(name) should return an entry.");

async_test(function (t) {
  self.performance.clearMarks();
  const mark = self.performance.mark("mark2", { startTime: 34 });
  assert_true(mark instanceof PerformanceMark);
  t.done();
}, "L3: performance.mark(name, param) should return an entry.");
