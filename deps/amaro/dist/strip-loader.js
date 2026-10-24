"use strict";
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);
var strip_loader_exports = {};
__export(strip_loader_exports, {
  load: () => load
});
module.exports = __toCommonJS(strip_loader_exports);
var import_node_url = require("node:url");
var import_errors = require("./errors.js");
var import_index = require("./index.js");
async function load(url, context, nextLoad) {
  const { format } = context;
  if (format?.endsWith("-typescript")) {
    try {
      const { source } = await nextLoad(url, {
        ...context,
        format
      });
      const { code } = (0, import_index.transformSync)(source.toString(), {
        mode: "strip-only",
        filename: (0, import_node_url.fileURLToPath)(url)
      });
      return {
        format: format.replace("-typescript", ""),
        // Source map is not necessary in strip-only mode. However, to map the source
        // file in debuggers to the original TypeScript source, add a sourceURL magic
        // comment to hint that it is a generated source.
        source: `${code}

//# sourceURL=${url}`
      };
    } catch (error) {
      if ((0, import_errors.isSwcError)(error)) {
        (0, import_errors.wrapAndReThrowSwcError)(error);
      }
      throw error;
    }
  }
  return nextLoad(url, context);
}
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  load
});
