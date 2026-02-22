// META: global=window,worker
// META: script=/common/get-host-info.sub.js

const authorizationValue = "Basic " + btoa("user:pass");
async function getAuthorizationHeaderValue(url)
{
  const headers = { "Authorization": authorizationValue};
  const requestInit = {"headers": headers};
  const response = await fetch(url, requestInit);
  return response.text();
}

promise_test(async test => {
  const result = await getAuthorizationHeaderValue("/fetch/api/resources/dump-authorization-header.py");
  assert_equals(result, authorizationValue);
}, "getAuthorizationHeaderValue - no redirection");

promise_test(async test => {
  result = await getAuthorizationHeaderValue("/fetch/api/resources/redirect.py?location=" + encodeURIComponent("/fetch/api/resources/dump-authorization-header.py"));
  assert_equals(result, authorizationValue);

  result = await getAuthorizationHeaderValue(get_host_info().HTTPS_REMOTE_ORIGIN + "/fetch/api/resources/redirect.py?allow_headers=Authorization&location=" + encodeURIComponent(get_host_info().HTTPS_REMOTE_ORIGIN + "/fetch/api/resources/dump-authorization-header.py"));
  assert_equals(result, authorizationValue);
}, "getAuthorizationHeaderValue - same origin redirection");

promise_test(async (test) => {
  const result = await getAuthorizationHeaderValue(get_host_info().HTTPS_REMOTE_ORIGIN + "/fetch/api/resources/redirect.py?allow_headers=Authorization&location=" + encodeURIComponent(get_host_info().HTTPS_ORIGIN + "/fetch/api/resources/dump-authorization-header.py?strip_auth_header=true"));
  assert_equals(result, "none");
}, "getAuthorizationHeaderValue - cross origin redirection");
