"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const code_1 = require("../code");
const codegen_1 = require("../../compile/codegen");
const error = {
    message: ({ schemaCode }) => codegen_1.str `should match pattern "${schemaCode}"`,
    params: ({ schemaCode }) => codegen_1._ `{pattern: ${schemaCode}}`,
};
const def = {
    keyword: "pattern",
    type: "string",
    schemaType: "string",
    $data: true,
    error,
    code(cxt) {
        const { gen, data, $data, schema, schemaCode } = cxt;
        const regExp = $data ? codegen_1._ `(new RegExp(${schemaCode}, "u"))` : code_1.usePattern(gen, schema); // TODO regexp should be wrapped in try/catch
        cxt.fail$data(codegen_1._ `!${regExp}.test(${data})`);
    },
};
exports.default = def;
//# sourceMappingURL=pattern.js.map