// META: title=Cache Storage: Abort
// META: global=window,worker
// META: script=./resources/test-helpers.js
// META: script=/common/utils.js
// META: timeout=long

// We perform the same tests on put, add, addAll. Parameterise the tests to
// reduce repetition.
const methodsToTest = {
  put: async (cache, request) => {
    const response = await fetch(request);
    return cache.put(request, response);
  },
  add: async (cache, request) => cache.add(request),
  addAll: async (cache, request) => cache.addAll([request]),
};

for (const method in methodsToTest) {
  const perform = methodsToTest[method];

  cache_test(async (cache, test) => {
    const controller = new AbortController();
    const signal = controller.signal;
    controller.abort();
    const request = new Request('../resources/simple.txt', { signal });
    return promise_rejects_dom(test, 'AbortError', perform(cache, request),
                          `${method} should reject`);
  }, `${method}() on an already-aborted request should reject with AbortError`);

  cache_test(async (cache, test) => {
    const controller = new AbortController();
    const signal = controller.signal;
    const request = new Request('../resources/simple.txt', { signal });
    const promise = perform(cache, request);
    controller.abort();
    return promise_rejects_dom(test, 'AbortError', promise,
                          `${method} should reject`);
  }, `${method}() synchronously followed by abort should reject with ` +
     `AbortError`);

  cache_test(async (cache, test) => {
    const controller = new AbortController();
    const signal = controller.signal;
    const stateKey = token();
    const abortKey = token();
    const request = new Request(
        `../../../fetch/api/resources/infinite-slow-response.py?stateKey=${stateKey}&abortKey=${abortKey}`,
        { signal });

    const promise = perform(cache, request);

    // Wait for the server to start sending the response body.
    let opened = false;
    do {
      // Normally only one fetch to 'stash-take' is needed, but the fetches
      // will be served in reverse order sometimes
      // (i.e., 'stash-take' gets served before 'infinite-slow-response').

      const response =
            await fetch(`../../../fetch/api/resources/stash-take.py?key=${stateKey}`);
      const body = await response.json();
      if (body === 'open') opened = true;
    } while (!opened);

    // Sadly the above loop cannot guarantee that the browser has started
    // processing the response body. This delay is needed to make the test
    // failures non-flaky in Chrome version 66. My deepest apologies.
    await new Promise(resolve => setTimeout(resolve, 250));

    controller.abort();

    await promise_rejects_dom(test, 'AbortError', promise,
                          `${method} should reject`);

    // infinite-slow-response.py doesn't know when to stop.
    return fetch(`../../../fetch/api/resources/stash-put.py?key=${abortKey}&value=close`);
  }, `${method}() followed by abort after headers received should reject ` +
     `with AbortError`);
}

done();
