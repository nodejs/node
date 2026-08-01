let name;
if (self.registration.scope.indexOf('scope1') != -1)
  name = 'sw1';
if (self.registration.scope.indexOf('scope2') != -1)
  name = 'sw2';


self.addEventListener('fetch', evt => {
  // There are three types of requests this service worker handles.

  // (1) The first request for the worker, which will redirect elsewhere.
  // "redirect.py" means to test network redirect, so let network handle it.
  if (evt.request.url.indexOf('redirect.py') != -1) {
    return;
  }
  // "sw-redirect" means to test service worker redirect, so respond with a
  // redirect.
  if (evt.request.url.indexOf('sw-redirect') != -1) {
    const url = new URL(evt.request.url);
    const redirect_to = url.searchParams.get('Redirect');
    evt.respondWith(Response.redirect(redirect_to));
    return;
  }

  // (2) After redirect, the request is for a "webworker.py" URL.
  // Add a search parameter to indicate this service worker handled the
  // final request for the worker.
  if (evt.request.url.indexOf('webworker.py') != -1) {
    const greeting = encodeURIComponent(`${name} saw the request for the worker script`);
    // Serve from `./subdir/`, not `./`,
    // to conform that the base URL used in the worker is
    // the response URL (`./subdir/`), not the current request URL (`./`).
    evt.respondWith(fetch(`subdir/worker_interception_redirect_webworker.py?greeting=${greeting}`));
    return;
  }

  const path = (new URL(evt.request.url)).pathname;

  // (3) The worker does an importScripts() to import-scripts-echo.py. Indicate
  // that this service worker handled the request.
  if (evt.request.url.indexOf('import-scripts-echo.py') != -1) {
    const msg = encodeURIComponent(`${name} saw importScripts from the worker: ${path}`);
    evt.respondWith(fetch(`import-scripts-echo.py?msg=${msg}`));
    return;
  }

  // (4) The worker does a fetch() to simple.txt. Indicate that this service
  // worker handled the request.
  if (evt.request.url.indexOf('simple.txt') != -1) {
    evt.respondWith(new Response(`${name} saw the fetch from the worker: ${path}`));
    return;
  }
});
