// This worker is meant to test range requests where the responses come from
// multiple origins. It forwards the first request to a cross-origin URL
// (generating an opaque response). The server is expected to return a 206
// Partial Content response.  Then the worker lets subsequent range requests
// fall back to network (generating same-origin responses). The intent is to try
// to trick the browser into treating the resource as same-origin.
//
// It would also be interesting to do the reverse test where the first request
// goes to the same-origin URL, and subsequent range requests go cross-origin in
// 'no-cors' mode to receive opaque responses. But the service worker cannot do
// this, because in 'no-cors' mode the 'range' HTTP header is disallowed.

importScripts('/common/get-host-info.sub.js')

let initial = true;
function is_initial_request() {
  const old = initial;
  initial = false;
  return old;
}

self.addEventListener('fetch', e => {
    const url = new URL(e.request.url);
    if (url.search.indexOf('VIDEO') == -1) {
      // Fall back for non-video.
      return;
    }

    // Make the first request go cross-origin.
    if (is_initial_request()) {
      const cross_origin_url = get_host_info().HTTPS_REMOTE_ORIGIN +
          url.pathname + url.search;
      const cross_origin_request = new Request(cross_origin_url,
          {mode: 'no-cors', headers: e.request.headers});
      e.respondWith(fetch(cross_origin_request));
      return;
    }

    // Fall back to same origin for subsequent range requests.
  });
