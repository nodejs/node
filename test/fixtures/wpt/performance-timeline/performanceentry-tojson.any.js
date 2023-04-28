test(() => {
  performance.mark('markName');
  performance.measure('measureName');

  const entries = performance.getEntries();
  const performanceEntryKeys = [
    'name',
    'entryType',
    'startTime',
    'duration'
  ];
  for (let i = 0; i < entries.length; ++i) {
    assert_equals(typeof(entries[i].toJSON), 'function');
    const json = entries[i].toJSON();
    assert_equals(typeof(json), 'object');
    for (const key of performanceEntryKeys) {
      assert_equals(json[key], entries[i][key],
        `entries[${i}].toJSON().${key} should match entries[${i}].${key}`);
    }
  }
}, 'Test toJSON() in PerformanceEntry');
