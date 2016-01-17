
var zlib = require('zlib');

var AVAILABLE_WINDOW_BITS = [8, 9, 10, 11, 12, 13, 14, 15];
var DEFAULT_WINDOW_BITS = 15;
var DEFAULT_MEM_LEVEL = 8;

PerMessageDeflate.extensionName = 'permessage-deflate';

/**
 * Per-message Compression Extensions implementation
 */

function PerMessageDeflate(options, isServer) {
  if (this instanceof PerMessageDeflate === false) {
    throw new TypeError("Classes can't be function-called");
  }

  this._options = options || {};
  this._isServer = !!isServer;
  this._inflate = null;
  this._deflate = null;
  this.params = null;
}

/**
 * Create extension parameters offer
 *
 * @api public
 */

PerMessageDeflate.prototype.offer = function() {
  var params = {};
  if (this._options.serverNoContextTakeover) {
    params.server_no_context_takeover = true;
  }
  if (this._options.clientNoContextTakeover) {
    params.client_no_context_takeover = true;
  }
  if (this._options.serverMaxWindowBits) {
    params.server_max_window_bits = this._options.serverMaxWindowBits;
  }
  if (this._options.clientMaxWindowBits) {
    params.client_max_window_bits = this._options.clientMaxWindowBits;
  } else if (this._options.clientMaxWindowBits == null) {
    params.client_max_window_bits = true;
  }
  return params;
};

/**
 * Accept extension offer
 *
 * @api public
 */

PerMessageDeflate.prototype.accept = function(paramsList) {
  paramsList = this.normalizeParams(paramsList);

  var params;
  if (this._isServer) {
    params = this.acceptAsServer(paramsList);
  } else {
    params = this.acceptAsClient(paramsList);
  }

  this.params = params;
  return params;
};

/**
 * Releases all resources used by the extension
 *
 * @api public
 */

PerMessageDeflate.prototype.cleanup = function() {
  if (this._inflate) {
    if (this._inflate.writeInProgress) {
      this._inflate.pendingClose = true;
    } else {
      if (this._inflate.close) this._inflate.close();
      this._inflate = null;
    }
  }
  if (this._deflate) {
    if (this._deflate.writeInProgress) {
      this._deflate.pendingClose = true;
    } else {
      if (this._deflate.close) this._deflate.close();
      this._deflate = null;
    }
  }
};

/**
 * Accept extension offer from client
 *
 * @api private
 */

PerMessageDeflate.prototype.acceptAsServer = function(paramsList) {
  var accepted = {};
  var result = paramsList.some(function(params) {
    accepted = {};
    if (this._options.serverNoContextTakeover === false && params.server_no_context_takeover) {
      return;
    }
    if (this._options.serverMaxWindowBits === false && params.server_max_window_bits) {
      return;
    }
    if (typeof this._options.serverMaxWindowBits === 'number' &&
        typeof params.server_max_window_bits === 'number' &&
        this._options.serverMaxWindowBits > params.server_max_window_bits) {
      return;
    }
    if (typeof this._options.clientMaxWindowBits === 'number' && !params.client_max_window_bits) {
      return;
    }

    if (this._options.serverNoContextTakeover || params.server_no_context_takeover) {
      accepted.server_no_context_takeover = true;
    }
    if (this._options.clientNoContextTakeover) {
      accepted.client_no_context_takeover = true;
    }
    if (this._options.clientNoContextTakeover !== false && params.client_no_context_takeover) {
      accepted.client_no_context_takeover = true;
    }
    if (typeof this._options.serverMaxWindowBits === 'number') {
      accepted.server_max_window_bits = this._options.serverMaxWindowBits;
    } else if (typeof params.server_max_window_bits === 'number') {
      accepted.server_max_window_bits = params.server_max_window_bits;
    }
    if (typeof this._options.clientMaxWindowBits === 'number') {
      accepted.client_max_window_bits = this._options.clientMaxWindowBits;
    } else if (this._options.clientMaxWindowBits !== false && typeof params.client_max_window_bits === 'number') {
      accepted.client_max_window_bits = params.client_max_window_bits;
    }
    return true;
  }, this);

  if (!result) {
    throw new Error('Doesn\'t support the offered configuration');
  }

  return accepted;
};

/**
 * Accept extension response from server
 *
 * @api privaye
 */

