'use strict';

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_MISSING_ARGS,
  },
} = require('internal/errors');
const webidl = require('internal/webidl');

const {
  structuredClone: nativeStructuredClone,
} = internalBinding('messaging');

webidl.converters['sequence<object>'] = webidl.createSequenceConverter(webidl.converters.any);

function structuredClone(value, options) {
  if (arguments.length === 0) {
    throw new ERR_MISSING_ARGS('The value argument must be specified');
  }

  // TODO(jazelly): implement generic webidl dictionary converter
  const prefix = 'Options';
  const optionsType = webidl.type(options);
  if (optionsType !== 'Undefined' && optionsType !== 'Null' && optionsType !== 'Object') {
    throw new ERR_INVALID_ARG_TYPE(
      prefix,
      ['object', 'null', 'undefined'],
      options,
    );
  }
  const key = 'transfer';
  const idlOptions = { __proto__: null, [key]: [] };
  if (options !== undefined && options !== null && key in options && options[key] !== undefined) {
    idlOptions[key] = webidl.converters['sequence<object>'](options[key], {
      __proto__: null,
      context: 'Transfer',
    });
  }

  const serializedData = nativeStructuredClone(value, idlOptions);
  return serializedData;
}

module.exports = {
  structuredClone,
};
