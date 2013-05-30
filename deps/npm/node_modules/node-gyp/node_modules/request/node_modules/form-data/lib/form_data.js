var CombinedStream = require('combined-stream');
var util = require('util');
var path = require('path');
var http = require('http');
var https = require('https');
var parseUrl = require('url').parse;
var fs = require('fs');
var mime = require('mime');
var async = require('async');

module.exports = FormData;
function FormData() {
  this._overheadLength = 0;
  this._valueLength = 0;
  this._lengthRetrievers = [];

  CombinedStream.call(this);
}
util.inherits(FormData, CombinedStream);

FormData.LINE_BREAK = '\r\n';

FormData.prototype.append = function(field, value) {
  var append = CombinedStream.prototype.append.bind(this);

  // all that streamy business can't handle numbers
  if (typeof value == 'number') value = ''+value;

  var header = this._multiPartHeader(field, value);
  var footer = this._multiPartFooter(field, value);

  append(header);
  append(value);
  append(footer);

  this._trackLength(header, value)
};

FormData.prototype._trackLength = function(header, value) {
  var valueLength = 0;
  if (Buffer.isBuffer(value)) {
    valueLength = value.length;
  } else if (typeof value === 'string') {
    valueLength = Buffer.byteLength(value);
  }

  this._valueLength += valueLength;
  this._overheadLength +=
    Buffer.byteLength(header) +
    + FormData.LINE_BREAK.length;

  // empty or ethier doesn't have path or not an http response
  if (!value || ( !value.path && !(value.readable && value.hasOwnProperty('httpVersion')) )) {
    return;
  }

  this._lengthRetrievers.push(function(next) {

    // check if it's local file
    if (value.hasOwnProperty('fd')) {
      fs.stat(value.path, function(err, stat) {
        if (err) {
          next(err);
          return;
        }

        next(null, stat.size);
      });

    // or http response
    } else if (value.hasOwnProperty('httpVersion')) {
      next(null, +value.headers['content-length']);

    // or request stream http://github.com/mikeal/request
    } else if (value.hasOwnProperty('httpModule')) {
      // wait till response come back
      value.on('response', function(response) {
        value.pause();
        next(null, +response.headers['content-length']);
      });
      value.resume();

    // something else
    } else {
      next('Unknown stream');
    }
  });
};

FormData.prototype._multiPartHeader = function(field, value) {
  var boundary = this.getBoundary();
  var header =
    '--' + boundary + FormData.LINE_BREAK +
    'Content-Disposition: form-data; name="' + field + '"';

  // fs- and request- streams have path property
  // TODO: Use request's response mime-type
  if (value.path) {
    header +=
      '; filename="' + path.basename(value.path) + '"' + FormData.LINE_BREAK +
      'Content-Type: ' + mime.lookup(value.path);

  // http response has not
  } else if (value.readable && value.hasOwnProperty('httpVersion')) {
    header +=
      '; filename="' + path.basename(value.client._httpMessage.path) + '"' + FormData.LINE_BREAK +
      'Content-Type: ' + value.headers['content-type'];
  }

  header += FormData.LINE_BREAK + FormData.LINE_BREAK;
  return header;
};

FormData.prototype._multiPartFooter = function(field, value) {
  return function(next) {
    var footer = FormData.LINE_BREAK;

    var lastPart = (this._streams.length === 0);
    if (lastPart) {
      footer += this._lastBoundary();
    }

    next(footer);
  }.bind(this);
};

FormData.prototype._lastBoundary = function() {
  return '--' + this.getBoundary() + '--';
};

FormData.prototype.getHeaders = function(userHeaders) {
  var formHeaders = {
    'content-type': 'multipart/form-data; boundary=' + this.getBoundary()
  };

  for (var header in userHeaders) {
    formHeaders[header.toLowerCase()] = userHeaders[header];
  }

  return formHeaders;
}

FormData.prototype.getCustomHeaders = function(contentType) {
    contentType = contentType ? contentType : 'multipart/form-data';

    var formHeaders = {
        'content-type': contentType + '; boundary=' + this.getBoundary(),
        'content-length': this.getLengthSync()
    };

    return formHeaders;
}

FormData.prototype.getBoundary = function() {
  if (!this._boundary) {
    this._generateBoundary();
  }

  return this._boundary;
};

FormData.prototype._generateBoundary = function() {
  // This generates a 50 character boundary similar to those used by Firefox.
  // They are optimized for boyer-moore parsing.
  var boundary = '--------------------------';
  for (var i = 0; i < 24; i++) {
    boundary += Math.floor(Math.random() * 10).toString(16);
  }

  this._boundary = boundary;
};

FormData.prototype.getLengthSync = function() {
    var knownLength = this._overheadLength + this._valueLength;

    if (this._streams.length) {
        knownLength += this._lastBoundary().length;
    }

    return knownLength;
};

FormData.prototype.getLength = function(cb) {
  var knownLength = this._overheadLength + this._valueLength;

  if (this._streams.length) {
    knownLength += this._lastBoundary().length;
  }

  if (!this._lengthRetrievers.length) {
    process.nextTick(cb.bind(this, null, knownLength));
    return;
  }

  async.parallel(this._lengthRetrievers, function(err, values) {
    if (err) {
      cb(err);
      return;
    }

    values.forEach(function(length) {
      knownLength += length;
    });

    cb(null, knownLength);
  });
};

FormData.prototype.submit = function(url, cb) {
  this.getLength(function(err, length) {
    var request
      , parsedUrl = parseUrl(url)
      , options = {
          method: 'post',
          port: parsedUrl.port || 80,
          path: parsedUrl.pathname,
          headers: this.getHeaders({'Content-Length': length}),
          host: parsedUrl.hostname
        };

    if (parsedUrl.protocol == 'https:') {
      // override default port
      if (!parsedUrl.port) options.port = 443;
      request = https.request(options);
    } else {
      request = http.request(options);
    }

    this.pipe(request);
    if (cb) {
      request.on('error', cb);
      request.on('response', cb.bind(this, null));
    }

    return request;
  }.bind(this));
};
