async function cancelBodyTest(event)
{
    await new Promise(resolve => setTimeout(resolve, 100));
    event.request.body.cancel("service worker cancelled body");
    await new Promise(resolve => setTimeout(resolve, 100));
    return new Response("PASS");
}

self.addEventListener("fetch", (event) => {
    if (!event.request.url.includes("cancel-body-test"))
        return;

    event.respondWith(cancelBodyTest(event));
});
