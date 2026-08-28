addEventListener('fetch', evt => {
  if (evt.request.url.includes('sample')) {
    evt.respondWith(new Response('intercepted'));
  }
});
