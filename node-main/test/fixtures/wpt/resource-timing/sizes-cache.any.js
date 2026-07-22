// META: global=window,worker
// META: script=/resource-timing/resources/sizes-helper.js
// META: script=/resource-timing/resources/resource-loaders.js

let url = new URL(
  '/resource-timing/resources/cacheable-and-validated.py' +
  '?content=loremipsumblablabla',
  location.href).href;
const bodySize = 19;

const accumulateEntries = () => {
  return new Promise(resolve => {
    const po = new PerformanceObserver(list => {
      resolve(list);
    });
    po.observe({type: "resource", buffered: true});
  });
};

const checkResourceSizes = list => {
  const entries = list.getEntriesByName(url);
  assert_equals(entries.length, 3, 'Wrong number of entries');
  let seenCount = 0;
  for (let entry of entries) {
    if (seenCount === 0) {
      // 200 response
      checkSizeFields(entry, bodySize, bodySize + headerSize);
    } else if (seenCount === 1) {
      // from cache
      checkSizeFields(entry, bodySize, 0);
    } else if (seenCount === 2) {
      // 304 response
      checkSizeFields(entry, bodySize, headerSize);
    } else {
      assert_unreached('Too many matching entries');
    }
    ++seenCount;
  }
};

promise_test(() => {
  // Use a different URL every time so that the cache behaviour does not
  // depend on execution order.
  url = load.cache_bust(url);
  const eatBody = response => response.arrayBuffer();
  const mustRevalidate = {headers: {'Cache-Control': 'max-age=0'}};
  return fetch(url)
    .then(eatBody)
    .then(() => fetch(url))
    .then(eatBody)
    .then(() => fetch(url, mustRevalidate))
    .then(eatBody)
    .then(accumulateEntries)
    .then(checkResourceSizes);
}, 'PerformanceResourceTiming sizes caching test');
