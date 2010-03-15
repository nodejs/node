var tcp = process.binding('tcp');

var TLS_STATUS_CODES = {
  1 : 'JS_GNUTLS_CERT_VALIDATED',
  0 : 'JS_GNUTLS_CERT_UNDEFINED',
}
TLS_STATUS_CODES[-100] = 'JS_GNUTLS_CERT_SIGNER_NOT_FOUND';
TLS_STATUS_CODES[-101] = 'JS_GNUTLS_CERT_SIGNER_NOT_CA';
TLS_STATUS_CODES[-102] = 'JS_GNUTLS_CERT_INVALID';
TLS_STATUS_CODES[-103] = 'JS_GNUTLS_CERT_NOT_ACTIVATED';
TLS_STATUS_CODES[-104] = 'JS_GNUTLS_CERT_EXPIRED';
TLS_STATUS_CODES[-105] = 'JS_GNUTLS_CERT_REVOKED';
TLS_STATUS_CODES[-106] = 'JS_GNUTLS_CERT_DOES_NOT_MATCH_HOSTNAME';

exports.createServer = function (on_connection, options) {
  var server = new tcp.Server();
  server.addListener("connection", on_connection);
  //server.setOptions(options);
  return server;
};

exports.createConnection = function (port, host) {
  var connection = new tcp.Connection();
  connection.connect(port, host);
  return connection;
};
