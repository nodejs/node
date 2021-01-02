"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const context_1 = require("../../compile/context");
const code_1 = require("../code");
const util_1 = require("../../compile/util");
const additionalProperties_1 = require("./additionalProperties");
const def = {
    keyword: "properties",
    type: "object",
    schemaType: "object",
    code(cxt) {
        const { gen, schema, parentSchema, data, it } = cxt;
        if (it.opts.removeAdditional === "all" && parentSchema.additionalProperties === undefined) {
            additionalProperties_1.default.code(new context_1.default(it, additionalProperties_1.default, "additionalProperties"));
        }
        const allProps = code_1.allSchemaProperties(schema);
        if (it.opts.unevaluated && allProps.length && it.props !== true) {
            it.props = util_1.mergeEvaluated.props(gen, util_1.toHash(allProps), it.props);
        }
        const properties = allProps.filter((p) => !util_1.alwaysValidSchema(it, schema[p]));
        if (properties.length === 0)
            return;
        const valid = gen.name("valid");
        for (const prop of properties) {
            if (hasDefault(prop)) {
                applyPropertySchema(prop);
            }
            else {
                gen.if(code_1.propertyInData(data, prop, it.opts.ownProperties));
                applyPropertySchema(prop);
                if (!it.allErrors)
                    gen.else().var(valid, true);
                gen.endIf();
            }
            cxt.ok(valid);
        }
        function hasDefault(prop) {
            return it.opts.useDefaults && !it.compositeRule && schema[prop].default !== undefined;
        }
        function applyPropertySchema(prop) {
            cxt.subschema({
                keyword: "properties",
                schemaProp: prop,
                dataProp: prop,
                strictSchema: it.strictSchema,
            }, valid);
        }
    },
};
exports.default = def;
//# sourceMappingURL=properties.js.map