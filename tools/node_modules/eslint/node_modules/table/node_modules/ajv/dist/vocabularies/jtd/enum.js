"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const metadata_1 = require("./metadata");
const def = {
    keyword: "enum",
    schemaType: "array",
    code(cxt) {
        metadata_1.checkMetadata(cxt);
        const { gen, data, schema, schemaValue, parentSchema, it } = cxt;
        if (schema.length === 0)
            throw new Error("enum must have non-empty array");
        if (schema.length !== new Set(schema).size)
            throw new Error("enum items must be unique");
        let valid;
        if (schema.length >= it.opts.loopEnum) {
            if (parentSchema.nullable) {
                valid = gen.let("valid", codegen_1._ `${data} === null`);
                gen.if(codegen_1.not(valid), loopEnum);
            }
            else {
                valid = gen.let("valid", false);
                loopEnum();
            }
        }
        else {
            /* istanbul ignore if */
            if (!Array.isArray(schema))
                throw new Error("ajv implementation error");
            valid = codegen_1.or(...schema.map((value) => codegen_1._ `${data} === ${value}`));
            if (parentSchema.nullable)
                valid = codegen_1.or(codegen_1._ `${data} === null`, valid);
        }
        cxt.pass(valid);
        function loopEnum() {
            gen.forOf("v", schemaValue, (v) => gen.if(codegen_1._ `${valid} = ${data} === ${v}`, () => gen.break()));
        }
    },
};
exports.default = def;
//# sourceMappingURL=enum.js.map