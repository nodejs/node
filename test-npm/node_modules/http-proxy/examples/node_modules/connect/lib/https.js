
/*!
 * Connect - HTTPServer
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var HTTPServer = require('./http').Server
  , https = require('https');

/**
 * Initialize a new `Server` with the given
 *`options` and `middleware`. The HTTPS api
 * is identical to the [HTTP](http.html) server,
 * however TLS `options` must be provided before
 * passing in the optional middleware.
 *
 * @params {Object} options
 * @params {Array} middleawre
 * @return {Server}
 * @api public
 */

var Server = exports.Server = function HTTPSServer(options, middleware) {
  this.stack = [];
  middleware.forEach(function(fn){
    this.use(fn);
  }, this);
  https.Server.call(this, options, this.handle);
};

/**
 * Inherit from `http.Server.prototype`.
 */

Server.prototype.__proto__ = https.Server.prototype;

// mixin HTTPServer methods

Object.keys(HTTPServer.prototype).forEach(function(method){
  Server.prototype[method] = HTTPServer.prototype[method];
});