addEventListener('fetch', e => {
  if (e.request.url.includes('get-worker-client-url')) {
    e.respondWith((async () => {
      const clients = await self.clients.matchAll({type: 'worker'});
      if (clients.length != 1)
        return new Response('one worker client should exist');
      return new Response(clients[0].url);
    })());
  }
});
