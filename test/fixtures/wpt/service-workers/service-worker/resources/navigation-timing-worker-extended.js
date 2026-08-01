importScripts("/resources/testharness.js");
const timings = {}

const DELAY_ACTIVATION = 500

self.addEventListener('activate', event => {
    event.waitUntil(new Promise(resolve => {
        timings.activateWorkerStart = performance.now() + performance.timeOrigin;

        // This gives us enough time to ensure activation would delay fetch handling
        step_timeout(resolve, DELAY_ACTIVATION);
    }).then(() => timings.activateWorkerEnd = performance.now() + performance.timeOrigin));
})

self.addEventListener('fetch', event => {
    timings.handleFetchEvent = performance.now() + performance.timeOrigin;
    event.respondWith(Promise.resolve(new Response(new Blob([`
            <script>
                parent.postMessage(${JSON.stringify(timings)}, "*")
            </script>
    `]))));
});
