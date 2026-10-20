importScripts('/resources/testharness.js');

let echo_output = null;

// Tests importing a script that sets |echo_output| to the query string.
function test_import(str) {
  echo_output = null;
  importScripts('import-scripts-echo.py?msg=' + str);
  assert_equals(echo_output, str);
}

test_import('root');
test_import('root-and-message');

self.addEventListener('install', () => {
    test_import('install');
    test_import('install-and-message');
  });

self.addEventListener('message', e => {
    var error = null;
    echo_output = null;

    try {
      importScripts('import-scripts-echo.py?msg=' + e.data);
    } catch (e) {
      error = e && e.name;
    }

    e.source.postMessage({ error: error, value: echo_output });
  });
