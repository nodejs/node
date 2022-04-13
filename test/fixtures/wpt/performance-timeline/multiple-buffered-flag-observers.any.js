promise_test(() => {
  // The first promise waits for one buffered flag observer to receive 3 entries.
  const promise1 = new Promise(resolve1 => {
    let numObserved1 = 0;
    new PerformanceObserver((entryList, obs) => {
      // This buffered flag observer is constructed after a regular observer detects a mark.
      new PerformanceObserver(list => {
        numObserved1 += list.getEntries().length;
        if (numObserved1 == 3)
          resolve1();
      }).observe({type: 'mark', buffered: true});
      obs.disconnect();
    }).observe({entryTypes: ['mark']});
    performance.mark('foo');
  });
  // The second promise waits for another buffered flag observer to receive 3 entries.
  const promise2 = new Promise(resolve2 => {
    step_timeout(() => {
      let numObserved2 = 0;
      // This buffered flag observer is constructed after a delay of 100ms.
      new PerformanceObserver(list => {
        numObserved2 += list.getEntries().length;
        if (numObserved2 == 3)
          resolve2();
      }).observe({type: 'mark', buffered: true});
    }, 100);
    performance.mark('bar');
  });
  performance.mark('meow');
  // Pass if and only if both buffered observers received all 3 mark entries.
  return Promise.all([promise1, promise2]);
}, 'Multiple PerformanceObservers with buffered flag see all entries');
