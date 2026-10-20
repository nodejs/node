'use strict';

self.addEventListener('fetch', event => {
    if (!event.request.url.match(/body-stream$/))
        return;

    var counter = 0;
    const encoder = new TextEncoder();
    const stream = new ReadableStream({ pull: controller => {
        switch (++counter) {
        case 1:
            controller.enqueue(encoder.encode(''));
            return;
        case 2:
            controller.enqueue(encoder.encode('chunk #1'));
            return;
        case 3:
            controller.enqueue(encoder.encode(' '));
            return;
        case 4:
            controller.enqueue(encoder.encode('chunk #2'));
            return;
        case 5:
            controller.enqueue(encoder.encode(' '));
            return;
        case 6:
            controller.enqueue(encoder.encode('chunk #3'));
            return;
        case 7:
            controller.enqueue(encoder.encode(' '));
            return;
        case 8:
            controller.enqueue(encoder.encode('chunk #4'));
            return;
        default:
            controller.close();
        }
    }});
    event.respondWith(new Response(stream));
});
