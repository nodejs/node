const requests = [];
let port = undefined;

self.onmessage = e => {
  const message = e.data;
  if ('port' in message) {
    port = message.port;
    port.postMessage({ready: true});
  }
};

self.addEventListener('fetch', e => {
  const url = e.request.url;
  if (!url.includes('sample?test')) {
    return;
  }
  port.postMessage({
    url: url,
    mode: e.request.mode,
    redirect: e.request.redirect,
    credentials: e.request.credentials,
    integrity: e.request.integrity,
    destination: e.request.destination
  });
  e.respondWith(Promise.reject());
});
