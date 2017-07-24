'use strict';

const url = require('url');
const https = require('https');

/**
 * This currently needs to be applied to all Node.js versions
 * in order to determine if the `req` is an HTTP or HTTPS request.
 *
 * There is currently no PR attempting to move this property upstream.
 */
https.request = (function(request) {
  return function(_options, cb) {
    let options
    if (typeof options === 'string') {
      options = url.parse(_options);
    } else {
      options = Object.assign({}, _options);
    }
    if (null == options.port) {
      options.port = 443;
    }
    options.secureEndpoint = true;
    return request.call(https, options, cb);
  };
})(https.request);
