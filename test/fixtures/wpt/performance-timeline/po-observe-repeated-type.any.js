// META: script=performanceobservers.js

async_test(function (t) {
  const observer  = new PerformanceObserver(
      t.step_func(function (entryList) {
        checkEntries(entryList.getEntries(),
          [{ entryType: "mark", name: "early"}]);
        observer.disconnect();
        t.done();
      })
    );
  performance.mark("early");
  // This call will not trigger anything.
  observer.observe({type: "mark"});
  // This call should override the previous call and detect the early mark.
  observer.observe({type: "mark", buffered: true});
}, "Two calls of observe() with the same 'type' cause override.");
