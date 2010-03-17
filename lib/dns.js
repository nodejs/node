var dns = process.binding('dns');

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

exports.resolve4    = dns.resolve4;
exports.resolve6    = dns.resolve6;
exports.resolveMx   = dns.resolveMx;
exports.resolveTxt  = dns.resolveTxt;
exports.resolveSrv  = dns.resolveSrv;
exports.reverse     = dns.reverse;

// ERROR CODES

// timeout, SERVFAIL or similar.
exports.TEMPFAIL = dns.TEMPFAIL;

// got garbled reply.
exports.PROTOCOL = dns.PROTOCOL;

// domain does not exists.
exports.NXDOMAIN = dns.NXDOMAIN;

// domain exists but no data of reqd type.
exports.NODATA = dns.NODATA;

// out of memory while processing.
exports.NOMEM = dns.NOMEM;

// the query is malformed.
exports.BADQUERY = dns.BADQUERY;

var resolveMap = {
  'A': exports.resolve4,
  'AAAA': exports.resolve6,
  'MX': exports.resolveMx,
  'TXT': exports.resolveTxt,
  'SRV': exports.resolveSrv,
  'PTR': exports.reverse,
};
