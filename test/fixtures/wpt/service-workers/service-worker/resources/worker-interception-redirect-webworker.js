// This is the (shared or dedicated) worker file for the
// worker-interception-redirect test. It should be served by the corresponding
// .py file instead of being served directly.
//
// This file is served from both resources/*webworker.py,
// resources/scope2/*webworker.py and resources/subdir/*webworker.py.
// Relative paths are used in `fetch()` and `importScripts()` to confirm that
// the correct base URLs are used.

// This greeting text is meant to be injected by the Python script that serves
// this file, to indicate how the script was served (from network or from
// service worker).
//
// We can't just use a sub pipe and name this file .sub.js since we want
// to serve the file from multiple URLs (see above).
let greeting = '%GREETING_TEXT%';
if (!greeting)
  greeting = 'the worker script was served from network';

// Call importScripts() which fills |echo_output| with a string indicating
// whether a service worker intercepted the importScripts() request.
let echo_output;
const import_scripts_msg = encodeURIComponent(
    'importScripts: served from network');
let import_scripts_greeting = 'not set';
try {
  importScripts(`import-scripts-echo.py?msg=${import_scripts_msg}`);
  import_scripts_greeting = echo_output;
} catch(e) {
  import_scripts_greeting = 'importScripts failed';
}

async function runTest(port) {
  port.postMessage(greeting);

  port.postMessage(import_scripts_greeting);

  const response = await fetch('simple.txt');
  const text = await response.text();
  port.postMessage('fetch(): ' + text);

  port.postMessage(self.location.href);
}

if ('DedicatedWorkerGlobalScope' in self &&
    self instanceof DedicatedWorkerGlobalScope) {
  runTest(self);
} else if (
    'SharedWorkerGlobalScope' in self &&
    self instanceof SharedWorkerGlobalScope) {
  self.onconnect = function(e) {
    const port = e.ports[0];
    port.start();
    runTest(port);
  };
}
