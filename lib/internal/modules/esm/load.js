'use strict';

const { defaultGetFormat } = require('internal/modules/esm/get_format');
const { defaultGetSource } = require('internal/modules/esm/get_source');
const { validateAssertions } = require('internal/modules/esm/assert');

/**
 * Node.js default load hook.
 * @param {string} url
 * @param {object} context
 * @returns {object}
 */
async function defaultLoad(url, context) {
  const { importAssertions } = context;
  let {
    format,
    source,
  } = context;

  if (format == null) {
    format = await defaultGetFormat(url, context);
  }

  validateAssertions(url, format, importAssertions);

  if (
    format === 'builtin' ||
    format === 'commonjs'
  ) {
    source = null;
  } else if (source == null) {
    source = await defaultGetSource(url, context);
  }

  return {
    format,
    source,
  };
}

module.exports = {
  defaultLoad,
};
