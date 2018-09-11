function get_host_info() {

  var HTTP_PORT = '{{ports[http][0]}}';
  var HTTP_PORT2 = '{{ports[http][1]}}';
  var HTTPS_PORT = '{{ports[https][0]}}';
  var ORIGINAL_HOST = '{{host}}';
  var REMOTE_HOST = (ORIGINAL_HOST === 'localhost') ? '127.0.0.1' : ('www1.' + ORIGINAL_HOST);
  var OTHER_HOST = '{{domains[www2]}}';
  var NOTSAMESITE_HOST = (ORIGINAL_HOST === 'localhost') ? '127.0.0.1' : ('not-' + ORIGINAL_HOST);

  return {
    HTTP_PORT: HTTP_PORT,
    HTTP_PORT2: HTTP_PORT2,
    HTTPS_PORT: HTTPS_PORT,
    ORIGINAL_HOST: ORIGINAL_HOST,
    REMOTE_HOST: REMOTE_HOST,

    HTTP_ORIGIN: 'http://' + ORIGINAL_HOST + ':' + HTTP_PORT,
    HTTPS_ORIGIN: 'https://' + ORIGINAL_HOST + ':' + HTTPS_PORT,
    HTTPS_ORIGIN_WITH_CREDS: 'https://foo:bar@' + ORIGINAL_HOST + ':' + HTTPS_PORT,
    HTTP_ORIGIN_WITH_DIFFERENT_PORT: 'http://' + ORIGINAL_HOST + ':' + HTTP_PORT2,
    HTTP_REMOTE_ORIGIN: 'http://' + REMOTE_HOST + ':' + HTTP_PORT,
    HTTP_NOTSAMESITE_ORIGIN: 'http://' + NOTSAMESITE_HOST + ':' + HTTP_PORT,
    HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT: 'http://' + REMOTE_HOST + ':' + HTTP_PORT2,
    HTTPS_REMOTE_ORIGIN: 'https://' + REMOTE_HOST + ':' + HTTPS_PORT,
    HTTPS_REMOTE_ORIGIN_WITH_CREDS: 'https://foo:bar@' + REMOTE_HOST + ':' + HTTPS_PORT,
    UNAUTHENTICATED_ORIGIN: 'http://' + OTHER_HOST + ':' + HTTP_PORT,
    AUTHENTICATED_ORIGIN: 'https://' + OTHER_HOST + ':' + HTTPS_PORT
  };
}

function get_port(loc) {
  // When a default port is used, location.port returns the empty string.
  // To compare with wptserve `ports` substitution we need a port...
  // loc can be Location/<a>/<area>/URL, but assumes http/https only.
  if (loc.port) {
    return loc.port;
  }
  return loc.protocol === 'https:' ? '443' : '80';
}
