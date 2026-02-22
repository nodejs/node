// META: global=window,worker
// META: script=/common/get-host-info.sub.js

const BASE = location.href;
const IS_HTTPS = new URL(BASE).protocol === 'https:';
const REMOTE_HOST = get_host_info()['REMOTE_HOST'];
const REMOTE_PORT =
  IS_HTTPS ? get_host_info()['HTTPS_PORT'] : get_host_info()['HTTP_PORT'];

const REMOTE_ORIGIN =
  new URL(`//${REMOTE_HOST}:${REMOTE_PORT}`, BASE).origin;
const DESTINATION = new URL('../resources/cors-top.txt', BASE);

function CreateURL(url, BASE, params) {
  const u = new URL(url, BASE);
  for (const {name, value} of params) {
    u.searchParams.append(name, value);
  }
  return u;
}

const redirect =
  CreateURL('/fetch/api/resources/redirect.py', REMOTE_ORIGIN,
            [{name: 'redirect_status', value: 303},
             {name: 'location', value: DESTINATION.href}]);

promise_test(async (test) => {
  const res = await fetch(redirect.href, {mode: 'no-cors'});
  // This is discussed at https://github.com/whatwg/fetch/issues/737.
  assert_equals(res.type, 'opaque');
}, 'original => remote => original with mode: "no-cors"');

promise_test(async (test) => {
  const res = await fetch(redirect.href, {mode: 'cors'});
  assert_equals(res.type, 'cors');
}, 'original => remote => original with mode: "cors"');

done();
