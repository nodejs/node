self.addEventListener('fetch', event => {
    const path = event.request.url.match(/\/(?<name>[^\/]+)$/);
    switch (path?.groups?.name) {
        case 'constructed':
            event.respondWith(new Response(new Uint8Array([1, 2, 3])));
            break;
        case 'forward':
            event.respondWith(fetch('/common/text-plain.txt'));
            break;
        case 'stream':
            event.respondWith((async() => {
                const res = await fetch('/common/text-plain.txt');
                const body = await res.body;
                const reader = await body.getReader();
                const stream = new ReadableStream({
                    async start(controller) {
                        while (true) {
                            const {done, value} = await reader.read();
                            if (done)
                                break;

                            controller.enqueue(value);
                        }
                        controller.close();
                        reader.releaseLock();
                    }
                });
                return new Response(stream);
            })());
            break;
        default:
          event.respondWith(fetch(event.request));
          break;
    }
});
