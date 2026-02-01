'use strict';

const {
  ArrayPrototypeMap,
  ArrayPrototypeSort,
  ObjectEntries,
  ObjectFromEntries,
  ObjectKeys,
  StringPrototypeReplace,
} = primordials;

const {
  getCLIOptionsValues,
  getCLIOptionsInfo,
  getOptionsAsFlags,
  getEmbedderOptions: getEmbedderOptionsFromBinding,
  getEnvOptionsInputType,
  getNamespaceOptionsInputType,
} = internalBinding('options');

let warnOnAllowUnauthorized = true;

let optionsDict;
let cliInfo;
let embedderOptions;
let optionsFlags;

// getCLIOptionsValues() would serialize the option values from C++ land.
// It would error if the values are queried before bootstrap is
// complete so that we don't accidentally include runtime-dependent
// states into a runtime-independent snapshot.
function getCLIOptionsFromBinding() {
  return optionsDict ??= getCLIOptionsValues();
}

function getCLIOptionsInfoFromBinding() {
  return cliInfo ??= getCLIOptionsInfo();
}

function getOptionsAsFlagsFromBinding() {
  return optionsFlags ??= getOptionsAsFlags();
}

function getEmbedderOptions() {
  return embedderOptions ??= getEmbedderOptionsFromBinding();
}

function generateConfigJsonSchema() {
  const envOptionsMap = getEnvOptionsInputType();
  const namespaceOptionsMap = getNamespaceOptionsInputType();

  function createPropertyForOptionDetail(detail) {
    const { type, description } = detail;
    if (type === 'array') {
      return {
        __proto__: null,
        oneOf: [
          { __proto__: null, type: 'string' },
          { __proto__: null, type: 'array', minItems: 1, items: { __proto__: null, type: 'string' } },
        ],
        description,
      };
    }

    return { __proto__: null, type, description };
  }

  const schema = {
    __proto__: null,
    $schema: 'https://json-schema.org/draft/2020-12/schema',
    additionalProperties: false,
    required: [],
    properties: {
      $schema: {
        __proto__: null,
        type: 'string',
      },
      nodeOptions: {
        __proto__: null,
        additionalProperties: false,
        required: [],
        properties: { __proto__: null },
        type: 'object',
      },
      __proto__: null,
    },
    type: 'object',
  };

  // Get the root properties object for adding namespaces
  const rootProperties = schema.properties;
  const nodeOptions = rootProperties.nodeOptions.properties;

  // Add env options to nodeOptions (backward compatibility)
  for (const { 0: key, 1: type } of ObjectEntries(envOptionsMap)) {
    const keyWithoutPrefix = StringPrototypeReplace(key, '--', '');
    nodeOptions[keyWithoutPrefix] = createPropertyForOptionDetail(type);
  }

  // Add namespace properties at the root level
  for (const { 0: namespace, 1: optionsMap } of namespaceOptionsMap) {
    // Create namespace object at the root level
    rootProperties[namespace] = {
      __proto__: null,
      type: 'object',
      additionalProperties: false,
      required: [],
      properties: { __proto__: null },
    };

    const namespaceProperties = rootProperties[namespace].properties;

    // Add all options for this namespace
    for (const { 0: optionName, 1: optionType } of ObjectEntries(optionsMap)) {
      const keyWithoutPrefix = StringPrototypeReplace(optionName, '--', '');
      namespaceProperties[keyWithoutPrefix] = createPropertyForOptionDetail(optionType);
    }

    // Sort the namespace properties alphabetically
    const sortedNamespaceKeys = ArrayPrototypeSort(ObjectKeys(namespaceProperties));
    const sortedNamespaceProperties = ObjectFromEntries(
      ArrayPrototypeMap(sortedNamespaceKeys, (key) => [key, namespaceProperties[key]]),
    );
    rootProperties[namespace].properties = sortedNamespaceProperties;
  }

  // Sort the top-level properties by key alphabetically
  const sortedKeys = ArrayPrototypeSort(ObjectKeys(nodeOptions));
  const sortedProperties = ObjectFromEntries(
    ArrayPrototypeMap(sortedKeys, (key) => [key, nodeOptions[key]]),
  );

  schema.properties.nodeOptions.properties = sortedProperties;

  // Also sort the root level properties
  const sortedRootKeys = ArrayPrototypeSort(ObjectKeys(rootProperties));
  const sortedRootProperties = ObjectFromEntries(
    ArrayPrototypeMap(sortedRootKeys, (key) => [key, rootProperties[key]]),
  );

  schema.properties = sortedRootProperties;

  return schema;
}

function refreshOptions() {
  optionsDict = undefined;
}

function getOptionValue(optionName) {
  return getCLIOptionsFromBinding()[optionName];
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
  getCLIOptionsInfo: getCLIOptionsInfoFromBinding,
  getOptionValue,
  getOptionsAsFlagsFromBinding,
  getAllowUnauthorized,
  getEmbedderOptions,
  generateConfigJsonSchema,
  refreshOptions,
};
