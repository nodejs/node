const kURL = '/service-worker-interception-network-worker.js';
const kScript = 'postMessage("LOADED_FROM_SERVICE_WORKER")';
const kHeaders = [['content-type', 'text/javascript']];

self.addEventListener('fetch', e => {
  // Serve a generated response for kURL.
  if (e.request.url.indexOf(kURL) != -1)
    e.respondWith(new Response(kScript, { headers: kHeaders }));
});
