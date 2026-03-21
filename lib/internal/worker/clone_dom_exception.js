'use strict';

// Delegate to the actual DOMException/QuotaExceededError implementation.
const messaging = internalBinding('messaging');
module.exports = {
  DOMException: messaging.DOMException,
  QuotaExceededError: messaging.QuotaExceededError,
};
