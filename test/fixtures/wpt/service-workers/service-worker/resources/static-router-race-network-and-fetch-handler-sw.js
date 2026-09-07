'use strict';

import {routerRules} from './router-rules.js';
import {
  recordRequest,
  recordError,
  getRecords,
  resetRecords } from './static-router-sw.sub.js';

import './imported-sw.js';

self.addEventListener('install', async e => {
  e.waitUntil(caches.open('v1').then(
      cache => {cache.put('cache.txt', new Response('From cache'))}));

  const params = new URLSearchParams(location.search);
  const key = params.get('key');
  try {
    await e.addRoutes(routerRules[key]);
  } catch (e) {
    recordError(e);
  }
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});

self.addEventListener('fetch', async function(event) {
  try {
    recordRequest(event.request);
    const url = new URL(event.request.url);
    // Force slow response
    if (url.searchParams.has('sw_slow')) {
      const start = Date.now();
      while (true) {
        if (Date.now() - start > 200) {
          break;
        }
      }
    }
    const nonce = url.searchParams.get('nonce');
    event.respondWith(new Response(nonce));
    // Call `fetch(e.request)` but it's not consumed.
    if (url.searchParams.has('call_fetch')) {
      await fetch(event.request);
    }
  } catch (e) {
    recordError(e)
  }
});

self.addEventListener('message', function(event) {
  if (event.data.reset) {
    resetRecords();
  }
  if (event.data.port) {
    const {requests, errors} = getRecords();
    event.data.port.postMessage({requests, errors});
  }
});
