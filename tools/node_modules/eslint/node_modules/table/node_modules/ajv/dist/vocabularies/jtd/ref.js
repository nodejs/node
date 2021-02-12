"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const compile_1 = require("../../compile");
const codegen_1 = require("../../compile/codegen");
const error_classes_1 = require("../../compile/error_classes");
const names_1 = require("../../compile/names");
const ref_1 = require("../core/ref");
const metadata_1 = require("./metadata");
const def = {
    keyword: "ref",
    schemaType: "string",
    code(cxt) {
        metadata_1.checkMetadata(cxt);
        const { gen, data, schema: ref, parentSchema, it } = cxt;
        const { schemaEnv: { root }, } = it;
        const valid = gen.name("valid");
        if (parentSchema.nullable) {
            gen.var(valid, codegen_1._ `${data} === null`);
            gen.if(codegen_1.not(valid), validateJtdRef);
        }
        else {
            gen.var(valid, false);
            validateJtdRef();
        }
        cxt.ok(valid);
        function validateJtdRef() {
            var _a;
            const refSchema = (_a = root.schema.definitions) === null || _a === void 0 ? void 0 : _a[ref];
            if (!refSchema)
                throw new error_classes_1.MissingRefError("", ref, `No definition ${ref}`);
            if (hasRef(refSchema) || !it.opts.inlineRefs)
                callValidate(refSchema);
            else
                inlineRefSchema(refSchema);
        }
        function callValidate(schema) {
            const sch = compile_1.compileSchema.call(it.self, new compile_1.SchemaEnv({ schema, root }));
            const v = ref_1.getValidate(cxt, sch);
            const errsCount = gen.const("_errs", names_1.default.errors);
            ref_1.callRef(cxt, v, sch, sch.$async);
            gen.assign(valid, codegen_1._ `${errsCount} === ${names_1.default.errors}`);
        }
        function inlineRefSchema(schema) {
            const schName = gen.scopeValue("schema", it.opts.code.source === true ? { ref: schema, code: codegen_1.stringify(schema) } : { ref: schema });
            cxt.subschema({
                schema,
                dataTypes: [],
                schemaPath: codegen_1.nil,
                topSchemaRef: schName,
                errSchemaPath: `/definitions/${ref}`,
            }, valid);
        }
        function hasRef(schema) {
            for (const key in schema) {
                let sch;
                if (key === "ref" || (typeof (sch = schema[key]) == "object" && hasRef(sch)))
                    return true;
            }
            return false;
        }
    },
};
exports.default = def;
//# sourceMappingURL=ref.js.map