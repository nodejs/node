'use strict';
importScripts("/resources/testharness.js");

const map = new Map();

self.addEventListener('fetch', event => {
  const url = new URL(event.request.url);
  if (!url.searchParams.has('stream')) return;

  if (url.searchParams.has('observe-cancel')) {
    const id = url.searchParams.get('id');
    if (id === undefined) {
      event.respondWith(new Error('error'));
      return;
    }
    event.waitUntil(new Promise(resolve => {
      map.set(id, {label: 'pending', resolve});
    }));

    const stream = new ReadableStream({
      pull(c) {
        if (url.searchParams.get('enqueue') === 'true') {
          url.searchParams.delete('enqueue');
          c.enqueue(new Uint8Array([65]));
        }
      },
      cancel() {
        map.get(id).label = 'cancelled';
      }
    });
    event.respondWith(new Response(stream));
    return;
  }

  if (url.searchParams.has('query-cancel')) {
    const id = url.searchParams.get('id');
    if (id === undefined) {
      event.respondWith(new Error('error'));
      return;
    }
    const entry = map.get(id);
    if (entry === undefined) {
      event.respondWith(new Error('not found'));
      return;
    }
    map.delete(id);
    entry.resolve();
    event.respondWith(new Response(entry.label));
    return;
  }

  if (url.searchParams.has('use-fetch-stream')) {
    event.respondWith(async function() {
      const response = await fetch('pass.txt');
      return new Response(response.body);
    }());
    return;
  }

  const delayEnqueue = url.searchParams.has('delay');

  const stream = new ReadableStream({
    start(controller) {
      const encoder = new TextEncoder();

      const populate = () => {
        controller.enqueue(encoder.encode('PASS'));
        controller.close();
      }

      if (delayEnqueue) {
        step_timeout(populate, 16);
      }
      else {
        populate();
      }
    }
  });

  event.respondWith(new Response(stream));
});
