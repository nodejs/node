'use strict';

const { getOptions, shouldNotRegisterESMLoader } = internalBinding('options');

let warnOnAllowUnauthorized = true;

let optionsMap;
let aliasesMap;

// getOptions() would serialize the option values from C++ land.
// It would error if the values are queried before bootstrap is
// complete so that we don't accidentally include runtime-dependent
// states into a runtime-independent snapshot.
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

function getOptionValue(optionName) {
  const options = getOptionsFromBinding();
  if (optionName.startsWith('--no-')) {
    const option = options.get('--' + optionName.slice(5));
    return option && !option.value;
  }
  return options.get(optionName)?.value;
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
