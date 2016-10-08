
/*!
 * Connect - session - Store
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var Session = require('./session')
  , Cookie = require('./cookie')
  , utils = require('../../utils');

/**
 * Initialize abstract `Store`.
 *
 * @api private
 */

var Store = module.exports = function Store(options){};

/**
 * Re-generate the given requests's session.
 *
 * @param {IncomingRequest} req
 * @return {Function} fn
 * @api public
 */

Store.prototype.regenerate = function(req, fn){
  var self = this;
  this.destroy(req.sessionID, function(err){
    self.generate(req);
    fn(err);
  });
};

/**
 * Create session from JSON `sess` data.
 *
 * @param {IncomingRequest} req
 * @param {Object} sess
 * @return {Session}
 * @api private
 */

Store.prototype.createSession = function(req, sess){
  var expires = sess.cookie.expires
    , orig = sess.cookie.originalMaxAge;
  sess.cookie = new Cookie(sess.cookie);
  if ('string' == typeof expires) sess.cookie.expires = new Date(expires);
  sess.cookie.originalMaxAge = orig;
  req.session = new Session(req, sess);
  req.session.resetLastAccess();
  return req.session;
};