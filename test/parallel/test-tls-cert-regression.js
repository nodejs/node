'use strict';
var assert = require('assert');
var common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');


var cert = '-----BEGIN CERTIFICATE-----\n' +
  'MIIBfjCCASgCCQDmmNjAojbDQjANBgkqhkiG9w0BAQUFADBFMQswCQYDVQQGEwJB\n' +
  'VTETMBEGA1UECBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJuZXQgV2lkZ2l0\n' +
  'cyBQdHkgTHRkMCAXDTE0MDExNjE3NTMxM1oYDzIyODcxMDMxMTc1MzEzWjBFMQsw\n' +
  'CQYDVQQGEwJBVTETMBEGA1UECBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJu\n' +
  'ZXQgV2lkZ2l0cyBQdHkgTHRkMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAPKwlfMX\n' +
  '6HGZIt1xm7fna72eWcOYfUfSxSugghvqYgJt2Oi3lH+wsU1O9FzRIVmpeIjDXhbp\n' +
  'Mjsa1HtzSiccPXsCAwEAATANBgkqhkiG9w0BAQUFAANBAHOoKy0NkyfiYH7Ne5ka\n' +
  'uvCyndyeB4d24FlfqEUlkfaWCZlNKRaV9YhLDiEg3BcIreFo4brtKQfZzTRs0GVm\n' +
  'KHg=\n' +
  '-----END CERTIFICATE-----';
var key = '-----BEGIN RSA PRIVATE KEY-----\n' +
  'MIIBPQIBAAJBAPKwlfMX6HGZIt1xm7fna72eWcOYfUfSxSugghvqYgJt2Oi3lH+w\n' +
  'sU1O9FzRIVmpeIjDXhbpMjsa1HtzSiccPXsCAwEAAQJBAM4uU9aJE0OfdE1p/X+K\n' +
  'LrCT3XMdFCJ24GgmHyOURtwDy18upQJecDVdcZp16fjtOPmaW95GoYRyifB3R4I5\n' +
  'RxECIQD7jRM9slCSVV8xp9kOJQNpHjhRQYVGBn+pyllS2sb+RQIhAPb7Y+BIccri\n' +
  'NWnuhwCW8hA7Fkj/kaBdAwyW7L3Tvui/AiEAiqLCovMecre4Yi6GcsQ1b/6mvSmm\n' +
  'IOS+AT6zIfXPTB0CIQCJKGR3ymN/Qw5crL1GQ41cHCQtF9ickOq/lBUW+j976wIh\n' +
  'AOaJnkQrmurlRdePX6LvN/LgGAQoxwovfjcOYNnZsIVY\n' +
  '-----END RSA PRIVATE KEY-----';

function test(cert, key, cb) {
  var server = tls.createServer({
    cert: cert,
    key: key
  }).listen(common.PORT, function() {
    server.close(cb);
  });
}

var completed = false;
test(cert, key, function() {
  test(new Buffer(cert), new Buffer(key), function() {
    completed = true;
  });
});

process.on('exit', function() {
  assert(completed);
});
