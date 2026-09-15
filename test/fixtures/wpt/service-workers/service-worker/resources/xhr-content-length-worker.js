// Service worker for the xhr-content-length test.

self.addEventListener("fetch", event => {
  const url = new URL(event.request.url);
  const type = url.searchParams.get("type");

  if (type === "no-content-length") {
    event.respondWith(new Response("Hello!"));
  }

  if (type === "larger-content-length") {
    event.respondWith(new Response("meeeeh", { headers: [["Content-Length", "10000"]] }));
  }

  if (type === "double-content-length") {
    event.respondWith(new Response("meeeeh", { headers: [["Content-Length", "10000"], ["Content-Length", "10000"]] }));
  }

  if (type === "bogus-content-length") {
    event.respondWith(new Response("meeeeh", { headers: [["Content-Length", "test"]] }));
  }
});
