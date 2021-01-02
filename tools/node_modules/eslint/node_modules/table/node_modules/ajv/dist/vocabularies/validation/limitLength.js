"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const ucs2length_1 = require("../../compile/ucs2length");
const error = {
    message({ keyword, schemaCode }) {
        const comp = keyword === "maxLength" ? "more" : "fewer";
        return codegen_1.str `should NOT have ${comp} than ${schemaCode} characters`;
    },
    params: ({ schemaCode }) => codegen_1._ `{limit: ${schemaCode}}`,
};
const def = {
    keyword: ["maxLength", "minLength"],
    type: "string",
    schemaType: "number",
    $data: true,
    error,
    code(cxt) {
        const { keyword, data, schemaCode, it } = cxt;
        const op = keyword === "maxLength" ? codegen_1.operators.GT : codegen_1.operators.LT;
        let len;
        if (it.opts.unicode === false) {
            len = codegen_1._ `${data}.length`;
        }
        else {
            const u2l = cxt.gen.scopeValue("func", {
                ref: ucs2length_1.default,
                code: codegen_1._ `require("ajv/dist/compile/ucs2length").default`,
            });
            len = codegen_1._ `${u2l}(${data})`;
        }
        cxt.fail$data(codegen_1._ `${len} ${op} ${schemaCode}`);
    },
};
exports.default = def;
//# sourceMappingURL=limitLength.js.map