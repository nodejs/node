importScripts('test-helpers.sub.js');

const BASE_PATH = base_path();

self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);
  // The iframe requests this cross-origin stylesheet; substitute a same-origin
  // response (a real same-origin fetch => a cross-origin -> same-origin internal
  // redirect on the page's channel).
  if (url.pathname.endsWith('/css-sw-substitute-marker.css')) {
    event.respondWith(fetch(
        BASE_PATH + 'fetch-request-css-cross-origin-mime-check-same.css'));
  }
});
