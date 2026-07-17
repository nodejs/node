async_test( t=> {
  for (let i = 0; i < 50; i++)
    performance.mark('foo' + i);
  let marksCreated = 50;
  let marksReceived = 0;
  new PerformanceObserver(list => {
    marksReceived += list.getEntries().length;
    if (marksCreated < 100) {
      performance.mark('bar' + marksCreated);
      marksCreated++;
    }
    if (marksReceived == 100)
      t.done();
  }).observe({type: 'mark', buffered: true});
}, 'PerformanceObserver with buffered flag should see past and future entries.');
