"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const code_1 = require("../code");
const codegen_1 = require("../../compile/codegen");
const names_1 = require("../../compile/names");
const subschema_1 = require("../../compile/subschema");
const util_1 = require("../../compile/util");
const error = {
    message: "should NOT have additional properties",
    params: ({ params }) => codegen_1._ `{additionalProperty: ${params.additionalProperty}}`,
};
const def = {
    keyword: "additionalProperties",
    type: ["object"],
    schemaType: ["boolean", "object"],
    allowUndefined: true,
    trackErrors: true,
    error,
    code(cxt) {
        const { gen, schema, parentSchema, data, errsCount, it } = cxt;
        /* istanbul ignore if */
        if (!errsCount)
            throw new Error("ajv implementation error");
        const { allErrors, opts } = it;
        it.props = true;
        if (opts.removeAdditional !== "all" && util_1.alwaysValidSchema(it, schema))
            return;
        const props = code_1.allSchemaProperties(parentSchema.properties);
        const patProps = code_1.allSchemaProperties(parentSchema.patternProperties);
        checkAdditionalProperties();
        cxt.ok(codegen_1._ `${errsCount} === ${names_1.default.errors}`);
        function checkAdditionalProperties() {
            gen.forIn("key", data, (key) => {
                if (!props.length && !patProps.length)
                    additionalPropertyCode(key);
                else
                    gen.if(isAdditional(key), () => additionalPropertyCode(key));
            });
        }
        function isAdditional(key) {
            let definedProp;
            if (props.length > 8) {
                // TODO maybe an option instead of hard-coded 8?
                const propsSchema = util_1.schemaRefOrVal(it, parentSchema.properties, "properties");
                definedProp = codegen_1._ `${propsSchema}.hasOwnProperty(${key})`;
            }
            else if (props.length) {
                definedProp = codegen_1.or(...props.map((p) => codegen_1._ `${key} === ${p}`));
            }
            else {
                definedProp = codegen_1.nil;
            }
            if (patProps.length) {
                definedProp = codegen_1.or(definedProp, ...patProps.map((p) => codegen_1._ `${code_1.usePattern(gen, p)}.test(${key})`));
            }
            return codegen_1._ `!(${definedProp})`;
        }
        function deleteAdditional(key) {
            gen.code(codegen_1._ `delete ${data}[${key}]`);
        }
        function additionalPropertyCode(key) {
            if (opts.removeAdditional === "all" || (opts.removeAdditional && schema === false)) {
                deleteAdditional(key);
                return;
            }
            if (schema === false) {
                cxt.setParams({ additionalProperty: key });
                cxt.error();
                if (!allErrors)
                    gen.break();
                return;
            }
            if (typeof schema == "object" && !util_1.alwaysValidSchema(it, schema)) {
                const valid = gen.name("valid");
                if (opts.removeAdditional === "failing") {
                    applyAdditionalSchema(key, valid, false);
                    gen.if(codegen_1.not(valid), () => {
                        cxt.reset();
                        deleteAdditional(key);
                    });
                }
                else {
                    applyAdditionalSchema(key, valid);
                    if (!allErrors)
                        gen.if(codegen_1.not(valid), () => gen.break());
                }
            }
        }
        function applyAdditionalSchema(key, valid, errors) {
            const subschema = {
                keyword: "additionalProperties",
                dataProp: key,
                dataPropType: subschema_1.Type.Str,
                strictSchema: it.strictSchema,
            };
            if (errors === false) {
                Object.assign(subschema, {
                    compositeRule: true,
                    createErrors: false,
                    allErrors: false,
                });
            }
            cxt.subschema(subschema, valid);
        }
    },
};
exports.default = def;
//# sourceMappingURL=additionalProperties.js.map