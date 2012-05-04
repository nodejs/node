/*!
 * Tobi - Cookie
 * Copyright(c) 2010 LearnBoost <dev@learnboost.com>
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var url = require('url');

/**
 * Initialize a new `Cookie` with the given cookie `str` and `req`.
 *
 * @param {String} str
 * @param {IncomingRequest} req
 * @api private
 */

var Cookie = exports = module.exports = function Cookie(str, req) {
  this.str = str;

  // First key is the name
  this.name = str.substr(0, str.indexOf('=')).trim();

  // Map the key/val pairs
  str.split(/ *; */).reduce(function(obj, pair){
   var p = pair.indexOf('=');
   if(p > 0)
    obj[pair.substring(0, p).trim()] = pair.substring(p + 1).trim();
   else
    obj[pair.trim()] = true;
   return obj;
  }, this);

  // Assign value
  this.value = this[this.name];

  // Expires
  this.expires = this.expires
    ? new Date(this.expires)
    : Infinity;

  // Default or trim path
  this.path = this.path
    ? this.path.trim(): req 
    ? url.parse(req.url).pathname: '/';
};

/**
 * Return the original cookie string.
 *
 * @return {String}
 * @api public
 */

Cookie.prototype.toString = function(){
  return this.str;
};
