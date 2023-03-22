'use strict';

const {
  codes: { ERR_MISSING_ARGS },
} = require('internal/errors');

const {
  lazyDOMException,
} = require('internal/util');

const {
  MessageChannel,
  receiveMessageOnPort,
} = require('internal/worker/io');

const {
  isURL,
  isURLSearchParams,
} = require('internal/url');

let channel;
function structuredClone(value, options = undefined) {
  if (arguments.length === 0) {
    throw new ERR_MISSING_ARGS('value');
  }

  if (isURL(value)) {
    throw new lazyDOMException(
      'URL: no structured serialize/deserialize support',
      'DataCloneError');
  } else if (isURLSearchParams(value)) {
    throw new lazyDOMException(
      'URLSearchParams: no structured serialize/deserialize support',
      'DataCloneError');
  }

  // TODO: Improve this with a more efficient solution that avoids
  // instantiating a MessageChannel
  channel ??= new MessageChannel();
  channel.port1.unref();
  channel.port2.unref();
  channel.port1.postMessage(value, options?.transfer);
  return receiveMessageOnPort(channel.port2).message;
}

module.exports = {
  structuredClone,
};
