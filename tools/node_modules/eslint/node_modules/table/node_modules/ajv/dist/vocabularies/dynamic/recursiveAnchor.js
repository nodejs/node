"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const dynamicAnchor_1 = require("./dynamicAnchor");
const validate_1 = require("../../compile/validate");
const def = {
    keyword: "$recursiveAnchor",
    schemaType: "boolean",
    code(cxt) {
        if (cxt.schema)
            dynamicAnchor_1.dynamicAnchor(cxt, "");
        else
            validate_1.checkStrictMode(cxt.it, "$recursiveAnchor: false is ignored");
    },
};
exports.default = def;
//# sourceMappingURL=recursiveAnchor.js.map