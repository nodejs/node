// Service worker for the xhr-response-url test.

self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);
  const respondWith = url.searchParams.get('respondWith');
  if (!respondWith)
    return;

  if (respondWith == 'fetch') {
    const target = url.searchParams.get('url');
    event.respondWith(fetch(target));
    return;
  }

  if (respondWith == 'string') {
    const headers = {'content-type': 'text/plain'};
    event.respondWith(new Response('hello', {headers}));
    return;
  }

  if (respondWith == 'document') {
    const doc = `
        <!DOCTYPE html>
        <html>
        <title>hi</title>
        <body>hello</body>
        </html>`;
    const headers = {'content-type': 'text/html'};
    event.respondWith(new Response(doc, {headers}));
    return;
  }
});
