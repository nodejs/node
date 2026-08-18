'use strict';

self.addEventListener('fetch', event => {
    if (!event.request.url.match(/body-stream-with-invalid-chunk$/))
      return;
    const stream = new ReadableStream({start: controller => {
        // The argument is intentionally a string, not a Uint8Array.
        controller.enqueue('hello');
      }});
    const headers = { 'x-content-type-options': 'nosniff' };
    event.respondWith(new Response(stream, { headers }));
  });