PerMessageDeflate.prototype.acceptAsClient = function(paramsList) {
  var params = paramsList[0];
  if (this._options.clientNoContextTakeover != null) {
    if (this._options.clientNoContextTakeover === false && params.client_no_context_takeover) {
      throw new Error('Invalid value for "client_no_context_takeover"');
    }
  }
  if (this._options.clientMaxWindowBits != null) {
    if (this._options.clientMaxWindowBits === false && params.client_max_window_bits) {
      throw new Error('Invalid value for "client_max_window_bits"');
    }
    if (typeof this._options.clientMaxWindowBits === 'number' &&
        (!params.client_max_window_bits || params.client_max_window_bits > this._options.clientMaxWindowBits)) {
      throw new Error('Invalid value for "client_max_window_bits"');
    }
  }
  return params;
};

/**
 * Normalize extensions parameters
 *
 * @api private
 */

PerMessageDeflate.prototype.normalizeParams = function(paramsList) {
  return paramsList.map(function(params) {
    Object.keys(params).forEach(function(key) {
      var value = params[key];
      if (value.length > 1) {
        throw new Error('Multiple extension parameters for ' + key);
      }

      value = value[0];

      switch (key) {
      case 'server_no_context_takeover':
      case 'client_no_context_takeover':
        if (value !== true) {
          throw new Error('invalid extension parameter value for ' + key + ' (' + value + ')');
        }
        params[key] = true;
        break;
      case 'server_max_window_bits':
      case 'client_max_window_bits':
        if (typeof value === 'string') {
          value = parseInt(value, 10);
          if (!~AVAILABLE_WINDOW_BITS.indexOf(value)) {
            throw new Error('invalid extension parameter value for ' + key + ' (' + value + ')');
          }
        }
        if (!this._isServer && value === true) {
          throw new Error('Missing extension parameter value for ' + key);
        }
        params[key] = value;
        break;
      default:
        throw new Error('Not defined extension parameter (' + key + ')');
      }
    }, this);
    return params;
  }, this);
};

/**
 * Decompress message
 *
 * @api public
 */

PerMessageDeflate.prototype.decompress = function (data, fin, callback) {
  var endpoint = this._isServer ? 'client' : 'server';

  if (!this._inflate) {
    var maxWindowBits = this.params[endpoint + '_max_window_bits'];
    this._inflate = zlib.createInflateRaw({
      windowBits: 'number' === typeof maxWindowBits ? maxWindowBits : DEFAULT_WINDOW_BITS
    });
  }
  this._inflate.writeInProgress = true;

  var self = this;
  var buffers = [];

  this._inflate.on('error', onError).on('data', onData);
  this._inflate.write(data);
  if (fin) {
    this._inflate.write(new Buffer([0x00, 0x00, 0xff, 0xff]));
  }
  this._inflate.flush(function() {
    cleanup();
    callback(null, Buffer.concat(buffers));
  });

  function onError(err) {
    cleanup();
    callback(err);
  }

  function onData(data) {
    buffers.push(data);
  }

  function cleanup() {
    if (!self._inflate) return;
    self._inflate.removeListener('error', onError);
    self._inflate.removeListener('data', onData);
    self._inflate.writeInProgress = false;
    if ((fin && self.params[endpoint + '_no_context_takeover']) || self._inflate.pendingClose) {
      if (self._inflate.close) self._inflate.close();
      self._inflate = null;
    }
  }
};

/**
 * Compress message
 *
 * @api public
 */

PerMessageDeflate.prototype.compress = function (data, fin, callback) {
  var endpoint = this._isServer ? 'server' : 'client';

  if (!this._deflate) {
    var maxWindowBits = this.params[endpoint + '_max_window_bits'];
    this._deflate = zlib.createDeflateRaw({
      flush: zlib.Z_SYNC_FLUSH,
      windowBits: 'number' === typeof maxWindowBits ? maxWindowBits : DEFAULT_WINDOW_BITS,
      memLevel: this._options.memLevel || DEFAULT_MEM_LEVEL
    });
  }
  this._deflate.writeInProgress = true;

  var self = this;
  var buffers = [];

  this._deflate.on('error', onError).on('data', onData);
  this._deflate.write(data);
  this._deflate.flush(function() {
    cleanup();
    var data = Buffer.concat(buffers);
    if (fin) {
      data = data.slice(0, data.length - 4);
    }
    callback(null, data);
  });

  function onError(err) {
    cleanup();
    callback(err);
  }

  function onData(data) {
    buffers.push(data);
  }

  function cleanup() {
    if (!self._deflate) return;
    self._deflate.removeListener('error', onError);
    self._deflate.removeListener('data', onData);
    self._deflate.writeInProgress = false;
    if ((fin && self.params[endpoint + '_no_context_takeover']) || self._deflate.pendingClose) {
      if (self._deflate.close) self._deflate.close();
      self._deflate = null;
    }
  }
};

module.exports = PerMessageDeflate;
