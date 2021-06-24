test(() => {
  if (typeof PerformanceObserver.supportedEntryTypes === "undefined")
    assert_unreached("supportedEntryTypes is not supported.");
  const types = PerformanceObserver.supportedEntryTypes;
  assert_greater_than(types.length, 0,
    "There should be at least one entry in supportedEntryTypes.");
  for (let i = 1; i < types.length; i++) {
    assert_true(types[i-1] < types[i],
      "The strings '" + types[i-1] + "' and '" + types[i] +
      "' are repeated or they are not in alphabetical order.")
  }
}, "supportedEntryTypes exists and returns entries in alphabetical order");

test(() => {
  if (typeof PerformanceObserver.supportedEntryTypes === "undefined")
    assert_unreached("supportedEntryTypes is not supported.");
  assert_true(PerformanceObserver.supportedEntryTypes ===
      PerformanceObserver.supportedEntryTypes);
}, "supportedEntryTypes caches result");
