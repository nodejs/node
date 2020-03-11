/**
 * Host information for cross-origin tests.
 * @returns {Object} with properties for different host information.
 */
function get_host_info() {

  var HTTP_PORT = '{{ports[http][0]}}';
  var HTTP_PORT2 = '{{ports[http][1]}}';
  var HTTPS_PORT = '{{ports[https][0]}}';
  var PROTOCOL = self.location.protocol;
  var IS_HTTPS = (PROTOCOL == "https:");
  var HTTP_PORT_ELIDED = HTTP_PORT == "80" ? "" : (":" + HTTP_PORT);
  var HTTP_PORT2_ELIDED = HTTP_PORT2 == "80" ? "" : (":" + HTTP_PORT2);
  var HTTPS_PORT_ELIDED = HTTPS_PORT == "443" ? "" : (":" + HTTPS_PORT);
  var PORT_ELIDED = IS_HTTPS ? HTTPS_PORT_ELIDED : HTTP_PORT_ELIDED;
  var ORIGINAL_HOST = '{{host}}';
  var REMOTE_HOST = (ORIGINAL_HOST === 'localhost') ? '127.0.0.1' : ('www1.' + ORIGINAL_HOST);
  var OTHER_HOST = '{{domains[www2]}}';
  var NOTSAMESITE_HOST = (ORIGINAL_HOST === 'localhost') ? '127.0.0.1' : ('{{hosts[alt][]}}');

  return {
    HTTP_PORT: HTTP_PORT,
    HTTP_PORT2: HTTP_PORT2,
    HTTPS_PORT: HTTPS_PORT,
    ORIGINAL_HOST: ORIGINAL_HOST,
    REMOTE_HOST: REMOTE_HOST,

    ORIGIN: PROTOCOL + "//" + ORIGINAL_HOST + PORT_ELIDED,
    HTTP_ORIGIN: 'http://' + ORIGINAL_HOST + HTTP_PORT_ELIDED,
    HTTPS_ORIGIN: 'https://' + ORIGINAL_HOST + HTTPS_PORT_ELIDED,
    HTTPS_ORIGIN_WITH_CREDS: 'https://foo:bar@' + ORIGINAL_HOST + HTTPS_PORT_ELIDED,
    HTTP_ORIGIN_WITH_DIFFERENT_PORT: 'http://' + ORIGINAL_HOST + HTTP_PORT2_ELIDED,
    REMOTE_ORIGIN: PROTOCOL + "//" + REMOTE_HOST + PORT_ELIDED,
    HTTP_REMOTE_ORIGIN: 'http://' + REMOTE_HOST + HTTP_PORT_ELIDED,
    HTTP_NOTSAMESITE_ORIGIN: 'http://' + NOTSAMESITE_HOST + HTTP_PORT_ELIDED,
    HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT: 'http://' + REMOTE_HOST + HTTP_PORT2_ELIDED,
    HTTPS_REMOTE_ORIGIN: 'https://' + REMOTE_HOST + HTTPS_PORT_ELIDED,
    HTTPS_REMOTE_ORIGIN_WITH_CREDS: 'https://foo:bar@' + REMOTE_HOST + HTTPS_PORT_ELIDED,
    HTTPS_NOTSAMESITE_ORIGIN: 'https://' + NOTSAMESITE_HOST + HTTPS_PORT_ELIDED,
    UNAUTHENTICATED_ORIGIN: 'http://' + OTHER_HOST + HTTP_PORT_ELIDED,
    AUTHENTICATED_ORIGIN: 'https://' + OTHER_HOST + HTTPS_PORT_ELIDED
  };
}

/**
 * When a default port is used, location.port returns the empty string.
 * This function attempts to provide an exact port, assuming we are running under wptserve.
 * @param {*} loc - can be Location/<a>/<area>/URL, but assumes http/https only.
 * @returns {string} The port number.
 */
function get_port(loc) {
  if (loc.port) {
    return loc.port;
  }
  return loc.protocol === 'https:' ? '443' : '80';
}
