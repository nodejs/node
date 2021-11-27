const babel = require("./babel-core.cjs");

const maybeParse = require("./maybeParse.cjs");

const {
  getVisitorKeys,
  getTokLabels
} = require("./ast-info.cjs");

const {
  normalizeBabelParseConfig,
  normalizeBabelParseConfigSync
} = require("./configuration.cjs");

module.exports = function handleMessage(action, payload) {
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
      return normalizeBabelParseConfig(payload.options).then(options => maybeParse(payload.code, options));

    case "MAYBE_PARSE_SYNC":
      {
        return maybeParse(payload.code, normalizeBabelParseConfigSync(payload.options));
      }
  }

  throw new Error(`Unknown internal parser worker action: ${action}`);
};