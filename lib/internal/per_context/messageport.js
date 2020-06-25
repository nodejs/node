'use strict';
const {
  SymbolFor,
} = primordials;

class MessageEvent {
  constructor(data, target, type) {
    this.data = data;
    this.target = target;
    this.type = type;
  }
}

const kMessageEventSymbol = SymbolFor('nodejs.internal.MessageEvent');

exports.emitMessage = function(data, type) {
  if (typeof this.dispatchEvent === 'function') {
    const MessageEvent = this[kMessageEventSymbol];
    const event = new MessageEvent(data, this, type);
    this.dispatchEvent(event);
    return;
  }

  const event = new MessageEvent(data, this, type);
  if (type === 'message') {
    if (typeof this.onmessage === 'function')
      this.onmessage(event);
  } else {
    // eslint-disable-next-line no-lonely-if
    if (typeof this.onmessageerror === 'function')
      this.onmessageerror(event);
  }
};
