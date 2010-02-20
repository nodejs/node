var events = require('events');

exports.resolve = function (domain, type_, callback_) {
  var type, callback;
  if (typeof(type_) == 'string') {
    type = type_;
    callback = callback_;
  } else {
    type = 'A';
    callback = arguments[1];
  }

  var resolveFunc = resolveMap[type];

  if (typeof(resolveFunc) == 'function') {
    resolveFunc(domain, callback);
  } else {
    throw new Error('Unknown type "' + type + '"');
  }
}

exports.resolve4    = process.dns.resolve4;
exports.resolve6    = process.dns.resolve6;
exports.resolveMx   = process.dns.resolveMx;
exports.resolveTxt  = process.dns.resolveTxt;
exports.resolveSrv  = process.dns.resolveSrv;
exports.reverse     = process.dns.reverse;

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

var resolveMap = {
  'A': exports.resolve4,
  'AAAA': exports.resolve6,
  'MX': exports.resolveMx,
  'TXT': exports.resolveTxt,
  'SRV': exports.resolveSrv,
  'PTR': exports.reverse,
};
