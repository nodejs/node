"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const subschema_1 = require("../../compile/subschema");
const util_1 = require("../../compile/util");
const codegen_1 = require("../../compile/codegen");
const metadata_1 = require("./metadata");
const nullable_1 = require("./nullable");
const def = {
    keyword: "values",
    schemaType: "object",
    code(cxt) {
        metadata_1.checkMetadata(cxt);
        const { gen, data, schema, it } = cxt;
        if (util_1.alwaysValidSchema(it, schema))
            return;
        const [valid, cond] = nullable_1.checkNullableObject(cxt, data);
        gen.if(cond, () => gen.assign(valid, validateMap()));
        cxt.pass(valid);
        function validateMap() {
            const _valid = gen.name("valid");
            if (it.allErrors) {
                const validMap = gen.let("valid", true);
                validateValues(() => gen.assign(validMap, false));
                return validMap;
            }
            gen.var(_valid, true);
            validateValues(() => gen.break());
            return _valid;
            function validateValues(notValid) {
                gen.forIn("key", data, (key) => {
                    cxt.subschema({
                        keyword: "values",
                        dataProp: key,
                        dataPropType: subschema_1.Type.Str,
                    }, _valid);
                    gen.if(codegen_1.not(_valid), notValid);
                });
            }
        }
    },
};
exports.default = def;
//# sourceMappingURL=values.js.map