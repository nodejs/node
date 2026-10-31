'use strict';

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const { emitExperimentalWarning } = require('internal/util');
const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');
const { parseDocuments } = require('internal/yaml/parser');

emitExperimentalWarning('YAML');

let TextDecoder;
function decode(input) {
  TextDecoder ??= require('internal/encoding').TextDecoder;
  return new TextDecoder('utf-8').decode(input);
}

/**
 * Parses a YAML stream. A stream with one document yields its value, a
 * stream with several documents yields an array of their values.
 * @param {string | ArrayBuffer | SharedArrayBuffer | ArrayBufferView} input
 * @returns {unknown}
 */
function parse(input) {
  let text;
  if (typeof input === 'string') {
    text = input;
  } else if (isArrayBufferView(input) || isAnyArrayBuffer(input)) {
    text = decode(input);
  } else {
    throw new ERR_INVALID_ARG_TYPE(
      'input', ['string', 'ArrayBuffer', 'SharedArrayBuffer', 'ArrayBufferView'], input);
  }
  const documents = parseDocuments(text);
  if (documents.length === 0) return null;
  if (documents.length === 1) return documents[0];
  return documents;
}

module.exports = {
  parse,
};
