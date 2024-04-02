  test(function () {
    assert_equals(typeof self.performance, "object");
    assert_equals(typeof self.performance.getEntriesByType, "function");
    var lowerList = self.performance.getEntriesByType("resource");
    var upperList = self.performance.getEntriesByType("RESOURCE");
    var mixedList = self.performance.getEntriesByType("ReSoUrCe");

    assert_not_equals(lowerList.length, 0, "Resource entries exist");
    assert_equals(upperList.length, 0, "getEntriesByType('RESOURCE').length");
    assert_equals(mixedList.length, 0, "getEntriesByType('ReSoUrCe').length");

  }, "getEntriesByType values are case sensitive");

  test(function () {
    assert_equals(typeof self.performance, "object");
    assert_equals(typeof self.performance.getEntriesByName, "function");
    var origin = self.location.protocol + "//" + self.location.host;
    var location1 = origin.toUpperCase() + "/resources/testharness.js";
    var location2 = self.location.protocol + "//"
     + self.location.host.toUpperCase() + "/resources/testharness.js";
    var lowerList = self.performance.getEntriesByName(origin + "/resources/testharness.js");
    var upperList = self.performance.getEntriesByName(location1);
    var mixedList = self.performance.getEntriesByName(location2);

    assert_equals(lowerList.length, 1, "Resource entry exist");
    assert_equals(upperList.length, 0, "getEntriesByName('" + location1 + "').length");
    assert_equals(mixedList.length, 0, "getEntriesByName('" + location2 + "').length");

  }, "getEntriesByName values are case sensitive");

  async_test(function (t) {
    // Test type/buffered case sensitivity.
    observer = new PerformanceObserver(
      t.step_func(function (entryList, obs) {
        assert_unreached("Observer(type) should not be called.");
      })
    );
    observer.observe({type: "Mark"});
    observer.observe({type: "Measure"});
    observer.observe({type: "MARK"});
    observer.observe({type: "MEASURE"});
    observer.observe({type: "Mark", buffered: true});
    observer.observe({type: "Measure", buffered: true});
    observer.observe({type: "MARK", buffered: true});
    observer.observe({type: "MEASURE", buffered: true});
    self.performance.mark("mark1");
    self.performance.measure("measure1");

    // Test entryTypes case sensitivity.
    observer = new PerformanceObserver(
      t.step_func(function (entryList, obs) {
        assert_unreached("Observer(entryTypes) should not be called.");
      })
    );
    observer.observe({entryTypes: ["Mark", "Measure"]});
    observer.observe({entryTypes: ["MARK", "MEASURE"]});
    self.performance.mark("mark1");
    self.performance.measure("measure1");

    t.step_timeout(function() {
      t.done();
    }, 1000);

  }, "observe() and case sensitivity for types/entryTypes and buffered.");
