// META: script=resources/user-timing-helper.js

function endTime(entry) {
  return entry.startTime + entry.duration;
}

test(function() {
  performance.clearMarks();
  performance.clearMeasures();
  const markEntry = performance.mark("mark", {startTime: 123});
  const measureEntry = performance.measure("A", undefined, "mark");
  assert_equals(measureEntry.startTime, 0);
  assert_equals(endTime(measureEntry), markEntry.startTime);
}, "When the end mark is given and the start is unprovided, the end time of the measure entry should be the end mark's time, the start time should be 0.");

test(function() {
  performance.clearMarks();
  performance.clearMeasures();
  const markEntry = performance.mark("mark", {startTime: 123});
  const endMin = Number(performance.now().toFixed(2));
  const measureEntry = performance.measure("A", "mark", undefined);
  const endMax = Number(performance.now().toFixed(2));
  assert_equals(measureEntry.startTime, markEntry.startTime);
  assert_greater_than_equal(Number(endTime(measureEntry).toFixed(2)), endMin);
  assert_greater_than_equal(endMax, Number(endTime(measureEntry).toFixed(2)));
}, "When the start mark is given and the end is unprovided, the start time of the measure entry should be the start mark's time, the end should be now.");

test(function() {
  performance.clearMarks();
  performance.clearMeasures();
  const markEntry = performance.mark("mark", {startTime: 123});
  const measureEntry = performance.measure("A", "mark", "mark");
  assert_equals(endTime(measureEntry), markEntry.startTime);
  assert_equals(measureEntry.startTime, markEntry.startTime);
}, "When start and end mark are both given, the start time and end time of the measure entry should be the the marks' time, repectively");
