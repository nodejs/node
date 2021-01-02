"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const subschema_1 = require("../../compile/subschema");
const util_1 = require("../../compile/util");
const validate_1 = require("../../compile/validate");
const error = {
    message: ({ params: { min, max } }) => max === undefined
        ? codegen_1.str `should contain at least ${min} valid item(s)`
        : codegen_1.str `should contain at least ${min} and no more than ${max} valid item(s)`,
    params: ({ params: { min, max } }) => max === undefined ? codegen_1._ `{minContains: ${min}}` : codegen_1._ `{minContains: ${min}, maxContains: ${max}}`,
};
const def = {
    keyword: "contains",
    type: "array",
    schemaType: ["object", "boolean"],
    before: "uniqueItems",
    trackErrors: true,
    error,
    code(cxt) {
        const { gen, schema, parentSchema, data, it } = cxt;
        let min;
        let max;
        const { minContains, maxContains } = parentSchema;
        if (it.opts.next) {
            min = minContains === undefined ? 1 : minContains;
            max = maxContains;
        }
        else {
            min = 1;
        }
        const len = gen.const("len", codegen_1._ `${data}.length`);
        cxt.setParams({ min, max });
        if (max === undefined && min === 0) {
            validate_1.checkStrictMode(it, `"minContains" == 0 without "maxContains": "contains" keyword ignored`);
            return;
        }
        if (max !== undefined && min > max) {
            validate_1.checkStrictMode(it, `"minContains" > "maxContains" is always invalid`);
            cxt.fail();
            return;
        }
        if (util_1.alwaysValidSchema(it, schema)) {
            let cond = codegen_1._ `${len} >= ${min}`;
            if (max !== undefined)
                cond = codegen_1._ `${cond} && ${len} <= ${max}`;
            cxt.pass(cond);
            return;
        }
        it.items = true;
        const valid = gen.name("valid");
        if (max === undefined && min === 1) {
            validateItems(valid, () => gen.if(valid, () => gen.break()));
        }
        else {
            gen.let(valid, false);
            const schValid = gen.name("_valid");
            const count = gen.let("count", 0);
            validateItems(schValid, () => gen.if(schValid, () => checkLimits(count)));
        }
        cxt.result(valid, () => cxt.reset());
        function validateItems(_valid, block) {
            gen.forRange("i", 0, len, (i) => {
                cxt.subschema({
                    keyword: "contains",
                    dataProp: i,
                    dataPropType: subschema_1.Type.Num,
                    compositeRule: true,
                }, _valid);
                block();
            });
        }
        function checkLimits(count) {
            gen.code(codegen_1._ `${count}++`);
            if (max === undefined) {
                gen.if(codegen_1._ `${count} >= ${min}`, () => gen.assign(valid, true).break());
            }
            else {
                gen.if(codegen_1._ `${count} > ${max}`, () => gen.assign(valid, false).break());
                if (min === 1)
                    gen.assign(valid, true);
                else
                    gen.if(codegen_1._ `${count} >= ${min}`, () => gen.assign(valid, true));
            }
        }
    },
};
exports.default = def;
//# sourceMappingURL=contains.js.map