var sys = require("sys");
var events = require('events');

exports.parse = function(options) {
  var promise = new events.Promise();

  try {
    var stream = new exports.Stream(options);
  } catch (e) {
    setTimeout(function() {
      promise.emitError(e);
    }, 0);
    return promise;
  }

  var parts = {};
  stream.addListener('part', function(part) {
    var name = part.name;
    var buffer = '';

    part.addListener('body', function(chunk) {
      buffer = buffer + chunk;
    });

    part.addListener('complete', function() {
      parts[name] = buffer;
    });
  });

  stream.addListener('complete', function() {
    promise.emitSuccess(parts);
  });

  return promise;
};

exports.Stream = function(options) {
  events.EventEmitter.call(this);

  this.init(options);
};
sys.inherits(exports.Stream, events.EventEmitter);


var proto = exports.Stream.prototype;

proto.init = function(options) {
  this.buffer = '';
  this.bytesReceived = 0;
  this.bytesTotal = 0;
  this.part = null;

  if ('headers' in options) {
    var req = options, contentType = req.headers['content-type'];
    if (!contentType) {
      throw new Error('Content-Type header not set');
    }

    var boundary = contentType.match(/^multipart\/form-data; ?boundary=(.+)$/i)
    if (!boundary) {
      throw new Error('Content-Type is not multipart: "'+contentType+'"');
    }

    this.boundary = '--'+boundary[1];
    this.bytesTotal = req.headers['content-length'];

    var self = this;
    req
      .addListener('body', function(chunk) {
        self.write(chunk);
      })
      .addListener('complete', function() {
        self.emit('complete');
      });
  } else {
    if (!options.boundary) {
      throw new Error('No boundary option given');
    }

    this.boundary = options.boundary;
    this.write(options.data || '');
  }
};

proto.write = function(chunk) {
  this.bytesReceived = this.bytesReceived + chunk.length;
  this.buffer = this.buffer + chunk;

  while (this.buffer.length) {
    var offset = this.buffer.indexOf(this.boundary);

    if (offset === 0) {
      this.buffer = this.buffer.substr(offset + this.boundary.length + 2);
    } else if (offset == -1) {
      if (this.buffer === "\r\n") {
        this.buffer = '';
      } else {
        this.part = (this.part || new Part(this));
        this.part.write(this.buffer);
        this.buffer = [];
      }
    } else if (offset > 0) {
      this.part = (this.part || new Part(this));
      this.part.write(this.buffer.substr(0, offset - 2));

      this.part.emit('complete');

      this.part = new Part(this);
      this.buffer = this.buffer.substr(offset + this.boundary.length + 2);
    }
  }
};

function Part(stream) {
  events.EventEmitter.call(this);

  this.headers = {};
  this.name = null;
  this.filename = null;
  this.buffer = '';
  this.bytesReceived = 0;

  // Avoids turning Part into a circular JSON object
  this.getStream = function() {
    return stream;
  };

  this._headersComplete = false;
}
sys.inherits(Part, events.EventEmitter);

Part.prototype.parsedHeaders = function() {
  for (var header in this.headers) {
    var parts = this.headers[header].split(/; ?/), parsedHeader = {};
    for (var i = 0; i < parts.length; i++) {
      var pair = parts[i].split('=');
      if (pair.length < 2) {
        continue;
      }

      var key = pair[0].toLowerCase(), val = pair[1] || '';
      val = stripslashes(val).substr(1);
      val = val.substr(0, val.length - 1);

      parsedHeader[key] = val;
    }

    if (header == 'content-disposition') {
      this.name = parsedHeader.name || null;
      this.filename = parsedHeader.filename || null;
    }

    this.headers[header] = parsedHeader;
  }
};

Part.prototype.write = function(chunk) {
  if (this._headersComplete) {
    this.bytesReceived = this.bytesReceived + chunk.length;
    this.emit('body', chunk);
    return;
  }

  this.buffer = this.buffer + chunk;
  while (this.buffer.length) {
    var offset = this.buffer.indexOf("\r\n");

    if (offset === 0) {
      this._headersComplete = true;
      this.parsedHeaders();
      this.getStream().emit('part', this);

      this.buffer = this.buffer.substr(2);
      this.bytesReceived = this.bytesReceived + this.buffer.length;
      this.emit('body', this.buffer);
      this.buffer = '';
      return;
    } else if (offset > 0) {
      var header = this.buffer.substr(0, offset).split(/: ?/);
      this.headers[header[0].toLowerCase()] = header[1];
      this.buffer = this.buffer.substr(offset+2);
    } else if (offset === false) {
      return;
    }
  }
};

function stripslashes(str) {
  // +   original by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // +   improved by: Ates Goral (http://magnetiq.com)
  // +      fixed by: Mick@el
  // +   improved by: marrtins
  // +   bugfixed by: Onno Marsman
  // +   improved by: rezna
  // +   input by: Rick Waldron
  // +   reimplemented by: Brett Zamir (http://brett-zamir.me)
  // *     example 1: stripslashes('Kevin\'s code');
  // *     returns 1: "Kevin's code"
  // *     example 2: stripslashes('Kevin\\\'s code');
  // *     returns 2: "Kevin\'s code"
  return (str+'').replace(/\\(.?)/g, function (s, n1) {
    switch(n1) {
      case '\\':
        return '\\';
      case '0':
        return '\0';
      case '':
        return '';
      default:
        return n1;
    }
  });
}
