var events = require('events');

function callback (promise) {
  return function () {
    if (arguments[0] instanceof Error) {
      promise.emitError.apply(promise, arguments);
    } else {
      promise.emitSuccess.apply(promise, arguments);
    }
  }
}

exports.resolve4 = function (domain) {
  var promise = new events.Promise();
  process.dns.resolve4(domain, callback(promise));
  return promise;
};

exports.resolve6 = function (domain) {
  var promise = new events.Promise();
  process.dns.resolve6(domain, callback(promise));
  return promise;
};

exports.reverse = function (ip) {
  var promise = new events.Promise();
  process.dns.reverse(ip, callback(promise));
  return promise;
};

// ERROR CODES

// timeout, SERVFAIL or similar.
exports.TEMPFAIL = process.dns.TEMPFAIL;

// got garbled reply.
exports.PROTOCOL = process.dns.PROTOCOL;

// domain does not exists.
exports.NXDOMAIN = process.dns.NXDOMAIN;

// domain exists but no data of reqd type.
exports.NODATA = process.dns.NODATA;

// out of memory while processing.
exports.NOMEM = process.dns.NOMEM;

// the query is malformed.
exports.BADQUERY = process.dns.BADQUERY;
