// META: title=Request signals &amp; the cache API
// META: global=window,worker

promise_test(async () => {
  await caches.delete('test');
  const controller = new AbortController();
  const signal = controller.signal;
  const request = new Request('../resources/data.json', { signal });

  const cache = await caches.open('test');
  await cache.put(request, new Response(''));

  const requests = await cache.keys();

  assert_equals(requests.length, 1, 'Ensuring cleanup worked');

  const [cachedRequest] = requests;

  controller.abort();

  assert_false(cachedRequest.signal.aborted, "Request from cache shouldn't be aborted");

  const data = await fetch(cachedRequest).then(r => r.json());
  assert_equals(data.key, 'value', 'Fetch fully completes');
}, "Signals are not stored in the cache API");

promise_test(async () => {
  await caches.delete('test');
  const controller = new AbortController();
  const signal = controller.signal;
  const request = new Request('../resources/data.json', { signal });
  controller.abort();

  const cache = await caches.open('test');
  await cache.put(request, new Response(''));

  const requests = await cache.keys();

  assert_equals(requests.length, 1, 'Ensuring cleanup worked');

  const [cachedRequest] = requests;

  assert_false(cachedRequest.signal.aborted, "Request from cache shouldn't be aborted");

  const data = await fetch(cachedRequest).then(r => r.json());
  assert_equals(data.key, 'value', 'Fetch fully completes');
}, "Signals are not stored in the cache API, even if they're already aborted");
