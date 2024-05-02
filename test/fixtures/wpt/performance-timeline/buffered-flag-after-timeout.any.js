async_test(t => {
  performance.mark('foo');
  t.step_timeout(() => {
    // After a timeout, PerformanceObserver should still receive entry if using the buffered flag.
    new PerformanceObserver(t.step_func_done(list => {
      const entries = list.getEntries();
      assert_equals(entries.length, 1, 'There should be 1 mark entry.');
      assert_equals(entries[0].entryType, 'mark');
    })).observe({type: 'mark', buffered: true});
  }, 100);
}, 'PerformanceObserver with buffered flag sees entry after timeout');
