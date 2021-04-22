"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const codegen_1 = require("../../compile/codegen");
const util_1 = require("../../compile/util");
const code_1 = require("../code");
const additionalItems_1 = require("./additionalItems");
const error = {
    message: ({ params: { len } }) => codegen_1.str `must NOT have more than ${len} items`,
    params: ({ params: { len } }) => codegen_1._ `{limit: ${len}}`,
};
const def = {
    keyword: "items",
    type: "array",
    schemaType: ["object", "boolean"],
    before: "uniqueItems",
    error,
    code(cxt) {
        const { schema, parentSchema, it } = cxt;
        const { prefixItems } = parentSchema;
        it.items = true;
        if (util_1.alwaysValidSchema(it, schema))
            return;
        if (prefixItems)
            additionalItems_1.validateAdditionalItems(cxt, prefixItems);
        else
            cxt.ok(code_1.validateArray(cxt));
    },
};
exports.default = def;
//# sourceMappingURL=items2020.js.map