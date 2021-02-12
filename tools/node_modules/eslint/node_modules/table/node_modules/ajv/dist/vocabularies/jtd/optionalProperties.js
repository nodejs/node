"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const properties_1 = require("./properties");
const def = {
    keyword: "optionalProperties",
    schemaType: "object",
    code(cxt) {
        if (cxt.parentSchema.properties)
            return;
        properties_1.validateProperties(cxt);
    },
};
exports.default = def;
//# sourceMappingURL=optionalProperties.js.map