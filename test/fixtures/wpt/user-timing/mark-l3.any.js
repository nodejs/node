// META: script=resources/user-timing-helper.js

async_test(function (t) {
    let mark_entries = [];
    const expected_entries =
        [{ entryType: "mark", name: "mark1", detail: null},
        { entryType: "mark", name: "mark2", detail: null},
        { entryType: "mark", name: "mark3", detail: null},
        { entryType: "mark", name: "mark4", detail: null},
        { entryType: "mark", name: "mark5", detail: null},
        { entryType: "mark", name: "mark6", detail: {}},
        { entryType: "mark", name: "mark7", detail: {info: 'abc'}},
        { entryType: "mark", name: "mark8", detail: null, startTime: 234.56},
        { entryType: "mark", name: "mark9", detail: {count: 3}, startTime: 345.67}];
    const observer = new PerformanceObserver(
        t.step_func(function (entryList, obs) {
          mark_entries =
            mark_entries.concat(entryList.getEntries());
          if (mark_entries.length >= expected_entries.length) {
            checkEntries(mark_entries, expected_entries);
            observer.disconnect();
            t.done();
          }
        })
      );
    self.performance.clearMarks();
    observer.observe({entryTypes: ["mark"]});
    const returned_entries = [];
    returned_entries.push(self.performance.mark("mark1"));
    returned_entries.push(self.performance.mark("mark2", undefined));
    returned_entries.push(self.performance.mark("mark3", null));
    returned_entries.push(self.performance.mark("mark4", {}));
    returned_entries.push(self.performance.mark("mark5", {detail: null}));
    returned_entries.push(self.performance.mark("mark6", {detail: {}}));
    returned_entries.push(self.performance.mark("mark7", {detail: {info: 'abc'}}));
    returned_entries.push(self.performance.mark("mark8", {startTime: 234.56}));
    returned_entries.push(self.performance.mark("mark9", {detail: {count: 3}, startTime: 345.67}));
    checkEntries(returned_entries, expected_entries);
}, "mark entries' detail and startTime are customizable.");
