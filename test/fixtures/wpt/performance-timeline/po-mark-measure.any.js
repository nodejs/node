// META: script=performanceobservers.js

  async_test(function (t) {
    var stored_entries = [];
    var observer  = new PerformanceObserver(
        t.step_func(function (entryList, obs) {
          stored_entries =
            stored_entries.concat(entryList.getEntries());
          if (stored_entries.length >= 4) {
            checkEntries(stored_entries,
              [{ entryType: "mark", name: "mark1"},
              { entryType: "mark", name: "mark2"},
              { entryType: "measure", name: "measure1"},
              { entryType: "measure", name: "measure2"}]);
            observer.disconnect();
            t.done();
          }
        })
      );
    observer.observe({entryTypes: ["mark", "measure"]});
  }, "entries are observable");

  async_test(function (t) {
    var mark_entries = [];
    var observer = new PerformanceObserver(
        t.step_func(function (entryList, obs) {
          mark_entries =
            mark_entries.concat(entryList.getEntries());
          if (mark_entries.length >= 2) {
            checkEntries(mark_entries,
              [{ entryType: "mark", name: "mark1"},
              { entryType: "mark", name: "mark2"}]);
            observer.disconnect();
            t.done();
          }
        })
      );
    observer.observe({entryTypes: ["mark"]});
    self.performance.mark("mark1");
    self.performance.mark("mark2");
  }, "mark entries are observable");

  async_test(function (t) {
    var measure_entries = [];
    var observer = new PerformanceObserver(
        t.step_func(function (entryList, obs) {
          measure_entries =
            measure_entries.concat(entryList.getEntries());
          if (measure_entries.length >= 2) {
            checkEntries(measure_entries,
              [{ entryType: "measure", name: "measure1"},
              { entryType: "measure", name: "measure2"}]);
            observer.disconnect();
            t.done();
          }
        })
      );
    observer.observe({entryTypes: ["measure"]});
    self.performance.measure("measure1");
    self.performance.measure("measure2");
  }, "measure entries are observable");
