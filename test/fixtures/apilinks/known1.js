'use strict';

class Known {
  getThing() {}
}

Known.classField = 8 * 1024;

function createKnown(size) {
}

module.exports = {
  Known,
  createKnown
};

Object.defineProperty(exports, 'constants', {
  configurable: false,
  enumerable: true,
  value: {k1: 1}
});

const expected = {
  "Known.classField": "known1.js#L7",
  "known1.createKnown": "known1.js#L9"
};
