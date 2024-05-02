// META: script=performanceobservers.js

  async_test(function (t) {
    var callbackCount = 0;
    var observer = new PerformanceObserver(
        t.step_func(function (entryList, obs) {
          callbackCount++;

          if (callbackCount === 1) {
            checkEntries(entryList.getEntries(), [
              {entryType: "measure", name: "measure1"},
            ]);
            observer.observe({entryTypes: ["mark"]});
            self.performance.mark("mark2");
            self.performance.measure("measure2");
            return;
          }

          if (callbackCount === 2) {
            checkEntries(entryList.getEntries(), [
              {entryType: "mark", name: "mark2"},
            ]);
            self.performance.mark("mark-before-change-observe-state-to-measure");
            self.performance.measure("measure-before-change-observe-state-to-measure");
            observer.observe({entryTypes: ["measure"]});
            self.performance.mark("mark3");
            self.performance.measure("measure3");
            return;
          }

          if (callbackCount === 3) {
            checkEntries(entryList.getEntries(), [
              {entryType: "measure", name: "measure3"},
              {entryType: "mark", name: "mark-before-change-observe-state-to-measure"},
            ]);
            self.performance.mark("mark-before-change-observe-state-to-both");
            self.performance.measure("measure-before-change-observe-state-to-both");
            observer.observe({entryTypes: ["mark", "measure"]});
            self.performance.mark("mark4");
            self.performance.measure("measure4");
            return;
          }

          if (callbackCount === 4) {
            checkEntries(entryList.getEntries(), [
              {entryType: "measure", name: "measure-before-change-observe-state-to-both"},
              {entryType: "measure", name: "measure4"},
              {entryType: "mark", name: "mark4"},
            ]);
            self.performance.mark("mark-before-disconnect");
            self.performance.measure("measure-before-disconnect");
            observer.disconnect();
            self.performance.mark("mark-after-disconnect");
            self.performance.measure("measure-after-disconnect");
            t.done();
            return;
          }

          assert_unreached("The callback must not be invoked after disconnecting");
        })
      );

    observer.observe({entryTypes: ["measure"]});
    self.performance.mark("mark1");
    self.performance.measure("measure1");
  }, "PerformanceObserver modifications inside callback should update filtering and not clear buffer");
