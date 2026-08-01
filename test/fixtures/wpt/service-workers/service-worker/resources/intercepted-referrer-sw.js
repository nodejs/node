self.addEventListener('install', event => {
  // Activate immediately to simplify the test.
  event.waitUntil(self.skipWaiting());
});

self.addEventListener('activate', event => {
  // Make sure the very next navigation is controlled by this SW.
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', event => {
  // Only handle top-level navigations within our scope.
  if (event.request.mode === 'navigate') {
    const html = `<!doctype html>
      <meta charset="utf-8">
      <title>SW Intercepted Page</title>
      <script>
        // Post the referrer observed by this new Document.
        window.addEventListener('load', () => {
          parent.postMessage({
            source: 'sw-intercepted',
            referrer: document.referrer,
            url: location.href
          }, '*');
        });
      </script>
      <h1>SW-synthesized page</h1>
      <p>This page was served by a Service Worker via respondWith().</p>`;

    event.respondWith(new Response(html, {
      headers: { 'Content-Type': 'text/html; charset=utf-8' }
    }));
  }
  // Otherwise, fall through to network.
});
