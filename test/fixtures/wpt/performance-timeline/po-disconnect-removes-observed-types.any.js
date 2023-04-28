// META: script=performanceobservers.js

async_test(function (t) {
  const observer  = new PerformanceObserver(
      t.step_func(function (entryList) {
        // There should be no mark entry.
        checkEntries(entryList.getEntries(),
          [{ entryType: "measure", name: "b"}]);
        t.done();
      })
    );
  observer.observe({type: "mark"});
  // Disconnect the observer.
  observer.disconnect();
  // Now, only observe measure.
  observer.observe({type: "measure"});
  performance.mark("a");
  performance.measure("b");
}, "Types observed are forgotten when disconnect() is called.");
