'use strict';

const { getOptions } = internalBinding('options');
const { options, aliases } = getOptions();

let warnOnAllowUnauthorized = true;

function getOptionValue(option) {
  const result = options.get(option);
  if (!result) {
    return undefined;
  }
  return result.value;
}

function getAllowUnauthorized() {
  const allowUnauthorized = process.env.NODE_TLS_REJECT_UNAUTHORIZED === '0';

  if (allowUnauthorized && warnOnAllowUnauthorized) {
    warnOnAllowUnauthorized = false;
    process.emitWarning(
      'Setting the NODE_TLS_REJECT_UNAUTHORIZED ' +
      'environment variable to \'0\' makes TLS connections ' +
      'and HTTPS requests insecure by disabling ' +
      'certificate verification.');
  }
  return allowUnauthorized;
}

module.exports = {
  options,
  aliases,
  getOptionValue,
  getAllowUnauthorized,
};
