
/*!
 * Connect
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var HTTPServer = require('./http').Server
  , HTTPSServer = require('./https').Server
  , fs = require('fs');

// node patches

require('./patch');

// expose createServer() as the module

exports = module.exports = createServer;

/**
 * Framework version.
 */

exports.version = '1.6.4';

/**
 * Initialize a new `connect.HTTPServer` with the middleware
 * passed to this function. When an object is passed _first_,
 * we assume these are the tls options, and return a `connect.HTTPSServer`.
 *
 * Examples:
 *
 * An example HTTP server, accepting several middleware.
 *
 *     var server = connect.createServer(
 *         connect.logger()
 *       , connect.static(__dirname + '/public')
 *     );
 *
 * An HTTPS server, utilizing the same middleware as above.
 *
 *     var server = connect.createServer(
 *         { key: key, cert: cert }
 *       , connect.logger()
 *       , connect.static(__dirname + '/public')
 *     );
 *
 * Alternatively with connect 1.0 we may omit `createServer()`.
 *
 *     connect(
 *         connect.logger()
 *       , connect.static(__dirname + '/public')
 *     ).listen(3000);
 *
 * @param  {Object|Function} ...
 * @return {Server}
 * @api public
 */

function createServer() {
  if ('object' == typeof arguments[0]) {
    return new HTTPSServer(arguments[0], Array.prototype.slice.call(arguments, 1));
  } else {
    return new HTTPServer(Array.prototype.slice.call(arguments));
  }
};

// support connect.createServer()

exports.createServer = createServer;

// auto-load getters

exports.middleware = {};

/**
 * Auto-load bundled middleware with getters.
 */

fs.readdirSync(__dirname + '/middleware').forEach(function(filename){
  if (/\.js$/.test(filename)) {
    var name = filename.substr(0, filename.lastIndexOf('.'));
    exports.middleware.__defineGetter__(name, function(){
      return require('./middleware/' + name);
    });
  }
});

// expose utils

exports.utils = require('./utils');

// expose getters as first-class exports

exports.utils.merge(exports, exports.middleware);

// expose constructors

exports.HTTPServer = HTTPServer;
exports.HTTPSServer = HTTPSServer;

