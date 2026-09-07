importScripts("/resources/testharness.js");

function doTest(event)
{
    if (!event.request.url.includes("fetch-error-test"))
        return;

    let counter = 0;
    const stream = new ReadableStream({ pull: controller => {
        switch (++counter) {
        case 1:
            controller.enqueue(new Uint8Array([1]));
            return;
        default:
            // We asynchronously error the stream so that there is ample time to resolve the fetch promise and call text() on the response.
            step_timeout(() => controller.error("Sorry"), 50);
        }
    }});
    event.respondWith(new Response(stream));
}

self.addEventListener("fetch", doTest);
