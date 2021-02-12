"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const timestamp_1 = require("../../compile/timestamp");
const metadata_1 = require("./metadata");
const intRange = {
    int8: [-128, 127],
    uint8: [0, 255],
    int16: [-32768, 32767],
    uint16: [0, 65535],
    int32: [-2147483648, 2147483647],
    uint32: [0, 4294967295],
};
const def = {
    keyword: "type",
    schemaType: "string",
    code(cxt) {
        metadata_1.checkMetadata(cxt);
        const { gen, data, schema, parentSchema } = cxt;
        let cond;
        switch (schema) {
            case "boolean":
            case "string":
                cond = codegen_1._ `typeof ${data} == ${schema}`;
                break;
            case "timestamp": {
                const vts = gen.scopeValue("func", {
                    ref: timestamp_1.default,
                    code: codegen_1._ `require("ajv/dist/compile/timestamp").default`,
                });
                cond = codegen_1._ `${data} instanceof Date || (typeof ${data} == "string" && ${vts}(${data}))`;
                break;
            }
            case "float32":
            case "float64":
                cond = codegen_1._ `typeof ${data} == "number"`;
                break;
            default: {
                const [min, max] = intRange[schema];
                cond = codegen_1._ `typeof ${data} == "number" && isFinite(${data}) && ${data} >= ${min} && ${data} <= ${max} && !(${data} % 1)`;
            }
        }
        cxt.pass(parentSchema.nullable ? codegen_1.or(codegen_1._ `${data} === null`, cond) : cond);
    },
};
exports.default = def;
//# sourceMappingURL=type.js.map