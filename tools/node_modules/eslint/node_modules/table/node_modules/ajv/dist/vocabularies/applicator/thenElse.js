"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const validate_1 = require("../../compile/validate");
const def = {
    keyword: ["then", "else"],
    schemaType: ["object", "boolean"],
    code({ keyword, parentSchema, it }) {
        if (parentSchema.if === undefined)
            validate_1.checkStrictMode(it, `"${keyword}" without "if" is ignored`);
    },
};
exports.default = def;
//# sourceMappingURL=thenElse.js.map