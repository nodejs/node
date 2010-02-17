
var sys = require("sys"),
  events = require("events"),
  wrapExpression = /^[ \t]+/,
  multipartExpression = new RegExp(
    "^multipart\/(" +
    "mixed|rfc822|message|digest|alternative|" +
    "related|report|signed|encrypted|form-data|" +
    "x-mixed-replace|byteranges)", "i"),
  boundaryExpression = /boundary=([^;]+)/i,
  CR = "\r",
  LF = "\n",
  CRLF = CR+LF,
  MAX_BUFFER_LENGTH = 16 * 1024,

  // parser states.
  s = 0,
  S_NEW_PART = s++,
  S_HEADER = s++,
  S_BODY = s++;

exports.parse = parse;
exports.cat = cat;
exports.Stream = Stream;

// Parse a streaming message to a stream.
// If the message has a "body" and no "addListener", then
// just take it in and write() the body.
function parse (message) {
  return new Stream(message);
};

// WARNING: DONT EVER USE THE CAT FUNCTION IN PRODUCTION WEBSITES!!
// It works pretty great, and it's a nice test function. But if
// you use this function to parse an HTTP request from a live web
// site, then you're essentially giving the world permission to
// rack up as much memory usage as they can manage.  This function
// buffers the whole message, which is very convenient, but also
// very much the wrong thing to do in most cases.
function cat (message) {
  var p = new (events.Promise),
    stream = parse(message);
  stream.files = {};
  stream.fields = {};
  stream.addListener("partBegin", function (part) {
    if (part.filename) stream.files[part.filename] = part;
    if (part.name) stream.fields[part.name] = part;
  });
  stream.addListener("body", function (chunk) {
    stream.part.body = (stream.part.body || "") + chunk;
  });
  stream.addListener("error", function (e) { p.emitError(e) });
  stream.addListener("complete", function () { p.emitSuccess(stream) });
  return p;
};

// events:
// "partBegin", "partEnd", "body", "complete"
// everything emits on the Stream directly.
// the stream's "parts" object is a nested collection of the header objects
// check the stream's "part" member to know what it's currently chewin on.
// this.part.parent refers to that part's containing message (which may be
// the stream itself)
// child messages inherit their parent's headers
// A non-multipart message looks just like a multipart message with a
// single part.
function Stream (message) {
  var isMultiPart = multipartHeaders(message, this),
    w = isMultiPart ? writer(this) : simpleWriter(this),
    e = ender(this);
  if (message.addListener) {
    message.addListener("data", w);
    message.addListener("end", e);
    if (message.pause && message.resume) {
      this._pause = message;
    }
  } else if (message.body) {
    var self = this;
    if (message.body.pause && message.body.resume) {
      this._pause = message.body;
    }
    if (message.body.addListener) {
      message.body.addListener("data", w);
      message.body.addListener("end", e);
    } if (message.body.forEach) {
      var p = message.body.forEach(w);
      if (p && p.addCallback) p.addCallback(e);
      else e();
    } else {
      // just write a string.
      w(message.body);
      e();
    }
  }
};
Stream.prototype = {
  __proto__ : events.EventEmitter.prototype,
  error : function (ex) {
    this._error = ex;
    this.emit("error", ex);
  },
  pause : function () {
    if (this._pause) return this._pause.pause();
    throw new Error("Unsupported");
  },
  resume : function () {
    if (this._pause) return this._pause.resume();
    throw new Error("Unsupported");
  }
};

