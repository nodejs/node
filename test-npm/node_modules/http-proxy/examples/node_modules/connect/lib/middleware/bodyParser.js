
/*!
 * Connect - bodyParser
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var qs = require('qs');

/**
 * Extract the mime type from the given request's
 * _Content-Type_ header.
 *
 * @param  {IncomingMessage} req
 * @return {String}
 * @api private
 */

function mime(req) {
  var str = req.headers['content-type'] || '';
  return str.split(';')[0];
}

/**
 * Parse request bodies.
 *
 * By default _application/json_ and _application/x-www-form-urlencoded_
 * are supported, however you may map `connect.bodyParser.parse[contentType]`
 * to a function of your choice to replace existing parsers, or implement
 * one for other content-types.
 *
 * Examples:
 *
 *      connect.createServer(
 *          connect.bodyParser()
 *        , function(req, res) {
 *          res.end('viewing user ' + req.body.user.name);
 *        }
 *      );
 *
 * Since both _json_ and _x-www-form-urlencoded_ are supported by
 * default, either of the following requests would result in the response
 * of "viewing user tj".
 *
 *      $ curl -d 'user[name]=tj' http://localhost/
 *      $ curl -d '{"user":{"name":"tj"}}' -H "Content-Type: application/json" http://localhost/
 *
 * @return {Function}
 * @api public
 */

exports = module.exports = function bodyParser(){
  return function bodyParser(req, res, next) {
    if ('GET' == req.method || 'HEAD' == req.method) return next();
    var parser = exports.parse[mime(req)];
    if (parser && !req.body) {
      var data = '';
      req.setEncoding('utf8');
      req.on('data', function(chunk) { data += chunk; });
      req.on('end', function(){
        req.rawBody = data;
        try {
          req.body = data
            ? parser(data)
            : {};
        } catch (err) {
          return next(err);
        }
        next();
      });
    } else {
      next();
    }
  }
};

/**
 * Supported decoders.
 *
 *  - application/x-www-form-urlencoded
 *  - application/json
 */

exports.parse = {
    'application/x-www-form-urlencoded': qs.parse
  , 'application/json': JSON.parse
};