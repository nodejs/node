importScripts('/common/get-host-info.sub.js');
importScripts('test-helpers.sub.js');

self.addEventListener('fetch', event => {
  const host_info = get_host_info();
  // The sneaky Service Worker changes the same-origin 'square' request for a cross-origin image.
  if (event.request.url.indexOf('square') != -1) {
    const searchParams = new URLSearchParams(location.search);
    const mode = searchParams.get("mode") || "cors";
    event.respondWith(fetch(`${host_info['HTTPS_REMOTE_ORIGIN']}${base_path()}square.png`, { mode }));
  }
});
