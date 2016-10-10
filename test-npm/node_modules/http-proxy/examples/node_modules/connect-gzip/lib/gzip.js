/*!
 * Ext JS Connect
 * Copyright(c) 2010 Sencha Inc.
 * MIT Licensed
 */

var spawn = require('child_process').spawn;

/**
 * Connect middleware providing gzip compression on the fly. By default, it
 * compresses requests with mime types that match the expression
 * /text|javascript|json/.
 *
 * Options:
 *
 *  - `matchType`   Regular expression matching mime types to be compressed
 *  - `flags`       String of flags passed to the binary. Nothing by default
 *  - `bin`         Binary executable defaulting to "gzip"
 *
 * @param {Object} options
 * @api public
 */

exports = module.exports = function gzip(options) {
  var options = options || {},
      matchType = options.matchType || /text|javascript|json/,
      bin = options.bin || 'gzip',
      flags = options.flags || '';

  if (!matchType.test) throw new Error('option matchType must be a regular expression');
  
  flags = (flags) ? '-c ' + flags : '-c';
  flags = flags.split(' ');
  
  return function gzip(req, res, next) {
    var writeHead = res.writeHead,
        defaults = {};
    
    ['write', 'end'].forEach(function(name) {
      defaults[name] = res[name];
      res[name] = function() {
        // Make sure headers are setup if they haven't been called yet
        if (res.writeHead !== writeHead) {
          res.writeHead(res.statusCode);
        }
        res[name].apply(this, arguments);
      };
    });
    
    res.writeHead = function(code) {
      var args = Array.prototype.slice.call(arguments, 0),
          write = defaults.write,
          end = defaults.end,
          headers, key, accept, type, encoding, gzip, ua;
      if (args.length > 1) {
        headers = args.pop();
        for (key in headers) {
          res.setHeader(key, headers[key]);
        }
      }
      
      ua = req.headers['user-agent'] || '';
      accept = req.headers['accept-encoding'] || '';
      type = res.getHeader('content-type') || '';
      encoding = res.getHeader('content-encoding');
      
      if (req.method === 'HEAD' || code !== 200 || !~accept.indexOf('gzip') ||
          !matchType.test(type) || encoding ||
          (~ua.indexOf('MSIE 6') && !~ua.indexOf('SV1'))) {
        res.write = write;
        res.end = end;
        return finish();
      }
      
      res.setHeader('Content-Encoding', 'gzip');
      res.setHeader('Vary', 'Accept-Encoding');
      res.removeHeader('Content-Length');
      
      gzip = spawn(bin, flags);
      
      res.write = function(chunk, encoding) {
        gzip.stdin.write(chunk, encoding);
      };

      res.end = function(chunk, encoding) {
        if (chunk) {
          res.write(chunk, encoding);
        }
        gzip.stdin.end();
      };

      gzip.stdout.addListener('data', function(chunk) {
        write.call(res, chunk);
      });

      gzip.addListener('exit', function(code) {
        res.write = write;
        res.end = end;
        res.end();
      });
      
      finish();
      
      function finish() {
        res.writeHead = writeHead;
        res.writeHead.apply(res, args);
      }
    };

    next();
  };
};
