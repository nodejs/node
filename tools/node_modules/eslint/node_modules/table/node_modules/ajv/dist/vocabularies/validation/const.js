"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const util_1 = require("../../compile/util");
const equal_1 = require("../../runtime/equal");
const error = {
    message: "must be equal to constant",
    params: ({ schemaCode }) => codegen_1._ `{allowedValue: ${schemaCode}}`,
};
const def = {
    keyword: "const",
    $data: true,
    error,
    code(cxt) {
        const { gen, data, schemaCode } = cxt;
        // TODO optimize for scalar values in schema
        cxt.fail$data(codegen_1._ `!${util_1.useFunc(gen, equal_1.default)}(${data}, ${schemaCode})`);
    },
};
exports.default = def;
//# sourceMappingURL=const.js.map