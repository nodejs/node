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
var transform_loader_exports = {};
__export(transform_loader_exports, {
  load: () => load
});
module.exports = __toCommonJS(transform_loader_exports);
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
      const { code, map } = (0, import_index.transformSync)(source.toString(), {
        mode: "transform",
        sourceMap: true,
        filename: (0, import_node_url.fileURLToPath)(url)
      });
      let output = code;
      if (map) {
        const base64SourceMap = Buffer.from(map).toString("base64");
        output = `${code}

//# sourceMappingURL=data:application/json;base64,${base64SourceMap}`;
      }
      return {
        format: format.replace("-typescript", ""),
        source: `${output}

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
