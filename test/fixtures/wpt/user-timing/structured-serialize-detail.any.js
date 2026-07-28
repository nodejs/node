test(function() {
  performance.clearMarks();
  const detail = { randomInfo: 123 }
  const markEntry = new PerformanceMark("A", { detail });
  assert_equals(markEntry.detail.randomInfo, detail.randomInfo);
  assert_not_equals(markEntry.detail, detail);
}, "The detail property in the mark constructor should be structured-clone.");

test(function() {
  performance.clearMarks();
  const detail = { randomInfo: 123 }
  const markEntry = performance.mark("A", { detail });
  assert_equals(markEntry.detail.randomInfo, detail.randomInfo);
  assert_not_equals(markEntry.detail, detail);
}, "The detail property in the mark method should be structured-clone.");

test(function() {
  performance.clearMarks();
  const markEntry = performance.mark("A");
  assert_equals(markEntry.detail, null);
}, "When accessing detail from a mark entry and the detail is not provided, just return a null value.");

test(function() {
  performance.clearMarks();
  const detail = { unserializable: Symbol() };
  assert_throws_dom("DataCloneError", ()=>{
    new PerformanceMark("A", { detail });
  }, "Trying to structured-serialize a Symbol.");
}, "Mark: Throw an exception when the detail property cannot be structured-serialized.");

test(function() {
  performance.clearMeasures();
  const detail = { randomInfo: 123 }
  const measureEntry = performance.measure("A", { start: 0, detail });
  assert_equals(measureEntry.detail.randomInfo, detail.randomInfo);
  assert_not_equals(measureEntry.detail, detail);
}, "The detail property in the measure method should be structured-clone.");

test(function() {
  performance.clearMeasures();
  const detail = { randomInfo: 123 }
  const measureEntry = performance.measure("A", { start: 0, detail });
  assert_equals(measureEntry.detail, measureEntry.detail);
}, "The detail property in the measure method should be the same reference.");

test(function() {
  performance.clearMeasures();
  const measureEntry = performance.measure("A");
  assert_equals(measureEntry.detail, null);
}, "When accessing detail from a measure entry and the detail is not provided, just return a null value.");

test(function() {
  performance.clearMeasures();
  const detail = { unserializable: Symbol() };
  assert_throws_dom("DataCloneError", ()=>{
    performance.measure("A", { start: 0, detail });
  }, "Trying to structured-serialize a Symbol.");
}, "Measure: Throw an exception when the detail property cannot be structured-serialized.");

test(function() {
  const bar = { 1: 2 };
  const detail = { foo: 1, bar };
  const mark = performance.mark("m", { detail });
  detail.foo = 2;
  assert_equals(mark.detail.foo, 1);
}, "The detail object is cloned when passed to mark API.");
