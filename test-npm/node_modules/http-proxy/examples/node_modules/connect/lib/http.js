
/*!
 * Connect - HTTPServer
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var http = require('http')
  , parse = require('url').parse
  , assert = require('assert');

// environment

var env = process.env.NODE_ENV || 'development';

/**
 * Initialize a new `Server` with the given `middleware`.
 *
 * Examples:
 *
 *     var server = connect.createServer(
 *         connect.favicon()
 *       , connect.logger()
 *       , connect.static(__dirname + '/public')
 *     );
 *
 * @params {Array} middleware 
 * @return {Server}
 * @api public
 */

var Server = exports.Server = function HTTPServer(middleware) {
  this.stack = [];
  middleware.forEach(function(fn){
    this.use(fn);
  }, this);
  http.Server.call(this, this.handle);
};

/**
 * Inherit from `http.Server.prototype`.
 */

Server.prototype.__proto__ = http.Server.prototype;

/**
 * Utilize the given middleware `handle` to the given `route`,
 * defaulting to _/_. This "route" is the mount-point for the
 * middleware, when given a value other than _/_ the middleware
 * is only effective when that segment is present in the request's
 * pathname.
 *
 * For example if we were to mount a function at _/admin_, it would
 * be invoked on _/admin_, and _/admin/settings_, however it would
 * not be invoked for _/_, or _/posts_.
 *
 * This is effectively the same as passing middleware to `connect.createServer()`,
 * however provides a progressive api.
 *
 * Examples:
 *
 *      var server = connect.createServer();
 *      server.use(connect.favicon());
 *      server.use(connect.logger());
 *      server.use(connect.static(__dirname + '/public'));
 *
 * If we wanted to prefix static files with _/public_, we could
 * "mount" the `static()` middleware:
 *
 *      server.use('/public', connect.static(__dirname + '/public'));
 *
 * This api is chainable, meaning the following is valid:
 *
 *      connect.createServer()
 *        .use(connect.favicon())
 *        .use(connect.logger())
 *        .use(connect.static(__dirname + '/public'))
 *        .listen(3000);
 *
 * @param {String|Function} route or handle
 * @param {Function} handle
 * @return {Server}
 * @api public
 */

Server.prototype.use = function(route, handle){
  this.route = '/';

  // default route to '/'
  if ('string' != typeof route) {
    handle = route;
    route = '/';
  }

  // wrap sub-apps
  if ('function' == typeof handle.handle) {
    var server = handle;
    server.route = route;
    handle = function(req, res, next) {
      server.handle(req, res, next);
    };
  }

  // wrap vanilla http.Servers
  if (handle instanceof http.Server) {
    handle = handle.listeners('request')[0];
  }

  // normalize route to not trail with slash
  if ('/' == route[route.length - 1]) {
    route = route.substr(0, route.length - 1);
  }

  // add the middleware
  this.stack.push({ route: route, handle: handle });

  // allow chaining
  return this;
};

/**
 * Handle server requests, punting them down
 * the middleware stack.
 *
 * @api private
 */

Server.prototype.handle = function(req, res, out) {
  var writeHead = res.writeHead
    , stack = this.stack
    , removed = ''
    , index = 0;

  function next(err) {
    var layer, path, c;
    req.url = removed + req.url;
    req.originalUrl = req.originalUrl || req.url;
    removed = '';

    layer = stack[index++];

    // all done
    if (!layer) {
      // but wait! we have a parent
      if (out) return out(err);

      // otherwise send a proper error message to the browser.
      if (err) {
        var msg = 'production' == env
          ? 'Internal Server Error'
          : err.stack || err.toString();

        // output to stderr in a non-test env
        if ('test' != env) console.error(err.stack || err.toString());

        res.statusCode = 500;
        res.setHeader('Content-Type', 'text/plain');
        res.end(msg);
      } else {
        res.statusCode = 404;
        res.setHeader('Content-Type', 'text/plain');
        res.end('Cannot ' + req.method + ' ' + req.url);
      }
      return;
    }

    try {
      path = parse(req.url).pathname;
      if (undefined == path) path = '/';

      // skip this layer if the route doesn't match.
      if (0 != path.indexOf(layer.route)) return next(err);

      c = path[layer.route.length];
      if (c && '/' != c && '.' != c) return next(err);

      // Call the layer handler
      // Trim off the part of the url that matches the route
      removed = layer.route;
      req.url = req.url.substr(removed.length);

      // Ensure leading slash
      if ('/' != req.url[0]) req.url = '/' + req.url;

      var arity = layer.handle.length;
      if (err) {
        if (arity === 4) {
          layer.handle(err, req, res, next);
        } else {
          next(err);
        }
      } else if (arity < 4) {
        layer.handle(req, res, next);
      } else {
        next();
      }
    } catch (e) {
      if (e instanceof assert.AssertionError) {
        console.error(e.stack + '\n');
        next(e);
      } else {
        next(e);
      }
    }
  }
  next();
};