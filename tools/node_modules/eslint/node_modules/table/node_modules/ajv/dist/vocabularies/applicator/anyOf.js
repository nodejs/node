"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const util_1 = require("../../compile/util");
const def = {
    keyword: "anyOf",
    schemaType: "array",
    trackErrors: true,
    code(cxt) {
        const { gen, schema, it } = cxt;
        /* istanbul ignore if */
        if (!Array.isArray(schema))
            throw new Error("ajv implementation error");
        const alwaysValid = schema.some((sch) => util_1.alwaysValidSchema(it, sch));
        if (alwaysValid && !it.opts.unevaluated)
            return;
        const valid = gen.let("valid", false);
        const schValid = gen.name("_valid");
        gen.block(() => schema.forEach((_sch, i) => {
            const schCxt = cxt.subschema({
                keyword: "anyOf",
                schemaProp: i,
                compositeRule: true,
            }, schValid);
            gen.assign(valid, codegen_1._ `${valid} || ${schValid}`);
            const merged = cxt.mergeValidEvaluated(schCxt, schValid);
            // can short-circuit if `unevaluatedProperties/Items` not supported (opts.unevaluated !== true)
            // or if all properties and items were evaluated (it.props === true && it.items === true)
            if (!merged)
                gen.if(codegen_1.not(valid));
        }));
        cxt.result(valid, () => cxt.reset(), () => cxt.error(true));
    },
    error: {
        message: "should match some schema in anyOf",
    },
};
exports.default = def;
//# sourceMappingURL=anyOf.js.map