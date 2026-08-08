"use strict";

self.onfetch = event => {
  if (event.request.url.endsWith("non-existent-stream-1.txt")) {
    const rs1 = new ReadableStream();
    event.respondWith(new Response(rs1));
    rs1.cancel(1);
  } else if (event.request.url.endsWith("non-existent-stream-2.txt")) {
    const rs2 = new ReadableStream({
      start(controller) { controller.error(1) }
    });
    event.respondWith(new Response(rs2));
  } else if (event.request.url.endsWith("non-existent-stream-3.txt")) {
    const rs3 = new ReadableStream({
      pull(controller) { controller.error(1) }
    });
    event.respondWith(new Response(rs3));
  }
};
