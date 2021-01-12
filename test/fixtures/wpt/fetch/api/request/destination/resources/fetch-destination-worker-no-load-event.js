self.addEventListener('fetch', function(event) {
    const url = event.request.url;
    if (url.includes('dummy') && url.includes('?')) {
        event.waitUntil(async function() {
            let destination = new URL(url).searchParams.get("dest");
            var result = "FAIL";
            if (event.request.destination == destination ||
                (event.request.destination == "empty" && destination == "")) {
              result = "PASS";
            }
            let cl = await clients.matchAll({includeUncontrolled: true});
            for (i = 0; i < cl.length; i++) {
              cl[i].postMessage(result);
            }
        }())
    }
    event.respondWith(fetch(event.request));
});


