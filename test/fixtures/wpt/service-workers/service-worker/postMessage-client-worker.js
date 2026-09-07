async function doTest(e)
{
    if (e.resultingClientId) {
        const promise = new Promise(async resolve => {
            let counter = 0;
            const client = await self.clients.get(e.resultingClientId);
            if (client)
                client.postMessage(counter++);
            if (e.request.url.includes("repeatMessage")) {
                setInterval(() => {
                    if (client)
                        client.postMessage(counter++);
                }, 100);
            }
            setTimeout(() => {
                resolve(fetch(e.request));
            }, 1000);
        });
        e.respondWith(promise);
    }
}

self.addEventListener("fetch", e => e.waitUntil(doTest(e)));
