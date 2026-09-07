// This worker is meant to test range requests where the responses are a mix of
// opaque ones and non-opaque ones. It forwards the first request to a
// cross-origin URL (generating an opaque response). The server is expected to
// return a 206 Partial Content response.  Then the worker forwards subsequent
// range requests to that URL, with CORS sharing generating a non-opaque
// responses. The intent is to try to trick the browser into treating the
// resource as non-opaque.
//
// It would also be interesting to do the reverse test where the first request
// uses 'cors', and subsequent range requests use 'no-cors' mode. But the
// service worker cannot do this, because in 'no-cors' mode the 'range' HTTP
// header is disallowed.

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

    let cross_origin_url = get_host_info().HTTPS_REMOTE_ORIGIN +
        url.pathname + url.search;

    // The first request is no-cors.
    if (is_initial_request()) {
      const init = { mode: 'no-cors', headers: e.request.headers };
      const cross_origin_request = new Request(cross_origin_url, init);
      e.respondWith(fetch(cross_origin_request));
      return;
    }

    // Subsequent range requests are cors.

    // Copy headers needed for range requests.
    let my_headers = new Headers;
    if (e.request.headers.get('accept'))
      my_headers.append('accept', e.request.headers.get('accept'));
    if (e.request.headers.get('range'))
    my_headers.append('range', e.request.headers.get('range'));

    // Add &ACAOrigin to allow CORS.
    cross_origin_url += '&ACAOrigin=' + get_host_info().HTTPS_ORIGIN;
    // Add &ACAHeaders to allow range requests.
    cross_origin_url += '&ACAHeaders=accept,range';

    // Make the CORS request.
    const init = { mode: 'cors', headers: my_headers };
    const cross_origin_request = new Request(cross_origin_url, init);
    e.respondWith(fetch(cross_origin_request));
  });

