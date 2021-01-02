"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.usePattern = exports.callValidateCode = exports.schemaProperties = exports.allSchemaProperties = exports.noPropertyInData = exports.propertyInData = exports.reportMissingProp = exports.checkMissingProp = exports.checkReportMissingProp = void 0;
const codegen_1 = require("../compile/codegen");
const util_1 = require("../compile/util");
const names_1 = require("../compile/names");
function checkReportMissingProp(cxt, prop) {
    const { gen, data, it } = cxt;
    gen.if(noPropertyInData(data, prop, it.opts.ownProperties), () => {
        cxt.setParams({ missingProperty: codegen_1._ `${prop}` }, true);
        cxt.error();
    });
}
exports.checkReportMissingProp = checkReportMissingProp;
function checkMissingProp({ data, it: { opts } }, properties, missing) {
    return codegen_1.or(...properties.map((prop) => codegen_1._ `${noPropertyInData(data, prop, opts.ownProperties)} && (${missing} = ${prop})`));
}
exports.checkMissingProp = checkMissingProp;
function reportMissingProp(cxt, missing) {
    cxt.setParams({ missingProperty: missing }, true);
    cxt.error();
}
exports.reportMissingProp = reportMissingProp;
function isOwnProperty(data, property) {
    return codegen_1._ `Object.prototype.hasOwnProperty.call(${data}, ${property})`;
}
function propertyInData(data, property, ownProperties) {
    const cond = codegen_1._ `${data}${codegen_1.getProperty(property)} !== undefined`;
    return ownProperties ? codegen_1._ `${cond} && ${isOwnProperty(data, property)}` : cond;
}
exports.propertyInData = propertyInData;
function noPropertyInData(data, property, ownProperties) {
    const cond = codegen_1._ `${data}${codegen_1.getProperty(property)} === undefined`;
    return ownProperties ? codegen_1._ `${cond} || !${isOwnProperty(data, property)}` : cond;
}
exports.noPropertyInData = noPropertyInData;
function allSchemaProperties(schemaMap) {
    return schemaMap ? Object.keys(schemaMap).filter((p) => p !== "__proto__") : [];
}
exports.allSchemaProperties = allSchemaProperties;
function schemaProperties(it, schemaMap) {
    return allSchemaProperties(schemaMap).filter((p) => !util_1.alwaysValidSchema(it, schemaMap[p]));
}
exports.schemaProperties = schemaProperties;
function callValidateCode({ schemaCode, data, it: { gen, topSchemaRef, schemaPath, errorPath }, it }, func, context, passSchema) {
    const dataAndSchema = passSchema ? codegen_1._ `${schemaCode}, ${data}, ${topSchemaRef}${schemaPath}` : data;
    const valCxt = [
        [names_1.default.dataPath, codegen_1.strConcat(names_1.default.dataPath, errorPath)],
        [names_1.default.parentData, it.parentData],
        [names_1.default.parentDataProperty, it.parentDataProperty],
        [names_1.default.rootData, names_1.default.rootData],
    ];
    if (it.opts.dynamicRef)
        valCxt.push([names_1.default.dynamicAnchors, names_1.default.dynamicAnchors]);
    const args = codegen_1._ `${dataAndSchema}, ${gen.object(...valCxt)}`;
    return context !== codegen_1.nil ? codegen_1._ `${func}.call(${context}, ${args})` : codegen_1._ `${func}(${args})`;
}
exports.callValidateCode = callValidateCode;
function usePattern(gen, pattern) {
    return gen.scopeValue("pattern", {
        key: pattern,
        ref: new RegExp(pattern, "u"),
        code: codegen_1._ `new RegExp(${pattern}, "u")`,
    });
}
exports.usePattern = usePattern;
//# sourceMappingURL=code.js.map