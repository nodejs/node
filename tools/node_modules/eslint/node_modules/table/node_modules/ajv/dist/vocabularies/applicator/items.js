"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.validateTuple = void 0;
const codegen_1 = require("../../compile/codegen");
const util_1 = require("../../compile/util");
const code_1 = require("../code");
const def = {
    keyword: "items",
    type: "array",
    schemaType: ["object", "array", "boolean"],
    before: "uniqueItems",
    code(cxt) {
        const { schema, it } = cxt;
        if (Array.isArray(schema))
            return validateTuple(cxt, "additionalItems", schema);
        it.items = true;
        if (util_1.alwaysValidSchema(it, schema))
            return;
        cxt.ok(code_1.validateArray(cxt));
    },
};
function validateTuple(cxt, extraItems, schArr = cxt.schema) {
    const { gen, parentSchema, data, keyword, it } = cxt;
    checkStrictTuple(parentSchema);
    if (it.opts.unevaluated && schArr.length && it.items !== true) {
        it.items = util_1.mergeEvaluated.items(gen, schArr.length, it.items);
    }
    const valid = gen.name("valid");
    const len = gen.const("len", codegen_1._ `${data}.length`);
    schArr.forEach((sch, i) => {
        if (util_1.alwaysValidSchema(it, sch))
            return;
        gen.if(codegen_1._ `${len} > ${i}`, () => cxt.subschema({
            keyword,
            schemaProp: i,
            dataProp: i,
        }, valid));
        cxt.ok(valid);
    });
    function checkStrictTuple(sch) {
        const l = schArr.length;
        const fullTuple = l === sch.minItems && (l === sch.maxItems || sch[extraItems] === false);
        if (it.opts.strictTuples && !fullTuple) {
            const msg = `"${keyword}" is ${l}-tuple, but minItems or maxItems/${extraItems} are not specified or different`;
            util_1.checkStrictMode(it, msg, it.opts.strictTuples);
        }
    }
}
exports.validateTuple = validateTuple;
exports.default = def;
//# sourceMappingURL=items.js.map