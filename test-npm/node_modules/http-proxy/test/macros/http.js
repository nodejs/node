/*
 * http.js: Macros for proxying HTTP requests
 *
 * (C) 2010 Nodejitsu Inc.
 * MIT LICENCE
 *
 */

var assert = require('assert'),
    fs = require('fs'),
    async = require('async'),
    request = require('request'),
    helpers = require('../helpers/index');

//
// ### function assertRequest (options)
// #### @options {Object} Options for this request assertion.
// ####    @request {Object} Options to use for `request`.
// ####    @assert  {Object} Test assertions against the response.
//
// Makes a request using `options.request` and then asserts the response
// and body against anything in `options.assert`.
//
exports.assertRequest = function (options) {
  return {
    topic: function () {
      //
      // Now make the HTTP request and assert.
      //
      options.request.rejectUnauthorized = false;
      request(options.request, this.callback);
    },
    "should succeed": function (err, res, body) {
      assert.isNull(err);
      if (options.assert.body) {
        assert.equal(body, options.assert.body);
      }

      if (options.assert.statusCode) {
        assert.equal(res.statusCode, options.assert.statusCode);
      }
    }
  };
};

//
// ### function assertProxied (options)
// #### @options {Object} Options for this test
// ####    @latency {number} Latency in milliseconds for the proxy server.
// ####    @ports   {Object} Ports for the request (target, proxy).
// ####    @output  {string} Output to assert from.
// ####    @forward {Object} Options for forward proxying.
//
// Creates a complete end-to-end test for requesting against an
// http proxy.
//
exports.assertProxied = function (options) {
  options = options || {};

  var ports    = options.ports   || helpers.nextPortPair,
      output   = options.output  || 'hello world from ' + ports.target,
      protocol = helpers.protocols.proxy,
      req      = options.request || {};

  req.uri = req.uri || protocol + '://127.0.0.1:' + ports.proxy;

  return {
    topic: function () {
      //
      // Create a target server and a proxy server
      // using the `options` supplied.
      //
      helpers.http.createServerPair({
        target: {
          output: output,
          port: ports.target,
          headers: req.headers
        },
        proxy: {
          latency: options.latency,
          port: ports.proxy,
          proxy: {
            forward: options.forward,
            target: {
              https: helpers.protocols.target === 'https',
              host: '127.0.0.1',
              port: ports.target
            }
          }
        }
      }, this.callback);
    },
    "the proxy request": exports.assertRequest({
      request: req,
      assert: {
        body: output
      }
    })
  };
};

//
// ### function assertInvalidProxy (options)
// #### @options {Object} Options for this test
// ####    @latency {number} Latency in milliseconds for the proxy server
// ####    @ports   {Object} Ports for the request (target, proxy)
//
// Creates a complete end-to-end test for requesting against an
// http proxy with no target server.
//
exports.assertInvalidProxy = function (options) {
  options = options || {};

  var ports    = options.ports   || helpers.nextPortPair,
      req      = options.request || {},
      protocol = helpers.protocols.proxy;


  req.uri = req.uri || protocol + '://127.0.0.1:' + ports.proxy;

  return {
    topic: function () {
      //
      // Only create the proxy server, simulating a reverse-proxy
      // to an invalid location.
      //
      helpers.http.createProxyServer({
        latency: options.latency,
        port: ports.proxy,
        proxy: {
          target: {
            host: '127.0.0.1',
            port: ports.target
          }
        }
      }, this.callback);
    },
    "the proxy request": exports.assertRequest({
      request: req,
      assert: {
        statusCode: 500
      }
    })
  };
};

//
// ### function assertForwardProxied (options)
// #### @options {Object} Options for this test.
//
// Creates a complete end-to-end test for requesting against an
// http proxy with both a valid and invalid forward target.
//
exports.assertForwardProxied = function (options) {
  var forwardPort = helpers.nextPort;

  return {
    topic: function () {
      helpers.http.createServer({
        output: 'hello from forward',
        port: forwardPort
      }, this.callback);
    },
    "and a valid forward target": exports.assertProxied({
      forward: {
        port: forwardPort,
        host: '127.0.0.1'
      }
    }),
    "and an invalid forward target": exports.assertProxied({
      forward: {
        port: 9898,
        host: '127.0.0.1'
      }
    })
  };
};

//
// ### function assertProxiedtoRoutes (options, nested)
// #### @options {Object} Options for this ProxyTable-based test
// ####    @routes       {Object|string} Routes to use for the proxy.
// ####    @hostnameOnly {boolean} Enables hostnameOnly routing.
// #### @nested  {Object} Nested vows to add to the returned context.
//
// Creates a complete end-to-end test for requesting against an
// http proxy using `options.routes`:
//
// 1. Creates target servers for all routes in `options.routes.`
// 2. Creates a proxy server.
// 3. Ensure requests to the proxy server for all route targets
//    returns the unique expected output.
//
exports.assertProxiedToRoutes = function (options, nested) {
  //
  // Assign dynamic ports to the routes to use.
  //
  options.routes = helpers.http.assignPortsToRoutes(options.routes);

  //
  // Parse locations from routes for making assertion requests.
  //
  var locations = helpers.http.parseRoutes(options),
      port = helpers.nextPort,
      protocol = helpers.protocols.proxy,
      context,
      proxy;

  if (options.filename) {
    //
    // If we've been passed a filename write the routes to it
    // and setup the proxy options to use that file.
    //
    fs.writeFileSync(options.filename, JSON.stringify({ router: options.routes }));
    proxy = { router: options.filename };
  }
  else {
    //
    // Otherwise just use the routes themselves.
    //
    proxy = {
      hostnameOnly: options.hostnameOnly,
      router: options.routes
    };
  }

  //
  // Set the https options if necessary
  //
  if (helpers.protocols.target === 'https') {
    proxy.target = { https: true };
  }

  //
  // Create the test context which creates all target
  // servers for all routes and a proxy server.
  //
  context = {
    topic: function () {
      var that = this;

      async.waterfall([
        //
        // 1. Create all the target servers
        //
        async.apply(
          async.forEach,
          locations,
          function createRouteTarget(location, next) {
            helpers.http.createServer({
              port: location.target.port,
              output: 'hello from ' + location.source.href
            }, next);
          }
        ),
        //
        // 2. Create the proxy server
        //
        async.apply(
          helpers.http.createProxyServer,
          {
            port: port,
            latency: options.latency,
            routing: true,
            proxy: proxy
          }
        )
      ], function (_, server) {
        //
        // 3. Set the proxy server for later use
        //
        that.proxyServer = server;
        that.callback();
      });

      //
      // 4. Assign the port to the context for later use
      //
      this.port = port;
    },
    //
    // Add an extra assertion to a route which
    // should respond with 404
    //
    "a request to unknown.com": exports.assertRequest({
      assert: { statusCode: 404 },
      request: {
        uri: protocol + '://127.0.0.1:' + port,
        headers: {
          host: 'unknown.com'
        }
      }
    })
  };

  //
  // Add test assertions for each of the route locations.
  //
  locations.forEach(function (location) {
    context[location.source.href] = exports.assertRequest({
      request: {
        uri: protocol + '://127.0.0.1:' + port + location.source.path,
        headers: {
          host: location.source.hostname
        }
      },
      assert: {
        body: 'hello from ' + location.source.href
      }
    });
  });

  //
  // If there are any nested vows to add to the context
  // add them before returning the full context.
  //
  if (nested) {
    Object.keys(nested).forEach(function (key) {
      context[key] = nested[key];
    });
  }

  return context;
};
