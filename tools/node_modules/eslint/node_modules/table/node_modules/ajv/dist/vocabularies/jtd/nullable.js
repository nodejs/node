"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.checkNullableObject = exports.checkNullable = void 0;
const codegen_1 = require("../../compile/codegen");
function checkNullable({ gen, data, parentSchema }, cond) {
    const valid = gen.name("valid");
    if (parentSchema.nullable) {
        gen.let(valid, codegen_1._ `${data} === null`);
        cond = codegen_1.not(valid);
    }
    else {
        gen.let(valid, false);
    }
    return [valid, cond];
}
exports.checkNullable = checkNullable;
function checkNullableObject(cxt, cond) {
    const [valid, cond_] = checkNullable(cxt, cond);
    return [valid, codegen_1._ `${cond_} && typeof ${cxt.data} == "object" && !Array.isArray(${cxt.data})`];
}
exports.checkNullableObject = checkNullableObject;
//# sourceMappingURL=nullable.js.map