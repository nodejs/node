// META: script=performanceobservers.js

  test(function () {
    const obs = new PerformanceObserver(() => {});
    assert_throws_js(TypeError, function () {
      obs.observe({entryTypes: "mark"});
    });
  }, "entryTypes must be a sequence or throw a TypeError");

  test(function () {
    const obs = new PerformanceObserver(() => {});
    obs.observe({entryTypes: []});
  }, "Empty sequence entryTypes does not throw an exception.");

  test(function () {
    const obs = new PerformanceObserver(() => {});
    obs.observe({entryTypes: ["this-cannot-match-an-entryType"]});
    obs.observe({entryTypes: ["marks","navigate", "resources"]});
  }, "Unknown entryTypes do not throw an exception.");

  test(function () {
    const obs = new PerformanceObserver(() => {});
    obs.observe({entryTypes: ["mark","this-cannot-match-an-entryType"]});
    obs.observe({entryTypes: ["this-cannot-match-an-entryType","mark"]});
    obs.observe({entryTypes: ["mark"], others: true});
  }, "Filter unsupported entryType entryType names within the entryTypes sequence");

  async_test(function (t) {
    var finish = t.step_func(function () { t.done(); });
    var observer = new PerformanceObserver(
      function (entryList, obs) {
        var self = this;
        t.step(function () {
          assert_true(entryList instanceof PerformanceObserverEntryList, "first callback parameter must be a PerformanceObserverEntryList instance");
          assert_true(obs instanceof PerformanceObserver, "second callback parameter must be a PerformanceObserver instance");
          assert_equals(observer, self, "observer is the this value");
          assert_equals(observer, obs, "observer is second parameter");
          assert_equals(self, obs, "this and second parameter are the same");
          observer.disconnect();
          finish();
        });
      }
    );
    self.performance.clearMarks();
    observer.observe({entryTypes: ["mark"]});
    self.performance.mark("mark1");
  }, "Check observer callback parameter and this values");

  async_test(function (t) {
  var observer = new PerformanceObserver(
      t.step_func(function (entryList, obs) {
        checkEntries(entryList.getEntries(),
          [{ entryType: "measure", name: "measure1"}]);
        observer.disconnect();
        t.done();
      })
    );
    self.performance.clearMarks();
    observer.observe({entryTypes: ["mark"]});
    observer.observe({entryTypes: ["measure"]});
    self.performance.mark("mark1");
    self.performance.measure("measure1");
  }, "replace observer if already present");
