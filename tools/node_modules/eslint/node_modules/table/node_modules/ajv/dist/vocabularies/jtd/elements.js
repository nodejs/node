"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const util_1 = require("../../compile/util");
const code_1 = require("../code");
const codegen_1 = require("../../compile/codegen");
const metadata_1 = require("./metadata");
const nullable_1 = require("./nullable");
const def = {
    keyword: "elements",
    schemaType: "object",
    code(cxt) {
        metadata_1.checkMetadata(cxt);
        const { gen, data, schema, it } = cxt;
        if (util_1.alwaysValidSchema(it, schema))
            return;
        const [valid, cond] = nullable_1.checkNullable(cxt, codegen_1.nil);
        gen.if(codegen_1.and(cond, codegen_1._ `Array.isArray(${data})`), () => gen.assign(valid, code_1.validateArray(cxt)));
        cxt.pass(valid);
    },
};
exports.default = def;
//# sourceMappingURL=elements.js.map