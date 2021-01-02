"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const subschema_1 = require("../../compile/subschema");
const util_1 = require("../../compile/util");
const validate_1 = require("../../compile/validate");
const def = {
    keyword: "items",
    type: "array",
    schemaType: ["object", "array", "boolean"],
    before: "uniqueItems",
    code(cxt) {
        const { gen, schema, parentSchema, data, it } = cxt;
        const len = gen.const("len", codegen_1._ `${data}.length`);
        if (Array.isArray(schema)) {
            if (it.opts.unevaluated && schema.length && it.items !== true) {
                it.items = util_1.mergeEvaluated.items(gen, schema.length, it.items);
            }
            validateTuple(schema);
        }
        else {
            it.items = true;
            if (!util_1.alwaysValidSchema(it, schema))
                validateArray();
        }
        function validateTuple(schArr) {
            if (it.opts.strictTuples && !fullTupleSchema(schema.length, parentSchema)) {
                const msg = `"items" is ${schArr.length}-tuple, but minItems or maxItems/additionalItems are not specified or different`;
                validate_1.checkStrictMode(it, msg, it.opts.strictTuples);
            }
            const valid = gen.name("valid");
            schArr.forEach((sch, i) => {
                if (util_1.alwaysValidSchema(it, sch))
                    return;
                gen.if(codegen_1._ `${len} > ${i}`, () => cxt.subschema({
                    keyword: "items",
                    schemaProp: i,
                    dataProp: i,
                    strictSchema: it.strictSchema,
                }, valid));
                cxt.ok(valid);
            });
        }
        function validateArray() {
            const valid = gen.name("valid");
            gen.forRange("i", 0, len, (i) => {
                cxt.subschema({
                    keyword: "items",
                    dataProp: i,
                    dataPropType: subschema_1.Type.Num,
                    strictSchema: it.strictSchema,
                }, valid);
                if (!it.allErrors)
                    gen.if(codegen_1.not(valid), () => gen.break());
            });
            cxt.ok(valid);
        }
    },
};
function fullTupleSchema(len, sch) {
    return len === sch.minItems && (len === sch.maxItems || sch.additionalItems === false);
}
exports.default = def;
//# sourceMappingURL=items.js.map