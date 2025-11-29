test(() => {
  if (typeof PerformanceObserver.supportedEntryTypes === "undefined")
    assert_unreached("supportedEntryTypes is not supported.");
  const types = PerformanceObserver.supportedEntryTypes;
  assert_true(types.includes("mark"),
    "There should be 'mark' in PerformanceObserver.supportedEntryTypes");
  assert_true(types.includes("measure"),
    "There should be 'measure' in PerformanceObserver.supportedEntryTypes");
  assert_greater_than(types.indexOf("measure"), types.indexOf('mark'),
    "The 'measure' entry should appear after the 'mark' entry");
}, "supportedEntryTypes contains 'mark' and 'measure'.");

if (typeof PerformanceObserver.supportedEntryTypes !== "undefined") {
  const entryTypes = {
    "mark": () => {
      performance.mark('foo');
    },
    "measure": () => {
      performance.measure('bar');
    }
  }
  for (let entryType in entryTypes) {
    if (PerformanceObserver.supportedEntryTypes.includes(entryType)) {
      promise_test(async() => {
        await new Promise((resolve) => {
          new PerformanceObserver(function (list, observer) {
            observer.disconnect();
            resolve();
          }).observe({entryTypes: [entryType]});

          // Force the PerformanceEntry.
          entryTypes[entryType]();
        })
      }, `'${entryType}' entries should be observable.`)
    }
  }
}
