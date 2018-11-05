'use strict';

const { getOptions } = internalBinding('options');
const { options, aliases } = getOptions();

function getOptionValue(option) {
  const result = options.get(option);
  if (!result) {
    return undefined;
  }
  return result.value;
}

module.exports = {
  options,
  aliases,
  getOptionValue
};
