
/*!
 * Connect - cookieParser
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var utils = require('./../utils');

/**
 * Parse _Cookie_ header and populate `req.cookies`
 * with an object keyed by the cookie names.
 *
 * Examples:
 *
 *     connect.createServer(
 *         connect.cookieParser()
 *       , function(req, res, next){
 *         res.end(JSON.stringify(req.cookies));
 *       }
 *     );
 *
 * @return {Function}
 * @api public
 */

module.exports = function cookieParser(){
  return function cookieParser(req, res, next) {
    var cookie = req.headers.cookie;
    if (req.cookies) return next();
    req.cookies = {};
    if (cookie) {
      try {
        req.cookies = utils.parseCookie(cookie);
      } catch (err) {
        return next(err);
      }
    }
    next();
  };
};