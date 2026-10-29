// This is the main driver of the canvas tainting tests.
const NOT_TAINTED = 'NOT_TAINTED';
const TAINTED = 'TAINTED';
const LOAD_ERROR = 'LOAD_ERROR';

let frame;

// Creates a single promise_test.
function canvas_taint_test(url, cross_origin, expected_result) {
  promise_test(t => {
      return frame.contentWindow.create_test_case_promise(url, cross_origin)
        .then(result => {
          assert_equals(result, expected_result);
        });
    }, 'url "' + url + '" with crossOrigin "' + cross_origin + '" should be ' +
           expected_result);
}


// Runs all the tests. The given |params| has these properties:
// * |resource_path|: the relative path to the (image/video) resource to test.
// * |cache|: when true, the service worker bounces responses into
//   Cache Storage and back out before responding with them.
function do_canvas_tainting_tests(params) {
  const host_info = get_host_info();
  let resource_path = params.resource_path;
  if (params.cache)
    resource_path += "&cache=true";
  const resource_url = host_info['HTTPS_ORIGIN'] + resource_path;
  const remote_resource_url = host_info['HTTPS_REMOTE_ORIGIN'] + resource_path;

  // Set up the service worker and the frame.
  promise_test(function(t) {
      const SCOPE = 'resources/fetch-canvas-tainting-iframe.html';
      const SCRIPT = 'resources/fetch-rewrite-worker.js';
      const host_info = get_host_info();

      // login_https() is needed because some test cases use credentials.
      return login_https(t)
        .then(function() {
            return service_worker_unregister_and_register(t, SCRIPT, SCOPE);
          })
        .then(function(registration) {
            promise_test(() => {
                if (frame)
                  frame.remove();
                return registration.unregister();
              }, 'restore global state');

            return wait_for_state(t, registration.installing, 'activated');
          })
        .then(function() { return with_iframe(SCOPE); })
        .then(f => {
            frame = f;
          });
    }, 'initialize global state');

  // Reject tests. Add '&reject' so the service worker responds with a rejected promise.
  // A load error is expected.
  canvas_taint_test(resource_url + '&reject', '', LOAD_ERROR);
  canvas_taint_test(resource_url + '&reject', 'anonymous', LOAD_ERROR);
  canvas_taint_test(resource_url + '&reject', 'use-credentials', LOAD_ERROR);

  // Fallback tests. Add '&ignore' so the service worker does not respond to the fetch
  // request, and we fall back to network.
  canvas_taint_test(resource_url + '&ignore', '', NOT_TAINTED);
  canvas_taint_test(remote_resource_url + '&ignore', '', TAINTED);
  canvas_taint_test(remote_resource_url + '&ignore', 'anonymous', LOAD_ERROR);
  canvas_taint_test(
      remote_resource_url + '&ACAOrigin=' + host_info['HTTPS_ORIGIN'] +
          '&ignore',
      'anonymous',
      NOT_TAINTED);
  canvas_taint_test(remote_resource_url + '&ignore', 'use-credentials', LOAD_ERROR);
  canvas_taint_test(
      remote_resource_url + '&ACAOrigin=' + host_info['HTTPS_ORIGIN'] +
          '&ignore',
      'use-credentials',
      LOAD_ERROR);
  canvas_taint_test(
      remote_resource_url + '&ACAOrigin=' + host_info['HTTPS_ORIGIN'] +
          '&ACACredentials=true&ignore',
      'use-credentials',
      NOT_TAINTED);

  // Credential tests (with fallback). Add '&Auth' so the server requires authentication.
  // Furthermore, add '&ignore' so the service worker falls back to network.
  canvas_taint_test(resource_url + '&Auth&ignore', '', NOT_TAINTED);
  canvas_taint_test(remote_resource_url + '&Auth&ignore', '', TAINTED);
  canvas_taint_test(
      remote_resource_url + '&Auth&ignore', 'anonymous', LOAD_ERROR);
  canvas_taint_test(
      remote_resource_url + '&Auth&ignore',
      'use-credentials',
      LOAD_ERROR);
  canvas_taint_test(
      remote_resource_url + '&Auth&ACAOrigin=' + host_info['HTTPS_ORIGIN'] +
      '&ignore',
      'use-credentials',
      LOAD_ERROR);
  canvas_taint_test(
      remote_resource_url + '&Auth&ACAOrigin=' + host_info['HTTPS_ORIGIN'] +
      '&ACACredentials=true&ignore',
      'use-credentials',
      NOT_TAINTED);

  // In the following tests, the service worker provides a response.
  // Add '&url' so the service worker responds with fetch(url).
  // Add '&mode' to configure the fetch request options.

  // Basic response tests. Set &url to the original url.
  canvas_taint_test(
      resource_url + '&mode=same-origin&url=' + encodeURIComponent(resource_url),
      '',
      NOT_TAINTED);
  canvas_taint_test(
      resource_url + '&mode=same-origin&url=' + encodeURIComponent(resource_url),
      'anonymous',
      NOT_TAINTED);
  canvas_taint_test(
      resource_url + '&mode=same-origin&url=' + encodeURIComponent(resource_url),
      'use-credentials',
      NOT_TAINTED);
  canvas_taint_test(
      remote_resource_url + '&mode=same-origin&url=' +
          encodeURIComponent(resource_url),
      '',
      NOT_TAINTED);
  canvas_taint_test(
      remote_resource_url + '&mode=same-origin&url=' +
          encodeURIComponent(resource_url),
      'anonymous',
      NOT_TAINTED);
  canvas_taint_test(
      remote_resource_url + '&mode=same-origin&url=' +
          encodeURIComponent(resource_url),
      'use-credentials',
      NOT_TAINTED);

  // Opaque response tests. Set &url to the cross-origin URL, and &mode to
  // 'no-cors' so we expect an opaque response.
  canvas_taint_test(
      resource_url +
          '&mode=no-cors&url=' + encodeURIComponent(remote_resource_url),
      '',
      TAINTED);
  canvas_taint_test(
      resource_url +
          '&mode=no-cors&url=' + encodeURIComponent(remote_resource_url),
      'anonymous',
      LOAD_ERROR);
  canvas_taint_test(
      resource_url +
          '&mode=no-cors&url=' + encodeURIComponent(remote_resource_url),
      'use-credentials',
      LOAD_ERROR);
  canvas_taint_test(
      remote_resource_url +
          '&mode=no-cors&url=' + encodeURIComponent(remote_resource_url),
      '',
      TAINTED);
  canvas_taint_test(
      remote_resource_url +
          '&mode=no-cors&url=' + encodeURIComponent(remote_resource_url),
      'anonymous',
      LOAD_ERROR);
  canvas_taint_test(
      remote_resource_url +
          '&mode=no-cors&url=' + encodeURIComponent(remote_resource_url),
      'use-credentials',
      LOAD_ERROR);

  // CORS response tests. Set &url to the cross-origin URL, and &mode
  // to 'cors' to attempt a CORS request.
  canvas_taint_test(
      resource_url + '&mode=cors&url=' +
          encodeURIComponent(remote_resource_url +
                             '&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      '',
      LOAD_ERROR); // We expect LOAD_ERROR since the server doesn't respond
                   // with an Access-Control-Allow-Credentials header.
  canvas_taint_test(
      resource_url + '&mode=cors&credentials=same-origin&url=' +
          encodeURIComponent(remote_resource_url +
                             '&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      '',
      NOT_TAINTED);
  canvas_taint_test(
      resource_url + '&mode=cors&url=' +
          encodeURIComponent(remote_resource_url +
                             '&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      'anonymous',
      NOT_TAINTED);
  canvas_taint_test(
      resource_url + '&mode=cors&url=' +
          encodeURIComponent(remote_resource_url +
                             '&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      'use-credentials',
      LOAD_ERROR); // We expect LOAD_ERROR since the server doesn't respond
                   // with an Access-Control-Allow-Credentials header.
  canvas_taint_test(
      resource_url + '&mode=cors&url=' +
          encodeURIComponent(
              remote_resource_url +
              '&ACACredentials=true&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      'use-credentials',
      NOT_TAINTED);
  canvas_taint_test(
      remote_resource_url + '&mode=cors&url=' +
          encodeURIComponent(remote_resource_url +
                             '&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      '',
      LOAD_ERROR); // We expect LOAD_ERROR since the server doesn't respond
                   // with an Access-Control-Allow-Credentials header.
  canvas_taint_test(
      remote_resource_url + '&mode=cors&credentials=same-origin&url=' +
          encodeURIComponent(remote_resource_url +
                             '&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      '',
      NOT_TAINTED);
  canvas_taint_test(
      remote_resource_url + '&mode=cors&url=' +
          encodeURIComponent(remote_resource_url +
                             '&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      'anonymous',
      NOT_TAINTED);
  canvas_taint_test(
      remote_resource_url + '&mode=cors&url=' +
          encodeURIComponent(remote_resource_url +
                             '&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      'use-credentials',
      LOAD_ERROR); // We expect LOAD_ERROR since the server doesn't respond
                   // with an Access-Control-Allow-Credentials header.
  canvas_taint_test(
      remote_resource_url + '&mode=cors&url=' +
          encodeURIComponent(
              remote_resource_url +
              '&ACACredentials=true&ACAOrigin=' + host_info['HTTPS_ORIGIN']),
      'use-credentials',
      NOT_TAINTED);
}
