'use strict';

const { defaultGetFormat } = require('internal/modules/esm/get_format');
const { defaultGetSource } = require('internal/modules/esm/get_source');
const { translators } = require('internal/modules/esm/translators');

async function defaultLoad(url, context) {
  let {
    format,
    source,
  } = context;

  if (!translators.has(format)) format = defaultGetFormat(url);

  if (
    format === 'builtin' ||
    format === 'commonjs'
  ) {
    source = null;
  } else if (source == null) {
    source = await defaultGetSource(url, { format });
  }

  return {
    format,
    source,
  };
}

module.exports = {
  defaultLoad,
};
