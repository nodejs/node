"use strict";

var _astInfo = require("./ast-info.cjs");
var _configuration = require("./configuration.cjs");
var _client = require("../client.cjs");
const babel = require("./babel-core.cjs");
const maybeParse = require("./maybeParse.cjs");
module.exports = function handleMessage(action, payload) {
  switch (action) {
    case _client.ACTIONS.GET_VERSION:
      return babel.version;
    case _client.ACTIONS.GET_TYPES_INFO:
      return {
        FLOW_FLIPPED_ALIAS_KEYS: babel.types.FLIPPED_ALIAS_KEYS.Flow,
        VISITOR_KEYS: babel.types.VISITOR_KEYS
      };
    case _client.ACTIONS.GET_TOKEN_LABELS:
      return (0, _astInfo.getTokLabels)();
    case _client.ACTIONS.GET_VISITOR_KEYS:
      return (0, _astInfo.getVisitorKeys)();
    case _client.ACTIONS.MAYBE_PARSE:
      return (0, _configuration.normalizeBabelParseConfig)(payload.options).then(options => maybeParse(payload.code, options));
    case _client.ACTIONS.MAYBE_PARSE_SYNC:
      {
        return maybeParse(payload.code, (0, _configuration.normalizeBabelParseConfigSync)(payload.options));
      }
  }
  throw new Error(`Unknown internal parser worker action: ${action}`);
};

//# sourceMappingURL=handle-message.cjs.map
