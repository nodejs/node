"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const equal = require("fast-deep-equal");
const error = {
    message: "should be equal to constant",
    params: ({ schemaCode }) => codegen_1._ `{allowedValue: ${schemaCode}}`,
};
const def = {
    keyword: "const",
    $data: true,
    error,
    code(cxt) {
        const eql = cxt.gen.scopeValue("func", {
            ref: equal,
            code: codegen_1._ `require("ajv/dist/compile/equal")`,
        });
        // TODO optimize for scalar values in schema
        cxt.fail$data(codegen_1._ `!${eql}(${cxt.data}, ${cxt.schemaCode})`);
    },
};
exports.default = def;
//# sourceMappingURL=const.js.map