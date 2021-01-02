"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const subschema_1 = require("../../compile/subschema");
const util_1 = require("../../compile/util");
const validate_1 = require("../../compile/validate");
const error = {
    message: ({ params: { len } }) => codegen_1.str `should NOT have more than ${len} items`,
    params: ({ params: { len } }) => codegen_1._ `{limit: ${len}}`,
};
const def = {
    keyword: "additionalItems",
    type: "array",
    schemaType: ["boolean", "object"],
    before: "uniqueItems",
    error,
    code(cxt) {
        const { gen, schema, parentSchema, data, it } = cxt;
        const { items } = parentSchema;
        if (!Array.isArray(items)) {
            validate_1.checkStrictMode(it, '"additionalItems" is ignored when "items" is not an array of schemas');
            return;
        }
        it.items = true;
        const len = gen.const("len", codegen_1._ `${data}.length`);
        if (schema === false) {
            cxt.setParams({ len: items.length });
            cxt.pass(codegen_1._ `${len} <= ${items.length}`);
        }
        else if (typeof schema == "object" && !util_1.alwaysValidSchema(it, schema)) {
            const valid = gen.var("valid", codegen_1._ `${len} <= ${items.length}`); // TODO var
            gen.if(codegen_1.not(valid), () => validateItems(valid));
            cxt.ok(valid);
        }
        function validateItems(valid) {
            gen.forRange("i", items.length, len, (i) => {
                cxt.subschema({ keyword: "additionalItems", dataProp: i, dataPropType: subschema_1.Type.Num }, valid);
                if (!it.allErrors)
                    gen.if(codegen_1.not(valid), () => gen.break());
            });
        }
    },
};
exports.default = def;
//# sourceMappingURL=additionalItems.js.map