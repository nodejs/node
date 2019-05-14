module.exports = function stringifyValidator(validator, nodePrefix) {
  if (validator === undefined) {
    return "any";
  }

  if (validator.each) {
    return `Array<${stringifyValidator(validator.each, nodePrefix)}>`;
  }

  if (validator.chainOf) {
    return stringifyValidator(validator.chainOf[1], nodePrefix);
  }

  if (validator.oneOf) {
    return validator.oneOf.map(JSON.stringify).join(" | ");
  }

  if (validator.oneOfNodeTypes) {
    return validator.oneOfNodeTypes.map(_ => nodePrefix + _).join(" | ");
  }

  if (validator.oneOfNodeOrValueTypes) {
    return validator.oneOfNodeOrValueTypes
      .map(_ => {
        return isValueType(_) ? _ : nodePrefix + _;
      })
      .join(" | ");
  }

  if (validator.type) {
    return validator.type;
  }

  return ["any"];
};

/**
 * Heuristic to decide whether or not the given type is a value type (eg. "null")
 * or a Node type (eg. "Expression").
 */
function isValueType(type) {
  return type.charAt(0).toLowerCase() === type.charAt(0);
}
