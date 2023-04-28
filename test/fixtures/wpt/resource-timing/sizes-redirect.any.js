// META: global=window,worker
// META: script=/common/get-host-info.sub.js
// META: script=/resource-timing/resources/sizes-helper.js

const baseUrl =
  new URL('/resource-timing/resources/TAOResponse.py?tao=wildcard', location.href).href;
const expectedSize = 4;

const hostInfo = get_host_info();
performance.clearResourceTimings();

const accumulateEntry = () => {
  return new Promise(resolve => {
    const po = new PerformanceObserver(list => {
      resolve(list);
    });
    po.observe({type: "resource", buffered: true});
  });
};

const checkResourceSizes = () => {
  const entries = performance.getEntriesByType('resource');
  for (let entry of entries) {
    checkSizeFields(entry, expectedSize, expectedSize + headerSize);
  }
}

const redirectUrl = (redirectSourceOrigin, allowOrigin, targetUrl) => {
  return redirectSourceOrigin +
    '/resource-timing/resources/redirect-cors.py?allow_origin=' +
    encodeURIComponent(allowOrigin) +
    '&timing_allow_origin=*' +
    '&location=' + encodeURIComponent(targetUrl);
}

promise_test(() => {
  // Use a different URL every time so that the cache behaviour does not
  // depend on execution order.
  const directUrl = cacheBustUrl(baseUrl);
  const sameOriginRedirect = redirectUrl(hostInfo.ORIGIN, '*', directUrl);
  const crossOriginRedirect = redirectUrl(hostInfo.REMOTE_ORIGIN,
    hostInfo.ORIGIN, directUrl);
  const mixedRedirect = redirectUrl(hostInfo.REMOTE_ORIGIN,
    hostInfo.ORIGIN, sameOriginRedirect);
  const complexRedirect = redirectUrl(hostInfo.ORIGIN,
    hostInfo.REMOTE_ORIGIN, mixedRedirect);
  let eatBody = response => response.arrayBuffer();
  return fetch(directUrl)
    .then(eatBody)
    .then(() => fetch(sameOriginRedirect))
    .then(eatBody)
    .then(() => fetch(crossOriginRedirect))
    .then(eatBody)
    .then(() => fetch(mixedRedirect))
    .then(eatBody)
    .then(() => fetch(complexRedirect))
    .then(eatBody)
    .then(accumulateEntry)
    .then(checkResourceSizes);
}, 'PerformanceResourceTiming sizes Fetch with redirect test');

done();
