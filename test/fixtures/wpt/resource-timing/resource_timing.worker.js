importScripts("/resources/testharness.js");

function check(initiatorType, protocol) {
  let entries = performance.getEntries();
  assert_equals(entries.length, 1);

  assert_true(entries[0] instanceof PerformanceEntry);
  assert_equals(entries[0].entryType, "resource");
  assert_true(entries[0].startTime > 0);
  assert_true(entries[0].duration > 0);

  assert_true(entries[0] instanceof PerformanceResourceTiming);
  assert_equals(entries[0].initiatorType, initiatorType);
  assert_equals(entries[0].nextHopProtocol, protocol);
}

async_test(t => {
  performance.clearResourceTimings();

  // Fetch
  fetch("resources/empty.js")
  .then(r => r.blob())
  .then(blob => {
    check("fetch", "http/1.1");
  })

  // XMLHttpRequest
  .then(() => {
    return new Promise(resolve => {
      performance.clearResourceTimings();
      let xhr = new XMLHttpRequest();
      xhr.onload = () => {
        check("xmlhttprequest", "http/1.1");
        resolve();
      };
      xhr.open("GET", "resources/empty.js");
      xhr.send();
    });
  })

  // Sync XMLHttpREquest
  .then(() => {
    performance.clearResourceTimings();
    let xhr = new XMLHttpRequest();
    xhr.open("GET", "resources/empty.js", false);
    xhr.send();

    check("xmlhttprequest", "http/1.1");
  })

  // ImportScripts
  .then(() => {
    performance.clearResourceTimings();
    importScripts(["resources/empty.js"]);
    check("other", "http/1.1");
  })

  // All done.
  .then(() => {
    t.done();
  });
}, "Performance Resource Entries in workers");

done();
