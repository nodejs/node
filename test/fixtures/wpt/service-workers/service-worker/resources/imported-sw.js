'use strict';

import {routerRules} from './router-rules.js';

const params = new URLSearchParams(location.search);
const key = params.get('imported-sw-router-key');

if (key) {
  self.addEventListener('install', async e => {
    await e.addRoutes(routerRules[key]);
    self.skipWaiting();
  });
}
