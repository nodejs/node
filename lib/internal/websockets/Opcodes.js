'use strict'
const Validation = require('./Validation').Validation;
const ErrorCodes = require('./ErrorCodes');

/**
 * utils
 */

function readUInt16BE(start) {
  return (this[start]<<8) +
         this[start+1];
}

function readUInt32BE(start) {
  return (this[start]<<24) +
         (this[start+1]<<16) +
         (this[start+2]<<8) +
         this[start+3];
}

function clone(obj) {
  var cloned = {};
  for (var k in obj) {
    if (obj.hasOwnProperty(k)) {
      cloned[k] = obj[k];
    }
  }
  return cloned;
}

/**
* Opcode handlers
*/
var opcodes = {}

opcodes['text'] = {
  start: function(data) {
    var self = this;
    // decode length
    var firstLength = data[1] & 0x7f;
    if (firstLength < 126) {
      opcodes['text'].getData.call(self, firstLength);
    }
    else if (firstLength == 126) {
      self.expectHeader(2, function(data) {
        opcodes['text'].getData.call(self, readUInt16BE.call(data, 0));
      });
    }
    else if (firstLength == 127) {
      self.expectHeader(8, function(data) {
        if (readUInt32BE.call(data, 0) != 0) {
          self.error('packets with length spanning more than 32 bit is currently not supported', 1008);
          return;
        }
        opcodes['text'].getData.call(self, readUInt32BE.call(data, 4));
      });
    }
  },
  getData: function(length) {
    var self = this;
    if (self.state.masked) {
      self.expectHeader(4, function(data) {
        var mask = data;
        self.expectData(length, function(data) {
          opcodes['text'].finish.call(self, mask, data);
        });
      });
    }
    else {
      self.expectData(length, function(data) {
        opcodes['text'].finish.call(self, null, data);
      });
    }
  },
  finish: function(mask, data) {
    var self = this;
    var packet = this.unmask(mask, data, true) || new Buffer(0);
    var state = clone(this.state);
    this.messageHandlers.push(function(callback) {
      self.applyExtensions(packet, state.lastFragment, state.compressed, function(err, buffer) {
        if (err) return self.error(err.message, 1007);
        if (buffer != null) self.currentMessage.push(buffer);

        if (state.lastFragment) {
          var messageBuffer = self.concatBuffers(self.currentMessage);
          self.currentMessage = [];
          if (!Validation.isValidUTF8(messageBuffer)) {
            self.error('invalid utf8 sequence', 1007);
            return;
          }
          self.ontext(messageBuffer.toString('utf8'), {masked: state.masked, buffer: messageBuffer});
        }
        callback();
      });
    });
    this.flush();
    this.endPacket();
  }
}


opcodes['binary'] = {
  start: function(data) {
    var self = this;
    // decode length
    var firstLength = data[1] & 0x7f;
    if (firstLength < 126) {
      opcodes['binary'].getData.call(self, firstLength);
    }
    else if (firstLength == 126) {
      self.expectHeader(2, function(data) {
        opcodes['binary'].getData.call(self, readUInt16BE.call(data, 0));
      });
    }
    else if (firstLength == 127) {
      self.expectHeader(8, function(data) {
        if (readUInt32BE.call(data, 0) != 0) {
          self.error('packets with length spanning more than 32 bit is currently not supported', 1008);
          return;
        }
        opcodes['binary'].getData.call(self, readUInt32BE.call(data, 4, true));
      });
    }
  },
  getData: function(length) {
    var self = this;
    if (self.state.masked) {
      self.expectHeader(4, function(data) {
        var mask = data;
        self.expectData(length, function(data) {
          opcodes['binary'].finish.call(self, mask, data);
        });
      });
    }
    else {
      self.expectData(length, function(data) {
        opcodes['binary'].finish.call(self, null, data);
      });
    }
  },
  finish: function(mask, data) {
    var self = this;
    var packet = this.unmask(mask, data, true) || new Buffer(0);
    var state = clone(this.state);
    this.messageHandlers.push(function(callback) {
      self.applyExtensions(packet, state.lastFragment, state.compressed, function(err, buffer) {
        if (err) return self.error(err.message, 1007);
        if (buffer != null) self.currentMessage.push(buffer);
        if (state.lastFragment) {
          var messageBuffer = self.concatBuffers(self.currentMessage);
          self.currentMessage = [];
          self.onbinary(messageBuffer, {masked: state.masked, buffer: messageBuffer});
        }
        callback();
      });
    });
    this.flush();
    this.endPacket();
  }
},


