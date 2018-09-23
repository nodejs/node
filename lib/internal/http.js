'use strict';

const { setUnrefTimeout } = require('internal/timers');

var dateCache;
function utcDate() {
  if (!dateCache) {
    const d = new Date();
    dateCache = d.toUTCString();

    setUnrefTimeout(resetCache, 1000 - d.getMilliseconds());
  }
  return dateCache;
}

function resetCache() {
  dateCache = undefined;
}

function ondrain() {
  if (this._httpMessage) this._httpMessage.emit('drain');
}

module.exports = {
  outHeadersKey: Symbol('outHeadersKey'),
  ondrain,
  utcDate
};
