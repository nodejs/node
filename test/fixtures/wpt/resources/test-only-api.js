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

/**
 * A helper for Chromium-based browsers to load Mojo JS bindings
 *
 * This is an async function that works in both workers and windows. It first
 * loads mojo_bindings.js, disables automatic dependency loading, and loads all
 * resources sequentially. The promise resolves if everything loads
 * successfully, or rejects if any exception is raised. If testharness.js is
 * used, an uncaught exception will terminate the test with a harness error
 * (unless `allow_uncaught_exception` is true), which is usually the desired
 * behaviour.
 *
 * This function also works with Blink web tests loaded from file://, in which
 * case file:// will be prepended to all '/gen/...' URLs.
 *
 * Only call this function if isChromiumBased === true.
 *
 * @param {Array.<string>} resources - A list of scripts to load: Mojo JS
 *   bindings should be of the form '/gen/../*.mojom.js' or
 *   '/gen/../*.mojom-lite.js' (requires `lite` to be true); the order does not
 *   matter. Do not include 'mojo_bindings.js' or 'mojo_bindings_lite.js'.
 * @param {boolean=} lite - Whether the lite bindings (*.mojom-lite.js) are used
 *   (default is false).
 * @returns {Promise}
 */
async function loadMojoResources(resources, lite = false) {
  if (!isChromiumBased) {
    throw new Error('MojoJS not enabled; start Chrome with --enable-blink-features=MojoJS,MojoJSTest');
  }
  if (resources.length == 0) {
    return;
  }

  let genPrefix = '';
  if (self.location.pathname.includes('/web_tests/')) {
    // Blink internal web tests
    genPrefix = 'file://';
  }

  for (const path of resources) {
    // We want to load mojo_bindings.js separately to set mojo.config.
    if (path.endsWith('/mojo_bindings.js')) {
      throw new Error('Do not load mojo_bindings.js explicitly.');
    }
    if (path.endsWith('/mojo_bindings_lite.js')) {
      throw new Error('Do not load mojo_bindings_lite.js explicitly.');
    }
    if (lite) {
      if (! /^\/gen\/.*\.mojom-lite\.js$/.test(path)) {
        throw new Error(`Unrecognized resource path: ${path}`);
      }
    } else {
      if (! /^\/gen\/.*\.mojom\.js$/.test(path)) {
        throw new Error(`Unrecognized resource path: ${path}`);
      }
    }
  }

  if (lite) {
    await loadScript(genPrefix + '/gen/layout_test_data/mojo/public/js/mojo_bindings_lite.js');
  } else {
    await loadScript(genPrefix + '/gen/layout_test_data/mojo/public/js/mojo_bindings.js');
    mojo.config.autoLoadMojomDeps = false;
  }

  for (const path of resources) {
    await loadScript(genPrefix + path);
  }
}
