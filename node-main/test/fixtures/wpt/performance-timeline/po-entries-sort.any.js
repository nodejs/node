// META: script=performanceobservers.js

  async_test(function (t) {
    var stored_entries = [];
    var stored_entries_by_type = [];
    var observer = new PerformanceObserver(
        t.step_func(function (entryList, obs) {

          stored_entries = entryList.getEntries();
          stored_entries_by_type = entryList.getEntriesByType("mark");
          stored_entries_by_name = entryList.getEntriesByName("name-repeat");
          var startTimeOfMark2 = entryList.getEntriesByName("mark2")[0].startTime;

          checkSorted(stored_entries);
          checkEntries(stored_entries, [
            {entryType: "measure", name: "measure1"},
            {entryType: "measure", name: "measure2"},
            {entryType: "measure", name: "measure3"},
            {entryType: "measure", name: "name-repeat"},
            {entryType: "mark", name: "mark1"},
            {entryType: "mark", name: "mark2"},
            {entryType: "measure", name: "measure-matching-mark2-1"},
            {entryType: "measure", name: "measure-matching-mark2-2"},
            {entryType: "mark", name: "name-repeat"},
            {entryType: "mark", name: "name-repeat"},
          ]);

          checkSorted(stored_entries_by_type);
          checkEntries(stored_entries_by_type, [
            {entryType: "mark", name: "mark1"},
            {entryType: "mark", name: "mark2"},
            {entryType: "mark", name: "name-repeat"},
            {entryType: "mark", name: "name-repeat"},
          ]);

          checkSorted(stored_entries_by_name);
          checkEntries(stored_entries_by_name, [
            {entryType: "measure", name: "name-repeat"},
            {entryType: "mark", name: "name-repeat"},
            {entryType: "mark", name: "name-repeat"},
          ]);

          observer.disconnect();
          t.done();
        })
      );

    observer.observe({entryTypes: ["mark", "measure"]});

    self.performance.mark("mark1");
    self.performance.measure("measure1");
    wait(); // Ensure mark1 !== mark2 startTime by making sure performance.now advances.
    self.performance.mark("mark2");
    self.performance.measure("measure2");
    self.performance.measure("measure-matching-mark2-1", "mark2");
    wait(); // Ensure mark2 !== mark3 startTime by making sure performance.now advances.
    self.performance.mark("name-repeat");
    self.performance.measure("measure3");
    self.performance.measure("measure-matching-mark2-2", "mark2");
    wait(); // Ensure name-repeat startTime will differ.
    self.performance.mark("name-repeat");
    wait(); // Ensure name-repeat startTime will differ.
    self.performance.measure("name-repeat");
  }, "getEntries, getEntriesByType, getEntriesByName sort order");
