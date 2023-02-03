"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.declare = declare;
exports.declarePreset = void 0;
const apiPolyfills = {
  assertVersion: api => range => {
    throwVersionError(range, api.version);
  },
  targets: () => () => {
    return {};
  },
  assumption: () => () => {
    return undefined;
  }
};
function declare(builder) {
  return (api, options, dirname) => {
    var _clonedApi2;
    let clonedApi;
    for (const name of Object.keys(apiPolyfills)) {
      var _clonedApi;
      if (api[name]) continue;

      clonedApi = (_clonedApi = clonedApi) != null ? _clonedApi : copyApiObject(api);
      clonedApi[name] = apiPolyfills[name](clonedApi);
    }

    return builder((_clonedApi2 = clonedApi) != null ? _clonedApi2 : api, options || {}, dirname);
  };
}
const declarePreset = declare;
exports.declarePreset = declarePreset;
function copyApiObject(api) {
  let proto = null;
  if (typeof api.version === "string" && /^7\./.test(api.version)) {
    proto = Object.getPrototypeOf(api);
    if (proto && (!has(proto, "version") || !has(proto, "transform") || !has(proto, "template") || !has(proto, "types"))) {
      proto = null;
    }
  }
  return Object.assign({}, proto, api);
}
function has(obj, key) {
  return Object.prototype.hasOwnProperty.call(obj, key);
}
function throwVersionError(range, version) {
  if (typeof range === "number") {
    if (!Number.isInteger(range)) {
      throw new Error("Expected string or integer value.");
    }
    range = `^${range}.0.0-0`;
  }
  if (typeof range !== "string") {
    throw new Error("Expected string or integer value.");
  }
  const limit = Error.stackTraceLimit;
  if (typeof limit === "number" && limit < 25) {
    Error.stackTraceLimit = 25;
  }
  let err;
  if (version.slice(0, 2) === "7.") {
    err = new Error(`Requires Babel "^7.0.0-beta.41", but was loaded with "${version}". ` + `You'll need to update your @babel/core version.`);
  } else {
    err = new Error(`Requires Babel "${range}", but was loaded with "${version}". ` + `If you are sure you have a compatible version of @babel/core, ` + `it is likely that something in your build process is loading the ` + `wrong version. Inspect the stack trace of this error to look for ` + `the first entry that doesn't mention "@babel/core" or "babel-core" ` + `to see what is calling Babel.`);
  }
  if (typeof limit === "number") {
    Error.stackTraceLimit = limit;
  }
  throw Object.assign(err, {
    code: "BABEL_VERSION_UNSUPPORTED",
    version,
    range
  });
}

//# sourceMappingURL=index.js.map
