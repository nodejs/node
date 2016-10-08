
/*!
 * Connect - csrf
 * Copyright(c) 2011 Sencha Inc.
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var utils = require('../utils')
  , crypto = require('crypto');

/**
 * CRSF protection middleware.
 *
 * By default this middleware generates a token named "_csrf"
 * which should be added to requests which mutate
 * state, within a hidden form field, query-string etc. This
 * token is validated against the visitor's `req.session._csrf`
 * property which is re-generated per request.
 *
 * The default `value` function checks `req.body` generated
 * by the `bodyParser()` middleware, `req.query` generated
 * by `query()`, and the "X-CSRF-Token" header field.
 *
 * This middleware requires session support, thus should be added
 * somewhere _below_ `session()` and `cookieParser()`.
 *
 * Examples:
 *
 *      var form = '\n\
 *        <form action="/" method="post">\n\
 *          <input type="hidden" name="_csrf" value="{token}" />\n\
 *          <input type="text" name="user[name]" value="{user}" />\n\
 *          <input type="password" name="user[pass]" />\n\
 *          <input type="submit" value="Login" />\n\
 *        </form>\n\
 *      '; 
 *      
 *      connect(
 *          connect.cookieParser()
 *        , connect.session({ secret: 'keyboard cat' })
 *        , connect.bodyParser()
 *        , connect.csrf()
 *      
 *        , function(req, res, next){
 *          if ('POST' != req.method) return next();
 *          req.session.user = req.body.user;
 *          next();
 *        }
 *      
 *        , function(req, res){
 *          res.setHeader('Content-Type', 'text/html');
 *          var body = form
 *            .replace('{token}', req.session._csrf)
 *            .replace('{user}', req.session.user && req.session.user.name || '');
 *          res.end(body);
 *        }
 *      ).listen(3000);
 *
 * Options:
 *
 *    - `value` a function accepting the request, returning the token 
 *
 * @param {Object} options
 * @api public
 */

module.exports = function csrf(options) {
  var options = options || {}
    , value = options.value || defaultValue;

  return function(req, res, next){
    // generate CSRF token
    var token = req.session._csrf || (req.session._csrf = utils.uid(24));

    // ignore GET (for now)
    if ('GET' == req.method) return next();

    // determine value
    var val = value(req);

    // check
    if (val != token) return utils.forbidden(res);
    
    next();
  }
};

/**
 * Default value function, checking the `req.body`
 * and `req.query` for the CSRF token.
 *
 * @param {IncomingMessage} req
 * @return {String}
 * @api private
 */

function defaultValue(req) {
  return (req.body && req.body._csrf)
    || (req.query && req.query._csrf)
    || (req.headers['x-csrf-token']);
}