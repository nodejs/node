
/*!
 * Connect - utils
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var crypto = require('crypto')
  , Path = require('path')
  , fs = require('fs');

/**
 * Flatten the given `arr`.
 *
 * @param {Array} arr
 * @return {Array}
 * @api private
 */

exports.flatten = function(arr, ret){
  var ret = ret || []
    , len = arr.length;
  for (var i = 0; i < len; ++i) {
    if (Array.isArray(arr[i])) {
      exports.flatten(arr[i], ret);
    } else {
      ret.push(arr[i]);
    }
  }
  return ret;
};

/**
 * Return md5 hash of the given string and optional encoding,
 * defaulting to hex.
 *
 *     utils.md5('wahoo');
 *     // => "e493298061761236c96b02ea6aa8a2ad"
 *
 * @param {String} str
 * @param {String} encoding
 * @return {String}
 * @api public
 */

exports.md5 = function(str, encoding){
  return crypto
    .createHash('md5')
    .update(str)
    .digest(encoding || 'hex');
};

/**
 * Merge object b with object a.
 *
 *     var a = { foo: 'bar' }
 *       , b = { bar: 'baz' };
 *     
 *     utils.merge(a, b);
 *     // => { foo: 'bar', bar: 'baz' }
 *
 * @param {Object} a
 * @param {Object} b
 * @return {Object}
 * @api public
 */

exports.merge = function(a, b){
  if (a && b) {
    for (var key in b) {
      a[key] = b[key];
    }
  }
  return a;
};

/**
 * Escape the given string of `html`.
 *
 * @param {String} html
 * @return {String}
 * @api public
 */

