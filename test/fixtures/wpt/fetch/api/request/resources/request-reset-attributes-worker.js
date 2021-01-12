self.addEventListener('fetch', (event) => {
    const params = new URL(event.request.url).searchParams;
    if (params.has('ignore')) {
      return;
    }
    if (!params.has('name')) {
      event.respondWith(Promise.reject(TypeError('No name is provided.')));
      return;
    }

    const name = params.get('name');
    const old_attribute = event.request[name];
    // If any of |init|'s member is present...
    const init = {cache: 'no-store'}
    const new_attribute = (new Request(event.request, init))[name];

    event.respondWith(
      new Response(`old: ${old_attribute}, new: ${new_attribute}`));
  });
