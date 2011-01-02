var tls = require('tls');
var http = require('http');
var inherits = require('util').inherits;


function Server(opts, requestListener) {
  if (!(this instanceof Server)) return new Server(opts, requestListener);
  tls.Server.call(this, opts, http._connectionListener);

  if (requestListener) {
    this.addListener('request', requestListener);
  }
}
inherits(Server, tls.Server);


exports.Server = Server;


exports.createServer = function(opts, requestListener) {
  return new Server(opts, requestListener);
};
