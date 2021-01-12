self.addEventListener('fetch', function(event) {
    if (event.request.url.includes('dummy')) {
        event.waitUntil(async function() {
            let destination = new URL(event.request.url).searchParams.get("dest");
            let clients = await self.clients.matchAll({"includeUncontrolled": true});
            clients.forEach(function(client) {
                if (client.url.includes("fetch-destination-iframe")) {
                    if (event.request.destination == destination) {
                        client.postMessage("PASS");
                    } else {
                        client.postMessage("FAIL");
                    }
                }
            })
        }());
    }
    event.respondWith(fetch(event.request));
});


