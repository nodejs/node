'use strict';
class MessageEvent {
  constructor(data, target) {
    this.data = data;
    this.target = target;
  }
}

exports.emitMessage = function(data) {
  if (typeof this.onmessage === 'function')
    this.onmessage(new MessageEvent(data, this));
};
