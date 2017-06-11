var url = require('url');
var http = require('http');
var https = require('https');
var semver = require('semver');
var inherits = require('util').inherits;


// we only need to patch the `http.request()` and
// `http.ClientRequest` on older versions of Node.js
if (semver.lt(process.version, '0.11.8')) {
  // subclass the native ClientRequest to include the
  // passed in `options` object.
  http.ClientRequest = (function (_ClientRequest) {
    function ClientRequest (options, cb) {
      this._options = options;
      _ClientRequest.call(this, options, cb);
    }
    inherits(ClientRequest, _ClientRequest);

    return ClientRequest;
  })(http.ClientRequest);


  // need to re-define the `request()` method, since on node v0.8/v0.10
  // the closure-local ClientRequest is used, rather than the monkey
  // patched version we have created here.
  http.request = (function (request) {
    return function (options, cb) {
      if (typeof options === 'string') {
        options = url.parse(options);
      }
      if (options.protocol && options.protocol !== 'http:') {
        throw new Error('Protocol:' + options.protocol + ' not supported.');
      }
      return new http.ClientRequest(options, cb);
    };
  })(http.request);
}


// this currently needs to be applied to all Node.js versions
// (v0.8.x, v0.10.x, v0.12.x), in order to determine if the `req`
// is an HTTP or HTTPS request. There is currently no PR attempting
// to move this property upstream.
https.request = (function (request) {
  return function (options, cb) {
    if (typeof options === 'string') {
      options = url.parse(options);
    }
    if (null == options.port) options.port = 443;
    options.secureEndpoint = true;
    return request.call(https, options, cb);
  };
})(https.request);
