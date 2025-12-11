'use strict';

// Delegate to the actual DOMException implementation.
module.exports = {
  DOMException: internalBinding('messaging').DOMException,
};
