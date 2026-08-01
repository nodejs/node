self.addEventListener('activate', event => {
    event.waitUntil(
        self.registration.navigationPreload.enable());
  });

function get_response_info(r) {
  var info = {
    type: r.type,
    url: r.url,
    status: r.status,
    ok: r.ok,
    statusText: r.statusText,
    headers: []
  };
  r.headers.forEach((value, name) => { info.headers.push([value, name]); });
  return info;
}

function post_to_page(data) {
  return self.clients.matchAll()
    .then(clients => clients.forEach(client => client.postMessage(data)));
}

self.addEventListener('fetch', event => {
    event.respondWith(
      event.preloadResponse
        .then(
          res => {
            if (res.url.includes("base")) {
              return res;
            }
            return post_to_page(get_response_info(res)).then(_ => res);
          },
          err => new Response(err.toString())));
  });
