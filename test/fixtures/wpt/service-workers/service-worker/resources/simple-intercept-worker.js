self.onfetch = function(event) {
  if (event.request.url.indexOf('simple') != -1)
    event.respondWith(
      new Response(new Blob(['intercepted by service worker'])));
};
