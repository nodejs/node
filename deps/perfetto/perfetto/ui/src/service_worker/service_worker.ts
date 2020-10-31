// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This script handles the caching of the UI resources, allowing it to work
// offline (as long as the UI site has been visited at least once).
// Design doc: http://go/perfetto-offline.

// When a new version of the UI is released (e.g. v1 -> v2), the following
// happens on the next visit:
// 1. The v1 (old) service worker is activated (at this point we don't know yet
//    that v2 is released).
// 2. /index.html is requested. The SW intercepts the request and serves
//    v1 from cache.
// 3. The browser checks if a new version of service_worker.js is available. It
//    does that by comparing the bytes of the current and new version.
// 5. service_worker.js v2 will not be byte identical with v1, even if v2 was a
//    css-only change. This is due to the hashes in UI_DIST_MAP below. For this
//    reason v2 is installed in the background (it takes several seconds).
// 6. The 'install' handler is triggered, the new resources are fetched and
//    populated in the cache.
// 7. The 'activate' handler is triggered. The old caches are deleted at this
//    point.
// 8. frontend/index.ts (in setupServiceWorker()) is notified about the activate
//    and shows a notification prompting to reload the UI.
//
// If the user just closes the tab or hits refresh, v2 will be served anyways
// on the next load.

// UI_DIST_FILES is map of {file_name -> sha1}.
// It is really important that this map is bundled directly in the
// service_worker.js bundle file, as it's used to cause the browser to
// re-install the service worker and re-fetch resources when anything changes.
// This is why the map contains the SHA1s even if we don't directly use them in
// the code (because it makes the final .js file content-dependent).

import {UI_DIST_MAP} from '../gen/dist_file_map';

declare var self: ServiceWorkerGlobalScope;

const CACHE_NAME = 'dist-' + UI_DIST_MAP.hex_digest.substr(0, 16);
const LOG_TAG = `ServiceWorker[${UI_DIST_MAP.hex_digest.substr(0, 16)}]: `;


function shouldHandleHttpRequest(req: Request): boolean {
  // Suppress warning: 'only-if-cached' can be set only with 'same-origin' mode.
  // This seems to be a chromium bug. An internal code search suggests this is a
  // socially acceptable workaround.
  if (req.cache === 'only-if-cached' && req.mode !== 'same-origin') {
    return false;
  }

  const url = new URL(req.url);
  return req.method === 'GET' && url.origin === self.location.origin;
}

async function handleHttpRequest(req: Request): Promise<Response> {
  if (!shouldHandleHttpRequest(req)) {
    throw new Error(LOG_TAG + `${req.url} shouldn't have been handled`);
  }

  // We serve from the cache even if req.cache == 'no-cache'. It's a bit
  // contra-intuitive but it's the most consistent option. If the user hits the
  // reload button*, the browser requests the "/" index with a 'no-cache' fetch.
  // However all the other resources (css, js, ...) are requested with a
  // 'default' fetch (this is just how Chrome works, it's not us). If we bypass
  // the service worker cache when we get a 'no-cache' request, we can end up in
  // an inconsistent state where the index.html is more recent than the other
  // resources, which is undesirable.
  // * Only Ctrl+R. Ctrl+Shift+R will always bypass service-worker for all the
  // requests (index.html and the rest) made in that tab.
  try {
    const cacheOps = {cacheName: CACHE_NAME} as CacheQueryOptions;
    const cachedRes = await caches.match(req, cacheOps);
    if (cachedRes) {
      console.debug(LOG_TAG + `serving ${req.url} from cache`);
      return cachedRes;
    }
    console.warn(LOG_TAG + `cache miss on ${req.url}`);
  } catch (exc) {
    console.error(LOG_TAG + `Cache request failed for ${req.url}`, exc);
  }

  // In any other case, just propagate the fetch on the network, which is the
  // safe behavior.
  console.debug(LOG_TAG + `falling back on network fetch() for ${req.url}`);
  return fetch(req);
}

// The install() event is fired:
// - The very first time the site is visited, after frontend/index.ts has
//   executed the serviceWorker.register() method.
// - *After* the site is loaded, if the service_worker.js code
//   has changed (because of the hashes in UI_DIST_MAP, service_worker.js will
//   change if anything in the UI has changed).
self.addEventListener('install', event => {
  const doInstall = async () => {
    if (await caches.has('BYPASS_SERVICE_WORKER')) {
      // Throw will prevent the installation.
      throw new Error(LOG_TAG + 'skipping installation, bypass enabled');
    }
    console.log(LOG_TAG + 'installation started');
    const cache = await caches.open(CACHE_NAME);
    const urlsToCache: RequestInfo[] = [];
    for (const [file, integrity] of Object.entries(UI_DIST_MAP.files)) {
      const reqOpts:
          RequestInit = {cache: 'reload', mode: 'same-origin', integrity};
      urlsToCache.push(new Request(file, reqOpts));
      if (file === 'index.html' && location.host !== 'storage.googleapis.com') {
        // Disable cachinig of '/' for cases where the UI is hosted on GCS.
        // GCS doesn't support auto indexes. GCS returns a 404 page on / that
        // fails the integrity check.
        urlsToCache.push(new Request('/', reqOpts));
      }
    }
    await cache.addAll(urlsToCache);
    console.log(LOG_TAG + 'installation completed');

    // skipWaiting() still waits for the install to be complete. Without this
    // call, the new version would be activated only when all tabs are closed.
    // Instead, we ask to activate it immediately. This is safe because each
    // service worker version uses a different cache named after the SHA256 of
    // the contents. When the old version is activated, the activate() method
    // below will evict the cache for the old versions. If there is an old still
    // opened, any further request from that tab will be a cache-miss and go
    // through the network (which is inconsitent, but not the end of the world).
    self.skipWaiting();
  };
  event.waitUntil(doInstall());
});

self.addEventListener('activate', (event) => {
  console.warn(LOG_TAG + 'activated');
  const doActivate = async () => {
    // Clear old caches.
    for (const key of await caches.keys()) {
      if (key !== CACHE_NAME) await caches.delete(key);
    }
    // This makes a difference only for the very first load, when no service
    // worker is present. In all the other cases the skipWaiting() will hot-swap
    // the active service worker anyways.
    await self.clients.claim();
  };
  event.waitUntil(doActivate());
});

self.addEventListener('fetch', event => {
  // The early return here will cause the browser to fall back on standard
  // network-based fetch.
  if (!shouldHandleHttpRequest(event.request)) {
    console.debug(LOG_TAG + `serving ${event.request.url} from network`);
    return;
  }

  event.respondWith(handleHttpRequest(event.request));
});