// check the headers of the message.  If it wants to be multipart,
// then we'll be returning true.  Regardless, if supplied, then
// stream will get a headers object that inherits from message's.
// If no stream object is supplied, then this function just inspects
// the message's headers for multipartness, and modifies the message
// directly.  This divergence is so that we can avoid modifying
// the original message when we want a wrapper, but still have the
// info available when it's one of our own objects.
function multipartHeaders (message, stream) {
  var field, val, contentType, contentDisposition = "";
  if (stream) stream.headers = {};
  for (var h in message.headers) if (message.headers.hasOwnProperty(h)) {
    val = message.headers[h];
    field = h.toLowerCase();
    if (stream) stream.headers[field] = val;
    if (field === "content-type") {
      contentType = val;
    } else if (field === "content-disposition") {
      contentDisposition = val;
    }
  }

  if (!Array.isArray(contentDisposition)) {
    contentDisposition = contentDisposition.split(",");
  }
  contentDisposition = contentDisposition[contentDisposition.length - 1];

  var mutate = (stream || message);

  // Name and filename can come along with either content-disposition
  // or content-type.  Well-behaved agents use CD rather than CT,
  // but sadly not all agents are well-behaved.
  [contentDisposition, contentType].forEach(function (h) {
    if (!h) return;
    var cd = h.split(/; */);
    cd.shift();
    for (var i = 0, l = cd.length; i < l; i ++) {
      var bit = cd[i].split("="),
        name = bit.shift(),
        val = stripQuotes(bit.join("="));
      if (name === "filename" || name === "name") {
        mutate[name] = val;
      }
    }
  });

  if (!contentType) {
    return false;
  }

  // legacy
  // TODO: Update this when/if jsgi-style headers are supported.
  // this will keep working, but is less efficient than it could be.
  if (!Array.isArray(contentType)) {
    contentType = contentType.split(",");
  }
  contentType = contentType[contentType.length-1];

  // make sure it's actually multipart.
  var mpType = multipartExpression.exec(contentType);
  if (!mpType) {
    return false;
  }

  // make sure we have a boundary.
  var boundary = boundaryExpression.exec(contentType);
  if (!boundary) {
    return false;
  }

  mutate.type = mpType[1];
  mutate.boundary = "--" + boundary[1];
  mutate.isMultiPart = true;

  return true;
};
function simpleWriter (stream) {
  stream.part = stream;
  stream.type = false;
  var started = false;
  return function (chunk) {
    if (!started) {
      stream.emit("partBegin", stream);
      started = true;
    }
    stream.emit("body", chunk);
  };
}
function writer (stream) {
  var buffer = "",
    state = S_NEW_PART,
    part = stream.part = stream;
  stream.parts = [];
  stream.parent = stream;
  return function (chunk) {
    if (stream._error) return;
    // write to the buffer, and then process the buffer.
    buffer += chunk;
    while (buffer.length > 0) {
      while (buffer.substr(0, 2) === CRLF) buffer = buffer.substr(2);
      switch (state) {
        case S_NEW_PART:
          // part is a multipart message.
          // we're either going to start reading a new part, or we're going to
          // end the current part, depending on whether the boundary has -- at
          // the end.  either way, we expect --boundary right away.
          var boundary = part.boundary,
            len = boundary.length,
            offset = buffer.indexOf(boundary);
          if (offset === -1) {
            if (buffer.length > MAX_BUFFER_LENGTH) {
              return stream.error(new Error(
                "Malformed: boundary not found at start of message"));
            }
            // keep waiting for it.
            return;
          }
          if (offset > 0) {
            return stream.error(Error("Malformed: data before the boundary"));
          }
          if (buffer.length < (len + 2)) {
            // we'll need to see either -- or CRLF after the boundary.
            // get it on the next pass.
            return;
          }
          if (buffer.substr(len, 2) === "--") {
            // this message is done.
            // chomp off the boundary and crlf and move up
            if (part !== stream) {
              // wait to see the crlf, unless this is the top-level message.
              if (buffer.length < (len + 4)) {
                return;
              }
              if (buffer.substr(len+2, 2) !== CRLF) {
                return stream.error(new Error(
                  "Malformed: CRLF not found after boundary"));
              }
            }
            buffer = buffer.substr(len + 4);
            stream.emit("partEnd", part);
            stream.part = part = part.parent;
            state = S_NEW_PART;
            continue;
          }
          if (part !== stream) {
            // wait to see the crlf, unless this is the top-level message.
            if (buffer.length < (len + 2)) {
              return;
            }
            if (buffer.substr(len, 2) !== CRLF) {
              return stream.error(new Error(
                "Malformed: CRLF not found after boundary"));
            }
          }
          // walk past the crlf
          buffer = buffer.substr(len + 2);
          // mint a new child part, and start parsing headers.
          stream.part = part = startPart(part);
          state = S_HEADER;
        continue;
        case S_HEADER:
          // just grab everything to the double crlf.
          var headerEnd = buffer.indexOf(CRLF+CRLF);
          if (headerEnd === -1) {
            if (buffer.length > MAX_BUFFER_LENGTH) {
              return stream.error(new Error(
                "Malformed: header unreasonably long."));
            }
            return;
          }
          var headerString = buffer.substr(0, headerEnd);
          // chomp off the header and the empty line.
          buffer = buffer.substr(headerEnd + 4);
          try {
            parseHeaderString(part.headers, headerString);
          } catch (ex) {
            return stream.error(ex);
          }
          multipartHeaders(part);

          // let the world know
          stream.emit("partBegin", part);

          if (part.isMultiPart) {
            // it has a boundary and we're ready to grab parts out.
            state = S_NEW_PART;
          } else {
            // it doesn't have a boundary, and is about to
            // start spitting out body bits.
            state = S_BODY;
          }
        continue;
        case S_BODY:
          // look for part.parent.boundary
          var boundary = part.parent.boundary,
            offset = buffer.indexOf(boundary);
          if (offset === -1) {
            // emit and wait for more data, but be careful, because
            // we might only have half of the boundary so far.
            // make sure to leave behind the boundary's length, so that we'll
            // definitely get it next time if it's on its way.
            var emittable = buffer.length - boundary.length;
            if (buffer.substr(-1) === CR) emittable -= 1;
            if (buffer.substr(-2) === CRLF) emittable -= 2;

            if (emittable > 0) {
              stream.emit("body", buffer.substr(0, emittable));
              buffer = buffer.substr(emittable);
            }
            // haven't seen the boundary, so wait for more bytes.
            return;
          }
          if (offset > 0) {
            var emit = buffer.substr(0, offset);
            if (emit.substr(-2) === CRLF) emit = emit.substr(0, emit.length-2);
            if (emit) stream.emit("body", emit);
            buffer = buffer.substr(offset);
          }

          // let em know we're done.
          stream.emit("partEnd", part);

          // now buffer starts with boundary.
          if (buffer.substr(boundary.length, 2) === "--") {
            // message end.
            // parent ends, look for a new part in the grandparent.
            stream.part = part = part.parent;
            stream.emit("partEnd", part);
            stream.part = part = part.parent;
            state = S_NEW_PART;
            buffer = buffer.substr(boundary.length + 4);
          } else {
            // another part coming for the parent message.
            stream.part = part = part.parent;
            state = S_NEW_PART;
          }
        continue;
      }
    }
  };
};

