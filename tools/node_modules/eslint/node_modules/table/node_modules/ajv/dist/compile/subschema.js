"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.applySubschema = exports.Type = void 0;
const validate_1 = require("./validate");
const util_1 = require("./util");
const codegen_1 = require("./codegen");
var Type;
(function (Type) {
    Type[Type["Num"] = 0] = "Num";
    Type[Type["Str"] = 1] = "Str";
})(Type = exports.Type || (exports.Type = {}));
function applySubschema(it, appl, valid) {
    const subschema = getSubschema(it, appl);
    extendSubschemaData(subschema, it, appl);
    extendSubschemaMode(subschema, appl);
    const nextContext = { ...it, ...subschema, items: undefined, props: undefined };
    validate_1.subschemaCode(nextContext, valid);
    return nextContext;
}
exports.applySubschema = applySubschema;
function getSubschema(it, { keyword, schemaProp, schema, schemaPath, errSchemaPath, topSchemaRef }) {
    if (keyword !== undefined && schema !== undefined) {
        throw new Error('both "keyword" and "schema" passed, only one allowed');
    }
    if (keyword !== undefined) {
        const sch = it.schema[keyword];
        return schemaProp === undefined
            ? {
                schema: sch,
                schemaPath: codegen_1._ `${it.schemaPath}${codegen_1.getProperty(keyword)}`,
                errSchemaPath: `${it.errSchemaPath}/${keyword}`,
            }
            : {
                schema: sch[schemaProp],
                schemaPath: codegen_1._ `${it.schemaPath}${codegen_1.getProperty(keyword)}${codegen_1.getProperty(schemaProp)}`,
                errSchemaPath: `${it.errSchemaPath}/${keyword}/${util_1.escapeFragment(schemaProp)}`,
            };
    }
    if (schema !== undefined) {
        if (schemaPath === undefined || errSchemaPath === undefined || topSchemaRef === undefined) {
            throw new Error('"schemaPath", "errSchemaPath" and "topSchemaRef" are required with "schema"');
        }
        return {
            schema,
            schemaPath,
            topSchemaRef,
            errSchemaPath,
        };
    }
    throw new Error('either "keyword" or "schema" must be passed');
}
function extendSubschemaData(subschema, it, { dataProp, dataPropType: dpType, data, dataTypes, propertyName }) {
    if (data !== undefined && dataProp !== undefined) {
        throw new Error('both "data" and "dataProp" passed, only one allowed');
    }
    const { gen } = it;
    if (dataProp !== undefined) {
        const { errorPath, dataPathArr, opts } = it;
        const nextData = gen.let("data", codegen_1._ `${it.data}${codegen_1.getProperty(dataProp)}`, true);
        dataContextProps(nextData);
        subschema.errorPath = codegen_1.str `${errorPath}${getErrorPath(dataProp, dpType, opts.jsPropertySyntax)}`;
        subschema.parentDataProperty = codegen_1._ `${dataProp}`;
        subschema.dataPathArr = [...dataPathArr, subschema.parentDataProperty];
    }
    if (data !== undefined) {
        const nextData = data instanceof codegen_1.Name ? data : gen.let("data", data, true); // replaceable if used once?
        dataContextProps(nextData);
        if (propertyName !== undefined)
            subschema.propertyName = propertyName;
        // TODO something is possibly wrong here with not changing parentDataProperty and not appending dataPathArr
    }
    if (dataTypes)
        subschema.dataTypes = dataTypes;
    function dataContextProps(_nextData) {
        subschema.data = _nextData;
        subschema.dataLevel = it.dataLevel + 1;
        subschema.dataTypes = [];
        it.definedProperties = new Set();
        subschema.parentData = it.data;
        subschema.dataNames = [...it.dataNames, _nextData];
    }
}
function extendSubschemaMode(subschema, { jtdDiscriminator, jtdMetadata, compositeRule, createErrors, allErrors }) {
    if (compositeRule !== undefined)
        subschema.compositeRule = compositeRule;
    if (createErrors !== undefined)
        subschema.createErrors = createErrors;
    if (allErrors !== undefined)
        subschema.allErrors = allErrors;
    subschema.jtdDiscriminator = jtdDiscriminator; // not inherited
    subschema.jtdMetadata = jtdMetadata; // not inherited
}
function getErrorPath(dataProp, dataPropType, jsPropertySyntax) {
    // let path
    if (dataProp instanceof codegen_1.Name) {
        const isNumber = dataPropType === Type.Num;
        return jsPropertySyntax
            ? isNumber
                ? codegen_1._ `"[" + ${dataProp} + "]"`
                : codegen_1._ `"['" + ${dataProp} + "']"`
            : isNumber
                ? codegen_1._ `"/" + ${dataProp}`
                : codegen_1._ `"/" + ${dataProp}.replace(/~/g, "~0").replace(/\\//g, "~1")`; // TODO maybe use global escapePointer
    }
    return jsPropertySyntax ? codegen_1.getProperty(dataProp).toString() : "/" + util_1.escapeJsonPointer(dataProp);
}
//# sourceMappingURL=subschema.js.map