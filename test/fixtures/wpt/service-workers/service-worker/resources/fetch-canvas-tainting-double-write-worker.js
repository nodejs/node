self.addEventListener('fetch', (event) => {
  url = new URL(event.request.url);
  if (url.search == '?PNGIMAGE') {
    localUrl = new URL(url.pathname + url.search, self.location);
    event.respondWith(fetch(localUrl));
  }
});
