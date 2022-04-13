  test(function () {
    assert_equals(typeof self.performance, "object");
    assert_equals(typeof self.performance.getEntriesByType, "function");

    self.performance.mark("mark1");
    self.performance.measure("measure1");

    const type = [
      'mark',
      'measure',
    ];
    type.forEach(function(entryType) {
      if (PerformanceObserver.supportedEntryTypes.includes(entryType)) {
        const entryTypeUpperCased = entryType.toUpperCase();
        const entryTypeCapitalized = entryType[0].toUpperCase() + entryType.substring(1);
        const lowerList = self.performance.getEntriesByType(entryType);
        const upperList = self.performance.getEntriesByType(entryTypeUpperCased);
        const mixedList = self.performance.getEntriesByType(entryTypeCapitalized);

        assert_greater_than(lowerList.length, 0, "Entries exist");
        assert_equals(upperList.length, 0, "getEntriesByType('" + entryTypeCapitalized + "').length");
        assert_equals(mixedList.length, 0, "getEntriesByType('" + entryTypeCapitalized + "').length");
      }
    });
  }, "getEntriesByType values are case sensitive");
