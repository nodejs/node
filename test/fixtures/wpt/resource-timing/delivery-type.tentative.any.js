// META: global=window,worker
// META: script=/common/get-host-info.sub.js
// META: script=/resource-timing/resources/resource-loaders.js

// TODO(crbug/1358591): Rename this file from "tentative" once
// `w3c/resource-timing#343` is merged.

const {REMOTE_ORIGIN, ORIGIN} = get_host_info();

const redirectBase = new URL(
  '/resource-timing/resources/redirect-cors.py', REMOTE_ORIGIN).href;
const cacheAndValidatedBase = new URL(
  '/resource-timing/resources/cacheable-and-validated.py?content=content',
  ORIGIN).href;

const mustRevalidate = {headers: {'Cache-Control': 'max-age=0'}};

const fetchAndEatBody = (url, fetchOption) => {
  return fetch(url, fetchOption).then(response => response.arrayBuffer());
};

const accumulateEntries = () => {
  return new Promise(resolve => {
    const po = new PerformanceObserver(list => {
      resolve(list);
    });
    po.observe({type: "resource", buffered: true});
  });
};

const checkDeliveryTypeBase =
  (list, lookupURL, deliveryTypeForCachedResources) => {
    const entries = list.getEntriesByName(lookupURL);
    assert_equals(entries.length, 3, 'Wrong number of entries');

    // 200 response (`cacheMode` is an empty string)
    assert_equals(entries[0].deliveryType, "",
      "Expect empty deliveryType for 200 response.");
    // Cached response (`cacheMode` is "local") or 304 response (`cacheMode` is
    // "validated").
    assert_equals(entries[1].deliveryType, deliveryTypeForCachedResources,
      `Expect "${deliveryTypeForCachedResources}" deliveryType for a
        cached response.`);
    assert_equals(entries[2].deliveryType, deliveryTypeForCachedResources,
      `Expect "${deliveryTypeForCachedResources}" deliveryType for a
        revalidated response.`);
};

promise_test(() => {
  // Use a different URL every time so that the cache behaviour does not depend
  // on execution order.
  const initialURL = load.cache_bust(cacheAndValidatedBase);
  const checkDeliveryType =
    list => checkDeliveryTypeBase(list, initialURL, "cache");
  return fetchAndEatBody(initialURL, {})                      // 200.
    .then(() => fetchAndEatBody(initialURL, {}))              // Cached.
    .then(() => fetchAndEatBody(initialURL, mustRevalidate))  // 304.
    .then(accumulateEntries)
    .then(checkDeliveryType);
}, 'PerformanceResourceTiming deliveryType test, same origin.');

promise_test(() => {
  const cacheAndValidatedURL = load.cache_bust(
    cacheAndValidatedBase + '&timing_allow_origin=*');
  const redirectURL = redirectBase
    + "?timing_allow_origin=*"
    + `&allow_origin=${encodeURIComponent(ORIGIN)}`
    + `&location=${encodeURIComponent(cacheAndValidatedURL)}`;
  const checkDeliveryType =
    list => checkDeliveryTypeBase(list, redirectURL, "cache");
  return fetchAndEatBody(redirectURL, {})                      // 200.
    .then(() => fetchAndEatBody(redirectURL, {}))              // Cached.
    .then(() => fetchAndEatBody(redirectURL, mustRevalidate))  // 304.
    .then(accumulateEntries)
    .then(checkDeliveryType);
}, 'PerformanceResourceTiming deliveryType test, cross origin, TAO passes.');

promise_test(() => {
  const cacheAndValidatedURL = load.cache_bust(cacheAndValidatedBase);
  const redirectURL = redirectBase
    + `?allow_origin=${encodeURIComponent(ORIGIN)}`
    + `&location=${encodeURIComponent(cacheAndValidatedURL)}`;
  const checkDeliveryType =
    list => checkDeliveryTypeBase(list, redirectURL, "");
  return fetchAndEatBody(redirectURL, {})                      // 200.
    .then(() => fetchAndEatBody(redirectURL, {}))              // Cached.
    .then(() => fetchAndEatBody(redirectURL, mustRevalidate))  // 304.
    .then(accumulateEntries)
    .then(checkDeliveryType);
}, 'PerformanceResourceTiming deliveryType test, cross origin, TAO fails.');
