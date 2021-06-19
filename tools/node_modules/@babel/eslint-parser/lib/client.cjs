const path = require("path");

let send;
exports.getVersion = sendCached("GET_VERSION");
exports.getTypesInfo = sendCached("GET_TYPES_INFO");
exports.getVisitorKeys = sendCached("GET_VISITOR_KEYS");
exports.getTokLabels = sendCached("GET_TOKEN_LABELS");

exports.maybeParse = (code, options) => send("MAYBE_PARSE", {
  code,
  options
});

function sendCached(action) {
  let cache = null;
  return () => {
    if (!cache) cache = send(action, undefined);
    return cache;
  };
}

{
  send = require("./worker/index.cjs");
}