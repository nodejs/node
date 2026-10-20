'use strict';

addEventListener('fetch', event => {
  const url = new URL(event.request.url);
  const type = url.searchParams.get('type');

  if (!type) return;

  if (type === 'string') {
    event.respondWith(new Response('PASS'));
  }
  else if (type === 'blob') {
    event.respondWith(
      new Response(new Blob(['PASS']))
    );
  }
  else if (type === 'buffer-view') {
    const encoder = new TextEncoder();
    event.respondWith(
      new Response(encoder.encode('PASS'))
    );
  }
  else if (type === 'buffer') {
    const encoder = new TextEncoder();
    event.respondWith(
      new Response(encoder.encode('PASS').buffer)
    );
  }
  else if (type === 'form-data') {
    const body = new FormData();
    body.set('result', 'PASS');
    event.respondWith(
      new Response(body)
    );
  }
  else if (type === 'search-params') {
    const body = new URLSearchParams();
    body.set('result', 'PASS');
    event.respondWith(
      new Response(body, {
        headers: { 'Content-Type': 'text/plain' }
      })
    );
  }
});
