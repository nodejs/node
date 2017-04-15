'use strict';

function ondrain() {
  if (this._httpMessage) this._httpMessage.emit('drain');
}

module.exports = {
  outHeadersKey: Symbol('outHeadersKey'),
  ondrain,
};
