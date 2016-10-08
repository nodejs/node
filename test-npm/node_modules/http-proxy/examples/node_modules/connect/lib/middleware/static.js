
/*!
 * Connect - staticProvider
 * Copyright(c) 2010 Sencha Inc.
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var fs = require('fs')
  , path = require('path')
  , join = path.join
  , basename = path.basename
  , normalize = path.normalize
  , utils = require('../utils')
  , Buffer = require('buffer').Buffer
  , parse = require('url').parse
  , mime = require('mime');

/**
 * Static file server with the given `root` path.
 *
 * Examples:
 *
 *     var oneDay = 86400000;
 *
 *     connect(
 *       connect.static(__dirname + '/public')
 *     ).listen(3000);
 *
 *     connect(
 *       connect.static(__dirname + '/public', { maxAge: oneDay })
 *     ).listen(3000);
 *
 * Options:
 *
 *    - `maxAge`   Browser cache maxAge in milliseconds. defaults to 0
 *    - `hidden`   Allow transfer of hidden files. defaults to false
 *
 * @param {String} root
 * @param {Object} options
 * @return {Function}
 * @api public
 */

exports = module.exports = function static(root, options){
  options = options || {};

  // root required
  if (!root) throw new Error('static() root path required');
  options.root = root;

  return function static(req, res, next) {
    options.path = req.url;
    options.getOnly = true;
    send(req, res, next, options);
  };
};

/**
 * Expose mime module.
 */

exports.mime = mime;

/**
 * Respond with 416  "Requested Range Not Satisfiable"
 *
 * @param {ServerResponse} res
 * @api private
 */

function invalidRange(res) {
  var body = 'Requested Range Not Satisfiable';
  res.setHeader('Content-Type', 'text/plain');
  res.setHeader('Content-Length', body.length);
  res.statusCode = 416;
  res.end(body);
}

/**
 * Attempt to tranfer the requseted file to `res`.
 *
 * @param {ServerRequest}
 * @param {ServerResponse}
 * @param {Function} next
 * @param {Object} options
 * @api private
 */

var send = exports.send = function(req, res, next, options){
  options = options || {};
  if (!options.path) throw new Error('path required');

  // setup
  var maxAge = options.maxAge || 0
    , ranges = req.headers.range
    , head = 'HEAD' == req.method
    , get = 'GET' == req.method
    , root = options.root ? normalize(options.root) : null
    , getOnly = options.getOnly
    , fn = options.callback
    , hidden = options.hidden
    , done;

  // replace next() with callback when available
  if (fn) next = fn;

  // ignore non-GET requests
  if (getOnly && !get && !head) return next();

  // parse url
  var url = parse(options.path)
    , path = decodeURIComponent(url.pathname)
    , type;

  // null byte(s)
  if (~path.indexOf('\0')) return utils.badRequest(res);

  // when root is not given, consider .. malicious
  if (!root && ~path.indexOf('..')) return utils.forbidden(res);

  // join / normalize from optional root dir
  path = normalize(join(root, path));

  // malicious path
  if (root && 0 != path.indexOf(root)) return fn
    ? fn(new Error('Forbidden'))
    : utils.forbidden(res);

  // index.html support
  if ('/' == path[path.length - 1]) path += 'index.html';

  // "hidden" file
  if (!hidden && '.' == basename(path)[0]) return next();

  fs.stat(path, function(err, stat){
    // mime type
    type = mime.lookup(path);

    // ignore ENOENT
    if (err) {
      if (fn) return fn(err);
      return 'ENOENT' == err.code
        ? next()
        : next(err);
    // redirect directory in case index.html is present
    } else if (stat.isDirectory()) {
      res.statusCode = 301;
      res.setHeader('Location', url.pathname + '/');
      res.end('Redirecting to ' + url.pathname + '/');
      return;
    }

    var opts = {};

    // we have a Range request
    if (ranges) {
      ranges = utils.parseRange(stat.size, ranges);
      // valid
      if (ranges) {
        // TODO: stream options
        // TODO: multiple support
        opts.start = ranges[0].start;
        opts.end = ranges[0].end;
        res.statusCode = 206;
        res.setHeader('Content-Range', 'bytes '
          + opts.start
          + '-'
          + opts.end
          + '/'
          + stat.size);
      // invalid
      } else {
        return fn
          ? fn(new Error('Requested Range Not Satisfiable'))
          : invalidRange(res);
      }
    // stream the entire file
    } else {
      res.setHeader('Content-Length', stat.size);
      if (!res.getHeader('Cache-Control')) res.setHeader('Cache-Control', 'public, max-age=' + (maxAge / 1000));
      if (!res.getHeader('Last-Modified')) res.setHeader('Last-Modified', stat.mtime.toUTCString());
      if (!res.getHeader('ETag')) res.setHeader('ETag', utils.etag(stat));

      // conditional GET support
      if (utils.conditionalGET(req)) {
        if (!utils.modified(req, res)) {
          return utils.notModified(res);
        }
      }
    }

    // header fields
    if (!res.getHeader('content-type')) {
      var charset = mime.charsets.lookup(type);
      res.setHeader('Content-Type', type + (charset ? '; charset=' + charset : ''));
    }
    res.setHeader('Accept-Ranges', 'bytes');

    // transfer
    if (head) return res.end();

    // stream
    var stream = fs.createReadStream(path, opts);
    stream.pipe(res);

    // callback
    if (fn) {
      function callback(err) { done || fn(err); done = true }
      req.on('close', callback);
      req.socket.on('error', callback);
      stream.on('end', callback);
    }
  });
};
