'use strict';

/* Whether the browser is Chromium-based with MojoJS enabled */
const isChromiumBased = 'MojoInterfaceInterceptor' in self;
/* Whether the browser is WebKit-based with internal test-only API enabled */
const isWebKitBased = !isChromiumBased && 'internals' in self;

/**
 * Loads a script in a window or worker.
 *
 * @param {string} path - A script path
 * @returns {Promise}
 */
function loadScript(path) {
  if (typeof document === 'undefined') {
    // Workers (importScripts is synchronous and may throw.)
    importScripts(path);
    return Promise.resolve();
  } else {
    // Window
    const script = document.createElement('script');
    script.src = path;
    script.async = false;
    const p = new Promise((resolve, reject) => {
      script.onload = () => { resolve(); };
      script.onerror = e => { reject(`Error loading ${path}`); };
    })
    document.head.appendChild(script);
    return p;
  }
}
