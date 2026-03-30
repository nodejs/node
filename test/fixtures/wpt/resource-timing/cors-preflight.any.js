// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js

// Because apache decrements the Keep-Alive max value on each request, the
// transferSize will vary slightly between requests for the same resource.
const fuzzFactor = 3;  // bytes

const {HTTP_REMOTE_ORIGIN} = get_host_info();
const url = new URL('/resource-timing/resources/preflight.py',
    HTTP_REMOTE_ORIGIN).href;

// The header bytes are expected to be > |minHeaderSize| and
// < |maxHeaderSize|. If they are outside this range the test will fail.
const minHeaderSize = 100;
const maxHeaderSize = 1024;

promise_test(async () => {
  const checkCorsAllowed = response => response.arrayBuffer();
  const requirePreflight = {headers: {'X-Require-Preflight' : '1'}};
  const collectEntries = new Promise(resolve => {
    let entriesSeen = [];
    new PerformanceObserver(entryList => {
      entriesSeen = entriesSeen.concat(entryList.getEntries());
      if (entriesSeen.length > 2) {
        throw new Error(`Saw too many PerformanceResourceTiming entries ` +
            `(${entriesSeen.length})`);
      }
      if (entriesSeen.length == 2) {
        resolve(entriesSeen);
      }
    }).observe({"type": "resource"});
  });

  // Although this fetch doesn't send a pre-flight request, the server response
  // will allow cross-origin requests explicitly with the
  // Access-Control-Allow-Origin header.
  await fetch(url).then(checkCorsAllowed);

  // This fetch will send a pre-flight request to do the CORS handshake
  // explicitly.
  await fetch(url, requirePreflight).then(checkCorsAllowed);

  const entries = await collectEntries;
  assert_greater_than(entries[0].transferSize, 0, 'No-preflight transferSize');
  const lowerBound = entries[0].transferSize - fuzzFactor;
  const upperBound = entries[0].transferSize + fuzzFactor;
  assert_between_exclusive(entries[1].transferSize, lowerBound, upperBound,
      'Preflighted transferSize');
}, 'PerformanceResourceTiming sizes fetch with preflight test');
