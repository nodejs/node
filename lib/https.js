var tls = require('tls');
var http = require('http');
var inherits = require('util').inherits;


function Server(opts, requestListener) {
  if (!(this instanceof Server)) return new Server(opts, requestListener);
  tls.Server.call(this, opts, http._connectionListener);

  this.httpAllowHalfOpen = false;

  if (requestListener) {
    this.addListener('request', requestListener);
  }
}
inherits(Server, tls.Server);


exports.Server = Server;


exports.createServer = function(opts, requestListener) {
  return new Server(opts, requestListener);
};


// HTTPS agents.
var agents = {};

function Agent(options) {
  http.Agent.call(this, options);
}
inherits(Agent, http.Agent);


Agent.prototype._getConnection = function(host, port, cb) {
  var s = tls.connect(port, host, this.options, function() {
    // do other checks here?
    if (cb) cb();
  });

  return s;
};


function getAgent(options) {
  if (!options.port) options.port = 443;

  var id = options.host + ':' + options.port;
  var agent = agents[id];

  if (!agent) {
    agent = agents[id] = new Agent(options);
  }

  return agent;
}
exports.getAgent = getAgent;


exports.request = function(options, cb) {
  if (options.agent === undefined) {
    options.agent = getAgent(options);
  } else if (options.agent === false) {
    options.agent = new Agent(options);
  }
  return http._requestFromAgent(options, cb);
};


exports.get = function(options, cb) {
  options.method = 'GET';
  var req = exports.request(options, cb);
  req.end();
  return req;
};
