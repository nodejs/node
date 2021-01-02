"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const validate_1 = require("../../compile/validate");
const def = {
    keyword: ["maxContains", "minContains"],
    type: "array",
    schemaType: "number",
    code({ keyword, parentSchema, it }) {
        if (parentSchema.contains === undefined) {
            validate_1.checkStrictMode(it, `"${keyword}" without "contains" is ignored`);
        }
    },
};
exports.default = def;
//# sourceMappingURL=limitContains.js.map