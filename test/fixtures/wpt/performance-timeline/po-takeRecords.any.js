// META: title=PerformanceObserver: takeRecords
// META: script=performanceobservers.js

async_test(function (t) {
    const observer = new PerformanceObserver(function (entryList, observer) {
      assert_unreached('This callback should not have been called.')
    });
    let entries = observer.takeRecords();
    checkEntries(entries, [], 'No records before observe');
    observer.observe({entryTypes: ['mark']});
    assert_equals(typeof(observer.takeRecords), 'function');
    entries = observer.takeRecords();
    checkEntries(entries, [], 'No records just from observe');
    performance.mark('a');
    performance.mark('b');
    entries = observer.takeRecords();
    checkEntries(entries, [
      {entryType: 'mark', name: 'a'},
      {entryType: 'mark', name: 'b'}
    ]);
    performance.mark('c');
    performance.mark('d');
    performance.mark('e');
    entries = observer.takeRecords();
    checkEntries(entries, [
      {entryType: 'mark', name: 'c'},
      {entryType: 'mark', name: 'd'},
      {entryType: 'mark', name: 'e'}
    ]);
    entries = observer.takeRecords();
    checkEntries(entries, [], 'No entries right after takeRecords');
    observer.disconnect();
    t.done();
  }, "Test PerformanceObserver's takeRecords()");