exports.escape = function(html){
  return String(html)
    .replace(/&(?!\w+;)/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
};


/**
 * Return a unique identifier with the given `len`.
 *
 *     utils.uid(10);
 *     // => "FDaS435D2z"
 *
 * @param {Number} len
 * @return {String}
 * @api public
 */

exports.uid = function(len) {
  var buf = []
    , chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'
    , charlen = chars.length;

  for (var i = 0; i < len; ++i) {
    buf.push(chars[getRandomInt(0, charlen - 1)]);
  }

  return buf.join('');
};

/**
 * Parse the given cookie string into an object.
 *
 * @param {String} str
 * @return {Object}
 * @api public
 */

exports.parseCookie = function(str){
  var obj = {}
    , pairs = str.split(/[;,] */);
  for (var i = 0, len = pairs.length; i < len; ++i) {
    var pair = pairs[i]
      , eqlIndex = pair.indexOf('=')
      , key = pair.substr(0, eqlIndex).trim().toLowerCase()
      , val = pair.substr(++eqlIndex, pair.length).trim();

    // quoted values
    if ('"' == val[0]) val = val.slice(1, -1);

    // only assign once
    if (undefined == obj[key]) {
      val = val.replace(/\+/g, ' ');
      try {
        obj[key] = decodeURIComponent(val);
      } catch (err) {
        if (err instanceof URIError) {
          obj[key] = val;
        } else {
          throw err;
        }
      }
    }
  }
  return obj;
};

/**
 * Serialize the given object into a cookie string.
 *
 *      utils.serializeCookie('name', 'tj', { httpOnly: true })
 *      // => "name=tj; httpOnly"
 *
 * @param {String} name
 * @param {String} val
 * @param {Object} obj
 * @return {String}
 * @api public
 */

exports.serializeCookie = function(name, val, obj){
  var pairs = [name + '=' + encodeURIComponent(val)]
    , obj = obj || {};

  if (obj.domain) pairs.push('domain=' + obj.domain);
  if (obj.path) pairs.push('path=' + obj.path);
  if (obj.expires) pairs.push('expires=' + obj.expires.toUTCString());
  if (obj.httpOnly) pairs.push('httpOnly');
  if (obj.secure) pairs.push('secure');

  return pairs.join('; ');
};

/**
 * Pause `data` and `end` events on the given `obj`.
 * Middleware performing async tasks _should_ utilize
 * this utility (or similar), to re-emit data once
 * the async operation has completed, otherwise these
 * events may be lost.
 *
 *      var pause = utils.pause(req);
 *      fs.readFile(path, function(){
 *         next();
 *         pause.resume();
 *      });
 *
 * @param {Object} obj
 * @return {Object}
 * @api public
 */

exports.pause = function(obj){
  var onData
    , onEnd
    , events = [];

  // buffer data
  obj.on('data', onData = function(data, encoding){
    events.push(['data', data, encoding]);
  });

  // buffer end
  obj.on('end', onEnd = function(data, encoding){
    events.push(['end', data, encoding]);
  });

  return {
    end: function(){
      obj.removeListener('data', onData);
      obj.removeListener('end', onEnd);
    },
    resume: function(){
      this.end();
      for (var i = 0, len = events.length; i < len; ++i) {
        obj.emit.apply(obj, events[i]);
      }
    }
  };
};

/**
 * Check `req` and `res` to see if it has been modified.
 *
 * @param {IncomingMessage} req
 * @param {ServerResponse} res
 * @return {Boolean}
 * @api public
 */

exports.modified = function(req, res, headers) {
  var headers = headers || res._headers || {}
    , modifiedSince = req.headers['if-modified-since']
    , lastModified = headers['last-modified']
    , noneMatch = req.headers['if-none-match']
    , etag = headers['etag'];

  if (noneMatch) noneMatch = noneMatch.split(/ *, */);

  // check If-None-Match
  if (noneMatch && etag && ~noneMatch.indexOf(etag)) {
    return false;
  }

  // check If-Modified-Since
  if (modifiedSince && lastModified) {
    modifiedSince = new Date(modifiedSince);
    lastModified = new Date(lastModified);
    // Ignore invalid dates
    if (!isNaN(modifiedSince.getTime())) {
      if (lastModified <= modifiedSince) return false;
    }
  }
  
  return true;
};

/**
 * Strip `Content-*` headers from `res`.
 *
 * @param {ServerResponse} res
 * @api public
 */

exports.removeContentHeaders = function(res){
  Object.keys(res._headers).forEach(function(field){
    if (0 == field.indexOf('content')) {
      res.removeHeader(field);
    }
  });
};

/**
 * Check if `req` is a conditional GET request.
 *
 * @param {IncomingMessage} req
 * @return {Boolean}
 * @api public
 */

exports.conditionalGET = function(req) {
  return req.headers['if-modified-since']
    || req.headers['if-none-match'];
};

/**
 * Respond with 403 "Forbidden".
 *
 * @param {ServerResponse} res
 * @api public
 */

exports.forbidden = function(res) {
  var body = 'Forbidden';
  res.setHeader('Content-Type', 'text/plain');
  res.setHeader('Content-Length', body.length);
  res.statusCode = 403;
  res.end(body);
};

/**
 * Respond with 401 "Unauthorized".
 *
 * @param {ServerResponse} res
 * @param {String} realm
 * @api public
 */

exports.unauthorized = function(res, realm) {
  res.statusCode = 401;
  res.setHeader('WWW-Authenticate', 'Basic realm="' + realm + '"');
  res.end('Unauthorized');
};

/**
 * Respond with 400 "Bad Request".
 *
 * @param {ServerResponse} res
 * @api public
 */

exports.badRequest = function(res) {
  res.statusCode = 400;
  res.end('Bad Request');
};

/**
 * Respond with 304 "Not Modified".
 *
 * @param {ServerResponse} res
 * @param {Object} headers
 * @api public
 */

exports.notModified = function(res) {
  exports.removeContentHeaders(res);
  res.statusCode = 304;
  res.end();
};

/**
 * Return an ETag in the form of `"<size>-<mtime>"`
 * from the given `stat`.
 *
 * @param {Object} stat
 * @return {String}
 * @api public
 */

exports.etag = function(stat) {
  return '"' + stat.size + '-' + Number(stat.mtime) + '"';
};

/**
 * Parse "Range" header `str` relative to the given file `size`.
 *
 * @param {Number} size
 * @param {String} str
 * @return {Array}
 * @api public
 */

exports.parseRange = function(size, str){
  var valid = true;
  var arr = str.substr(6).split(',').map(function(range){
    var range = range.split('-')
      , start = parseInt(range[0], 10)
      , end = parseInt(range[1], 10);

    // -500
    if (isNaN(start)) {
      start = size - end;
      end = size - 1;
    // 500-
    } else if (isNaN(end)) {
      end = size - 1;
    }

    // Invalid
    if (isNaN(start) || isNaN(end) || start > end) valid = false;

    return { start: start, end: end };
  });
  return valid ? arr : undefined;
};

/**
 * Convert array-like object to an `Array`.
 *
 * node-bench measured "16.5 times faster than Array.prototype.slice.call()"
 *
 * @param {Object} obj
 * @return {Array}
 * @api public
 */

var toArray = exports.toArray = function(obj){
  var len = obj.length
    , arr = new Array(len);
  for (var i = 0; i < len; ++i) {
    arr[i] = obj[i];
  }
  return arr;
};

/**
 * Retrun a random int, used by `utils.uid()`
 *
 * @param {Number} min
 * @param {Number} max
 * @return {Number}
 * @api private
 */

function getRandomInt(min, max) {
  return Math.floor(Math.random() * (max - min + 1)) + min;
}
