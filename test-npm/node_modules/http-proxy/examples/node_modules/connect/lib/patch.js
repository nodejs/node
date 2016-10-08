
/*!
 * Connect
 * Copyright(c) 2011 TJ Holowaychuk
 * MIT Licensed
 */

/**
 * Module dependencies.
 */

var http = require('http')
  , res = http.OutgoingMessage.prototype;

// original setHeader()

var setHeader = res.setHeader;

// original _renderHeaders()

var _renderHeaders = res._renderHeaders;

if (res._hasConnectPatch) return;

/**
 * Set header `field` to `val`, special-casing
 * the `Set-Cookie` field for multiple support.
 *
 * @param {String} field
 * @param {String} val
 * @api public
 */

res.setHeader = function(field, val){
  var key = field.toLowerCase()
    , prev;

  // special-case Set-Cookie
  if (this._headers && 'set-cookie' == key) {
    if (prev = this.getHeader(field)) {
      val = Array.isArray(prev)
        ? prev.concat(val)
        : [prev, val];
    }
  // charset
  } else if ('content-type' == key && this.charset) {
    val += '; charset=' + this.charset;
  }

  return setHeader.call(this, field, val);
};

/**
 * Proxy `res.end()` to expose a 'header' event,
 * allowing arbitrary augmentation before the header
 * fields are written to the socket.
 *
 * NOTE: this _only_ supports node's progressive header
 * field API aka `res.setHeader()`.
 */

res._renderHeaders = function(){
  this.emit('header');
  return _renderHeaders.call(this);
};

res._hasConnectPatch = true;
