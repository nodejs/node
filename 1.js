'use strict';

var options = {
  key: "-----BEGIN RSA PRIVATE KEY-----\nMIICXQIBAAKBgQDN9WSJABq8qOoQSmzWcSOvdcjJ/3hASjWe/spyRuWviC4G3YQR\n1ap8kwCCmtuWsaEctxG2xxs7l0w4BlrQNm5IynHMl28EF8Uej3qIASfhF15hjSFJ\n/RxWGNg3/3yF53g7tIAsAMk7h1d8CaudN8h51G3lMdLVrVUPaQiTAE2mxQIDAQAB\nAoGAI376f74b3Y4DISGilmbTbqcPHvk/oVzo1uk0vPNJHLKMtDQzUduQUX4IZXoJ\nBHTCvq8yh1zTbbbKtRErT51B7kxNOVHefzCiibZNFlrk9ATanAbmBoCat5DMkw3I\nPqCnQ1gDyZjuj+P5IVYHOezIn1/5nInw7f4akhCGHU32MfUCQQDU1vI7rvVxkFPP\nI/dnEnggG5JYmufxZzlskiBrowEOaZIfvcgTUT5VhRuSakE+FBDGh1oyRUyDDhlP\nVBsZwjh/AkEA97k7hUyUB3erEccUyLLGPHzsxXc711Lsf5NQkljXThntTCkjnGpZ\nSulOgvuko4sFSuRgzm1r0DeAA6AS53CeuwJBAKTobgLkSnPVGbqS6WvJGZ32/ur8\nCt411n5Ssh/zyiu6jGdfihe9iQiF+5j0DtzkeyL3WGE+5EterymRxvWsUE0CQQCx\nKgJNZOUBKi5oOm680k4/+EAFQS7E4gNNgfe/klX4/0XckBdtyAkwMAb8Wif25nfU\nhdxOBadzdB3TeenLJ5n9AkA+gAl04OMKdURMtlNewEAbHDO02raUQ2Ui4Bl2PKfh\nHZ9EBnruiJTxDYjxoLBcJ8MzIvzL9MfUXAlcfw07BPI4\n-----END RSA PRIVATE KEY-----",
  cert: "-----BEGIN CERTIFICATE-----\nMIIBtDCCAR+gAwIBAgIDAQABMAsGCSqGSIb3DQEBCzAUMRIwEAYDVQQDFgl0ZXN0\nLnRlc3QwHhcNNjkwMTAxMDAwMDAwWhcNMjUwNzE1MDMwMTM1WjAUMRIwEAYDVQQD\nFgl0ZXN0LnRlc3QwgZ0wCwYJKoZIhvcNAQEBA4GNADCBiQKBgQDN9WSJABq8qOoQ\nSmzWcSOvdcjJ/3hASjWe/spyRuWviC4G3YQR1ap8kwCCmtuWsaEctxG2xxs7l0w4\nBlrQNm5IynHMl28EF8Uej3qIASfhF15hjSFJ/RxWGNg3/3yF53g7tIAsAMk7h1d8\nCaudN8h51G3lMdLVrVUPaQiTAE2mxQIDAQABoxowGDAWBgNVHREEDzANggsqLnRl\nc3QudGVzdDALBgkqhkiG9w0BAQsDgYEAvKZ+uTM0kZAYBZg+wwTLZwzW3LSyeVFA\nQgxVPmXpNSfvFvBsPrHLbtGsqbDw9Yx+l6XWl9iAqW7FR3nJseKRzJYnlnvIO56g\nr7Pz4K308QJKSNIT4Cm54mxO1ZLaVdpumtoTxncplAJgUC/MZqvFK5ig8tvDGMCe\nHOtVT2sWjMQ=\n-----END CERTIFICATE-----"
};

var Https = require('https');
var Fs = require('fs');
var C = require('constants');

var server = Https.createServer({
    secureOptions: C.SSL_OP_NO_TICKET,
    cert: options.cert,
    key: options.key
}, function (req, res) {
    res.writeHead(200);
    res.end("hello world\n");
});

server.on('resumeSession', function (id, callback) {
    console.log('resume requested');
    callback();
});

server.listen(8000);

