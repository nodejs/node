/*!
 * Ext JS Connect
 * Copyright(c) 2010 Sencha Inc.
 * MIT Licensed
 */

var fs = require('fs'),
    parse = require('url').parse,
    path = require('path'),
    mime = require('mime'),
    exec = require('child_process').exec,
    staticSend = require('connect').static.send;

/**
 * staticGzip gzips statics and then serves them with the regular Connect
 * static provider. By default, it compresses files with mime types that
 * match the expression /text|javascript|json/.
 *
 * Options:
 *
 *  - `matchType`   Regular expression matching mime types to be compressed
 *  - `flags`       String of flags passed to the binary. Defaults to "--best"
 *  - `bin`         Binary executable defaulting to "gzip"
 *
 * @param {String} root
 * @param {Object} options
 * @api public
 */

exports = module.exports = function staticGzip(root, options) {
  var options = options || {},
      matchType = options.matchType || /text|javascript|json/,
      bin = options.bin || 'gzip',
      flags = options.flags || '--best',
      rootLength;

  if (!root) throw new Error('staticGzip root must be set');
  if (!matchType.test) throw new Error('option matchType must be a regular expression');
  
  options.root = root;
  rootLength = root.length;
  
  return function(req, res, next) {
    var url, filename, type, acceptEncoding, ua;
    
    if (req.method !== 'GET') return next();
    
    url = parse(req.url);
    filename = path.join(root, url.pathname);
    if ('/' == filename[filename.length - 1]) filename += 'index.html';
    
    type = mime.lookup(filename);
    if (!matchType.test(type)) {
      return passToStatic(filename);
    }
    
    acceptEncoding = req.headers['accept-encoding'] || '';
    if (!~acceptEncoding.indexOf('gzip')) {
      return passToStatic(filename);
    }

    ua = req.headers['user-agent'] || '';
    if (~ua.indexOf('MSIE 6') && !~ua.indexOf('SV1')) {
      return passToStatic(filename);
    }
    
    // Potentially malicious path
    if (~filename.indexOf('..')) {
      return passToStatic(filename);
    }
    
    // Check for requested file
    fs.stat(filename, function(err, stat) {
      if (err || stat.isDirectory()) {
        return passToStatic(filename);
      }
      
      // Check for compressed file
      var base = path.basename(filename),
          dir = path.dirname(filename),
          gzipname = path.join(dir, base + '.' + Number(stat.mtime) + '.gz');
      fs.stat(gzipname, function(err) {
        if (err && err.code === 'ENOENT') {
          // Remove any old gz files
          exec('rm ' + path.join(dir, base + '.*.gz'), function(err) {
            // Gzipped file doesn't exist, so make it then send
            gzip(bin, flags, filename, gzipname, function(err) {
              return sendGzip();
            });
          });
        } else if (err) {
          return passToStatic(filename);
        } else {
          return sendGzip();
        }
      });
      
      function sendGzip() {
        var charset = mime.charsets.lookup(type),
            contentType = type + (charset ? '; charset=' + charset : '');
        res.setHeader('Content-Type', contentType);
        res.setHeader('Content-Encoding', 'gzip');
        res.setHeader('Vary', 'Accept-Encoding');
        passToStatic(gzipname);
      }
    });
    
    function passToStatic(name) {
      var o = Object.create(options);
      o.path = name.substr(rootLength);
      staticSend(req, res, next, o);
    }
  };
};

function gzip(bin, flags, src, dest, callback) {
  var cmd = bin + ' ' + flags + ' -c ' + src + ' > ' + dest;
  exec(cmd, function(err, stdout, stderr) {
    if (err) {
      console.error('\n' + err.stack);
      fs.unlink(dest);
    }
    callback(err);
  });
}
