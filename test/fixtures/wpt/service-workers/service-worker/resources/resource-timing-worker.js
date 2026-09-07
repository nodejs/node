self.addEventListener('fetch', function(event) {
  if (event.request.url.indexOf('sample.js') != -1) {
    event.respondWith(new Promise(resolve => {
      // Slightly delay the response so we ensure we get a non-zero
      // duration.
      setTimeout(_ => resolve(new Response('// Empty javascript')), 50);
    }));
  }
  else if (event.request.url.indexOf('missing.jpg?SWRespondsWithFetch') != -1) {
    event.respondWith(fetch('sample.txt?SWFetched'));
  }
});
