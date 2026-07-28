async_test(t => {
  performance.clearResourceTimings();
  // First observer creates second in callback to ensure the entry has been dispatched by the time
  // the second observer begins observing.
  new PerformanceObserver(() => {
    // Second observer requires 'buffered: true' to see an entry.
    new PerformanceObserver(t.step_func_done(list => {
      const entries = list.getEntries();
      assert_equals(entries.length, 1, 'There should be 1 resource entry.');
      assert_equals(entries[0].entryType, 'resource');
      assert_greater_than(entries[0].startTime, 0);
      assert_greater_than(entries[0].responseEnd, entries[0].startTime);
      assert_greater_than(entries[0].duration, 0);
      assert_true(entries[0].name.endsWith('resources/empty.js'));
    })).observe({'type': 'resource', buffered: true});
  }).observe({'entryTypes': ['resource']});
  fetch('resources/empty.js');
}, 'PerformanceObserver with buffered flag sees previous resource entries.');