function parseHeaderString (headers, string) {
  var lines = string.split(CRLF),
    field, value, line;
  for (var i = 0, l = lines.length; i < l; i ++) {
    line = lines[i];
    if (line.match(wrapExpression)) {
      if (!field) {
        throw new Error("Malformed. First header starts with whitespace.");
      }
      value += line.replace(wrapExpression, " ");
      continue;
    } else if (field) {
      // now that we know it's not wrapping, put it on the headers obj.
      affixHeader(headers, field, value);
    }
    line = line.split(":");
    field = line.shift().toLowerCase();
    if (!field) {
      throw new Error("Malformed: improper field name.");
    }
    value = line.join(":").replace(/^\s+/, "");
  }
  // now affix the last field.
  affixHeader(headers, field, value);
};

function affixHeader (headers, field, value) {
  if (!headers.hasOwnProperty(field)) {
    headers[field] = value;
  } else if (Array.isArray(headers[field])) {
    headers[field].push(value);
  } else {
    headers[field] = [headers[field], value];
  }
};

function startPart (parent) {
  var part = {
    headers : {},
    parent : parent
  };
  parent.parts = parent.parts || [];
  parent.parts.push(part);
  return part;
};

function ender (stream) { return function () {
  if (stream._error) return;
  if (!stream.isMultiPart) stream.emit("partEnd", stream);
  stream.emit("complete");
}};

function stripslashes(str) {
  // +   original by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
  // +   improved by: Ates Goral (http://magnetiq.com)
  // +      fixed by: Mick@el
  // +   improved by: marrtins
  // +   bugfixed by: Onno Marsman
  // +   improved by: rezna
  // +   input by: Rick Waldron
  // +   reimplemented by: Brett Zamir (http://brett-zamir.me)
  // *     example 1: stripslashes("Kevin\'s code");
  // *     returns 1: "Kevin's code"
  // *     example 2: stripslashes("Kevin\\\'s code");
  // *     returns 2: "Kevin\'s code"
  return (str+"").replace(/\\(.?)/g, function (s, n1) {
    switch(n1) {
      case "\\":
        return "\\";
      case "0":
        return "\0";
      case "":
        return "";
      default:
        return n1;
    }
  });
};
function stripQuotes (str) {
  str = stripslashes(str);
  return str.substr(1, str.length - 2);
};
