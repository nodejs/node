importScripts('test-helpers.sub.js');
importScripts('/common/get-host-info.sub.js');

const NAME = 'foo';
const SAME_ORIGIN_BASE = new URL('./', self.location.href).href;
const CROSS_ORIGIN_BASE = new URL('./',
    get_host_info().HTTPS_REMOTE_ORIGIN + base_path()).href;

const urls = [
  `${SAME_ORIGIN_BASE}opaque-script-small.js`,
  `${SAME_ORIGIN_BASE}opaque-script-large.js`,
  `${CROSS_ORIGIN_BASE}opaque-script-small.js`,
  `${CROSS_ORIGIN_BASE}opaque-script-large.js`,
];

self.addEventListener('install', evt => {
  evt.waitUntil(async function() {
    const c = await caches.open(NAME);
    const promises = urls.map(async function(u) {
      const r = await fetch(u, { mode: 'no-cors' });
      await c.put(u, r);
    });
    await Promise.all(promises);
  }());
});

self.addEventListener('fetch', evt => {
  const url = new URL(evt.request.url);
  if (!url.pathname.includes('opaque-script-small.js') &&
      !url.pathname.includes('opaque-script-large.js')) {
    return;
  }
  evt.respondWith(async function() {
    const c = await caches.open(NAME);
    return c.match(evt.request);
  }());
});
