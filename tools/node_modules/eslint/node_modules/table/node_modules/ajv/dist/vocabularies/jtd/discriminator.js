"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const metadata_1 = require("./metadata");
const nullable_1 = require("./nullable");
const def = {
    keyword: "discriminator",
    schemaType: "string",
    implements: ["mapping"],
    code(cxt) {
        metadata_1.checkMetadata(cxt);
        const { gen, data, schema, parentSchema } = cxt;
        const [valid, cond] = nullable_1.checkNullableObject(cxt, data);
        gen.if(cond, () => {
            const tag = gen.const("tag", codegen_1._ `${data}${codegen_1.getProperty(schema)}`);
            gen.if(codegen_1._ `typeof ${tag} == "string"`, () => {
                gen.if(false);
                for (const tagValue in parentSchema.mapping) {
                    gen.elseIf(codegen_1._ `${tag} === ${tagValue}`);
                    gen.assign(valid, applyTagSchema(tagValue));
                }
                gen.endIf();
            });
        });
        cxt.pass(valid);
        function applyTagSchema(schemaProp) {
            const _valid = gen.name("valid");
            cxt.subschema({
                keyword: "mapping",
                schemaProp,
                jtdDiscriminator: schema,
            }, _valid);
            return _valid;
        }
    },
};
exports.default = def;
//# sourceMappingURL=discriminator.js.map