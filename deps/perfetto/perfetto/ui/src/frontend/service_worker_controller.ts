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

// Handles registration, unregistration and lifecycle of the service worker.
// This class contains only the controlling logic, all the code in here runs in
// the main thread, not in the service worker thread.
// The actual service worker code is in src/service_worker.
// Design doc: http://go/perfetto-offline.

import {reportError} from '../base/logging';

import {globals} from './globals';

// We use a dedicated |caches| object to share a global boolean beween the main
// thread and the SW. SW cannot use local-storage or anything else other than
// IndexedDB (which would be overkill).
const BYPASS_ID = 'BYPASS_SERVICE_WORKER';

export class ServiceWorkerController {
  private _initialWorker: ServiceWorker|null = null;
  private _bypassed = false;
  private _installing = false;

  // Caller should reload().
  async setBypass(bypass: boolean) {
    if (!('serviceWorker' in navigator)) return;  // Not supported.
    this._bypassed = bypass;
    if (bypass) {
      await caches.open(BYPASS_ID);  // Create the entry.
      for (const reg of await navigator.serviceWorker.getRegistrations()) {
        await reg.unregister();
      }
    } else {
      await caches.delete(BYPASS_ID);
      this.install();
    }
    globals.rafScheduler.scheduleFullRedraw();
  }

  onStateChange(sw: ServiceWorker) {
    globals.rafScheduler.scheduleFullRedraw();
    if (sw.state === 'installing') {
      this._installing = true;
    } else if (sw.state === 'activated') {
      this._installing = false;
      // Don't show the notification if the site was served straight
      // from the network (e.g., on the very first visit or after
      // Ctrl+Shift+R). In these cases, we are already at the last
      // version.
      if (sw !== this._initialWorker && this._initialWorker) {
        globals.frontendLocalState.newVersionAvailable = true;
      }
    }
  }

  monitorWorker(sw: ServiceWorker|null) {
    if (!sw) return;
    sw.addEventListener('error', (e) => reportError(e));
    sw.addEventListener('statechange', () => this.onStateChange(sw));
    this.onStateChange(sw);  // Trigger updates for the current state.
  }

  async install() {
    if (!('serviceWorker' in navigator)) return;  // Not supported.

    if (await caches.has(BYPASS_ID)) {
      this._bypassed = true;
      console.log('Skipping service worker registration, disabled by the user');
      return;
    }
    navigator.serviceWorker.register('service_worker.js').then(registration => {
      this._initialWorker = registration.active;

      // At this point there are two options:
      // 1. This is the first time we visit the site (or cache was cleared) and
      //    no SW is installed yet. In this case |installing| will be set.
      // 2. A SW is already installed (though it might be obsolete). In this
      //    case |active| will be set.
      this.monitorWorker(registration.installing);
      this.monitorWorker(registration.active);

      // Setup the event that shows the "A new release is available"
      // notification.
      registration.addEventListener('updatefound', () => {
        this.monitorWorker(registration.installing);
      });
    });
  }

  get bypassed() {
     return this._bypassed;
  }
  get installing() {
    return this._installing;
  }
}