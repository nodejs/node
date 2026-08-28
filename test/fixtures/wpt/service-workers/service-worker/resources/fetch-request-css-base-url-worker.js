let source;
let resolveDone;
let done = new Promise(resolve => resolveDone = resolve);

// The page messages this worker to ask for the result. Keep the worker alive
// via waitUntil() until the result is sent.
self.addEventListener('message', event => {
  source = event.data.port;
  source.postMessage('pong');
  event.waitUntil(done);
});

self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);

  // For the CSS file, respond in a way that may change the response URL,
  // depending on |url.search|.
  const cssPath = 'request-url-path/fetch-request-css-base-url-style.css';
  if (url.pathname.indexOf(cssPath) != -1) {
    // Respond with a different URL, deleting "request-url-path/".
    if (url.search == '?fetch') {
      event.respondWith(fetch('fetch-request-css-base-url-style.css?fetch'));
    }
    // Respond with new Response().
    else if (url.search == '?newResponse') {
      const styleString = 'body { background-image: url("./sample.png");}';
      const headers = {'content-type': 'text/css'};
      event.respondWith(new Response(styleString, headers));
    }
  }

  // The image request indicates what the base URL of the CSS was. Message the
  // result back to the test page.
  else if (url.pathname.indexOf('sample.png') != -1) {
    // For some reason |source| is undefined here when running the test manually
    // in Firefox. The test author experimented with both using Client
    // (event.source) and MessagePort to try to get the test to pass, but
    // failed.
    source.postMessage({
      url: event.request.url,
      referrer: event.request.referrer
    });
    resolveDone();
  }
});
