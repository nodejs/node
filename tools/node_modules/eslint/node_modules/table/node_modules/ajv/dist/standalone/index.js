"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const scope_1 = require("../compile/codegen/scope");
const code_1 = require("../compile/codegen/code");
function standaloneCode(ajv, refsOrFunc) {
    if (!ajv.opts.code.source) {
        throw new Error("moduleCode: ajv instance must have code.source option");
    }
    const { _n } = ajv.scope.opts;
    return typeof refsOrFunc == "function"
        ? funcExportCode(refsOrFunc.source)
        : refsOrFunc !== undefined
            ? multiExportsCode(refsOrFunc, getValidate)
            : multiExportsCode(ajv.schemas, (sch) => sch.meta ? undefined : ajv.compile(sch.schema));
    function getValidate(id) {
        const v = ajv.getSchema(id);
        if (!v)
            throw new Error(`moduleCode: no schema with id ${id}`);
        return v;
    }
    function funcExportCode(source) {
        const usedValues = {};
        const n = source === null || source === void 0 ? void 0 : source.validateName;
        const vCode = validateCode(usedValues, source);
        return `"use strict";${_n}module.exports = ${n};${_n}module.exports.default = ${n};${_n}${vCode}`;
    }
    function multiExportsCode(schemas, getValidateFunc) {
        var _a;
        const usedValues = {};
        let code = code_1._ `"use strict";`;
        for (const name in schemas) {
            const v = getValidateFunc(schemas[name]);
            if (v) {
                const vCode = validateCode(usedValues, v.source);
                code = code_1._ `${code}${_n}exports${code_1.getProperty(name)} = ${(_a = v.source) === null || _a === void 0 ? void 0 : _a.validateName};${_n}${vCode}`;
            }
        }
        return `${code}`;
    }
    function validateCode(usedValues, s) {
        if (!s)
            throw new Error('moduleCode: function does not have "source" property');
        const { prefix } = s.validateName;
        const nameSet = (usedValues[prefix] = usedValues[prefix] || new Set());
        nameSet.add(s.validateName);
        const scopeCode = ajv.scope.scopeCode(s.scopeValues, usedValues, refValidateCode);
        const code = new code_1._Code(`${scopeCode}${_n}${s.validateCode}`);
        return s.evaluated ? code_1._ `${code}${s.validateName}.evaluated = ${s.evaluated};${_n}` : code;
        function refValidateCode(n) {
            var _a;
            const vRef = (_a = n.value) === null || _a === void 0 ? void 0 : _a.ref;
            if (n.prefix === "validate" && typeof vRef == "function") {
                const v = vRef;
                return validateCode(usedValues, v.source);
            }
            else if ((n.prefix === "root" || n.prefix === "wrapper") && typeof vRef == "object") {
                const { validate, validateName } = vRef;
                const vCode = validateCode(usedValues, validate === null || validate === void 0 ? void 0 : validate.source);
                const def = ajv.opts.code.es5 ? scope_1.varKinds.var : scope_1.varKinds.const;
                return code_1._ `${def} ${n} = {validate: ${validateName}};${_n}${vCode}`;
            }
            return undefined;
        }
    }
}
exports.default = standaloneCode;
//# sourceMappingURL=index.js.map