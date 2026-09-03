// Service worker for fetch-event-download-*.https.html.
//
// Echoes the FetchEvent it sees for download requests onto a BroadcastChannel
// named after the channel ID embedded in the request URL's `?channel=` query
// parameter, then optionally provides a response based on the `?mode=` param.
//
// Mode values:
//   "respond" (default) — call event.respondWith(new Response("from-sw", ...)).
//   "fallback"          — observe the event but do not call respondWith,
//                         letting the request fall back to the network.

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);
  const channelId = url.searchParams.get('channel');
  if (!channelId) {
    return;
  }
  const mode = url.searchParams.get('mode') || 'respond';

  const channel = new BroadcastChannel(channelId);
  channel.postMessage({
    type: 'fetch',
    url: event.request.url,
    destination: event.request.destination,
    method: event.request.method,
    mode: event.request.mode,
    referrer: event.request.referrer,
    clientId: event.clientId,
    resultingClientId: event.resultingClientId,
  });

  if (mode === 'fallback') {
    return;
  }

  event.respondWith(new Response('from-sw', {
    headers: {'content-type': 'text/plain'},
  }));
});
