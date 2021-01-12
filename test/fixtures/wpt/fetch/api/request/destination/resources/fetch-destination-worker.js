self.addEventListener('fetch', function(event) {
    if (event.request.url.includes('dummy')) {
        let destination = new URL(event.request.url).searchParams.get("dest");
        if (event.request.destination == destination ||
            (event.request.destination == "empty" && destination == "")) {
            event.respondWith(fetch(event.request));
        } else {
            event.respondWith(Response.error());
        }
    }
});