opcodes['close'] = {
  start: function(data) {
    var self = this;
    if (self.state.lastFragment == false) {
      self.error('fragmented close is not supported', 1002);
      return;
    }

    // decode length
    var firstLength = data[1] & 0x7f;
    if (firstLength < 126) {
      opcodes['close'].getData.call(self, firstLength);
    }
    else {
      self.error('control frames cannot have more than 125 bytes of data', 1002);
    }
  },
  getData: function(length) {
    var self = this;
    if (self.state.masked) {
      self.expectHeader(4, function(data) {
        var mask = data;
        self.expectData(length, function(data) {
          opcodes['close'].finish.call(self, mask, data);
        });
      });
    }
    else {
      self.expectData(length, function(data) {
        opcodes['close'].finish.call(self, null, data);
      });
    }
  },
  finish: function(mask, data) {
    var self = this;
    data = self.unmask(mask, data, true);

    var state = clone(this.state);
    this.messageHandlers.push(function() {
      if (data && data.length == 1) {
        self.error('close packets with data must be at least two bytes long', 1002);
        return;
      }
      var code = data && data.length > 1 ? readUInt16BE.call(data, 0) : 1000;
      if (!ErrorCodes.isValidErrorCode(code)) {
        self.error('invalid error code', 1002);
        return;
      }
      var message = '';
      if (data && data.length > 2) {
        var messageBuffer = data.slice(2);
        if (!Validation.isValidUTF8(messageBuffer)) {
          self.error('invalid utf8 sequence', 1007);
          return;
        }
        message = messageBuffer.toString('utf8');
      }
      self.onclose(code, message, {masked: state.masked});
      self.reset();
    });
    this.flush();
  },
}


opcodes['ping'] = {
  start: function(data) {
    var self = this;
    if (self.state.lastFragment == false) {
      self.error('fragmented ping is not supported', 1002);
      return;
    }

    // decode length
    var firstLength = data[1] & 0x7f;
    if (firstLength < 126) {
      opcodes['ping'].getData.call(self, firstLength);
    }
    else {
      self.error('control frames cannot have more than 125 bytes of data', 1002);
    }
  },
  getData: function(length) {
    var self = this;
    if (self.state.masked) {
      self.expectHeader(4, function(data) {
        var mask = data;
        self.expectData(length, function(data) {
          opcodes['ping'].finish.call(self, mask, data);
        });
      });
    }
    else {
      self.expectData(length, function(data) {
        opcodes['ping'].finish.call(self, null, data);
      });
    }
  },
  finish: function(mask, data) {
    var self = this;
    data = this.unmask(mask, data, true);
    var state = clone(this.state);
    this.messageHandlers.push(function(callback) {
      self.onping(data, {masked: state.masked, binary: true});
      callback();
    });
    this.flush();
    this.endPacket();
  }
}


opcodes['pong'] = {
  start: function(data) {
    var self = this;
    if (self.state.lastFragment == false) {
      self.error('fragmented pong is not supported', 1002);
      return;
    }

    // decode length
    var firstLength = data[1] & 0x7f;
    if (firstLength < 126) {
      opcodes['pong'].getData.call(self, firstLength);
    }
    else {
      self.error('control frames cannot have more than 125 bytes of data', 1002);
    }
  },
  getData: function(length) {
    var self = this;
    if (this.state.masked) {
      this.expectHeader(4, function(data) {
        var mask = data;
        self.expectData(length, function(data) {
          opcodes['pong'].finish.call(self, mask, data);
        });
      });
    }
    else {
      this.expectData(length, function(data) {
        opcodes['pong'].finish.call(self, null, data);
      });
    }
  },
  finish: function(mask, data) {
    var self = this;
    data = self.unmask(mask, data, true);
    var state = clone(this.state);
    this.messageHandlers.push(function(callback) {
      self.onpong(data, {masked: state.masked, binary: true});
      callback();
    });
    this.flush();
    this.endPacket();
  }
}

module.exports = opcodes
