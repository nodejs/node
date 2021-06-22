const babel = require("./babel-core.cjs");

const maybeParse = require("./maybeParse.cjs");

const {
  getVisitorKeys,
  getTokLabels
} = require("./ast-info.cjs");

const normalizeBabelParseConfig = require("./configuration.cjs");

function handleMessage(action, payload) {
  switch (action) {
    case "GET_VERSION":
      return babel.version;

    case "GET_TYPES_INFO":
      return {
        FLOW_FLIPPED_ALIAS_KEYS: babel.types.FLIPPED_ALIAS_KEYS.Flow,
        VISITOR_KEYS: babel.types.VISITOR_KEYS
      };

    case "GET_TOKEN_LABELS":
      return getTokLabels();

    case "GET_VISITOR_KEYS":
      return getVisitorKeys();

    case "MAYBE_PARSE":
      {
        return maybeParse(payload.code, normalizeBabelParseConfig(payload.options));
      }
  }

  throw new Error(`Unknown internal parser worker action: ${action}`);
}

{
  module.exports = handleMessage;
}