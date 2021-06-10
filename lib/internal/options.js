'use strict';

const { getOptions, shouldNotRegisterESMLoader } = internalBinding('options');

let warnOnAllowUnauthorized = true;

let optionsMap;
let aliasesMap;

function getOptionsFromBinding() {
  if (!optionsMap) {
    ({ options: optionsMap } = getOptions());
  }
  return optionsMap;
}

function getAliasesFromBinding() {
  if (!aliasesMap) {
    ({ aliases: aliasesMap } = getOptions());
  }
  return aliasesMap;
}

function getOptionValue(option) {
  return getOptionsFromBinding().get(option)?.value;
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
  get options() {
    return getOptionsFromBinding();
  },
  get aliases() {
    return getAliasesFromBinding();
  },
  getOptionValue,
  getAllowUnauthorized,
  shouldNotRegisterESMLoader
};
