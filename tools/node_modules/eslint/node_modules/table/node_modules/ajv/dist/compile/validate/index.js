"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.checkStrictMode = exports.schemaCxtHasRules = exports.subschemaCode = exports.validateFunctionCode = void 0;
const boolSchema_1 = require("./boolSchema");
const dataType_1 = require("./dataType");
const iterate_1 = require("./iterate");
const codegen_1 = require("../codegen");
const names_1 = require("../names");
const resolve_1 = require("../resolve");
const util_1 = require("../util");
// schema compilation - generates validation function, subschemaCode (below) is used for subschemas
function validateFunctionCode(it) {
    if (isSchemaObj(it)) {
        checkKeywords(it);
        if (schemaCxtHasRules(it)) {
            topSchemaObjCode(it);
            return;
        }
    }
    validateFunction(it, () => boolSchema_1.topBoolOrEmptySchema(it));
}
exports.validateFunctionCode = validateFunctionCode;
function validateFunction({ gen, validateName, schema, schemaEnv, opts }, body) {
    if (opts.code.es5) {
        gen.func(validateName, codegen_1._ `${names_1.default.data}, ${names_1.default.valCxt}`, schemaEnv.$async, () => {
            gen.code(codegen_1._ `"use strict"; ${funcSourceUrl(schema, opts)}`);
            destructureValCxtES5(gen, opts);
            gen.code(body);
        });
    }
    else {
        gen.func(validateName, codegen_1._ `${names_1.default.data}, ${destructureValCxt(opts)}`, schemaEnv.$async, () => gen.code(funcSourceUrl(schema, opts)).code(body));
    }
}
function destructureValCxt(opts) {
    return codegen_1._ `{${names_1.default.dataPath}="", ${names_1.default.parentData}, ${names_1.default.parentDataProperty}, ${names_1.default.rootData}=${names_1.default.data}${opts.dynamicRef ? codegen_1._ `, ${names_1.default.dynamicAnchors}={}` : codegen_1.nil}}={}`;
}
function destructureValCxtES5(gen, opts) {
    gen.if(names_1.default.valCxt, () => {
        gen.var(names_1.default.dataPath, codegen_1._ `${names_1.default.valCxt}.${names_1.default.dataPath}`);
        gen.var(names_1.default.parentData, codegen_1._ `${names_1.default.valCxt}.${names_1.default.parentData}`);
        gen.var(names_1.default.parentDataProperty, codegen_1._ `${names_1.default.valCxt}.${names_1.default.parentDataProperty}`);
        gen.var(names_1.default.rootData, codegen_1._ `${names_1.default.valCxt}.${names_1.default.rootData}`);
        if (opts.dynamicRef)
            gen.var(names_1.default.dynamicAnchors, codegen_1._ `${names_1.default.valCxt}.${names_1.default.dynamicAnchors}`);
    }, () => {
        gen.var(names_1.default.dataPath, codegen_1._ `""`);
        gen.var(names_1.default.parentData, codegen_1._ `undefined`);
        gen.var(names_1.default.parentDataProperty, codegen_1._ `undefined`);
        gen.var(names_1.default.rootData, names_1.default.data);
        if (opts.dynamicRef)
            gen.var(names_1.default.dynamicAnchors, codegen_1._ `{}`);
    });
}
function topSchemaObjCode(it) {
    const { schema, opts, gen } = it;
    validateFunction(it, () => {
        if (opts.$comment && schema.$comment)
            commentKeyword(it);
        checkNoDefault(it);
        gen.let(names_1.default.vErrors, null);
        gen.let(names_1.default.errors, 0);
        if (opts.unevaluated)
            resetEvaluated(it);
        typeAndKeywords(it);
        returnResults(it);
    });
    return;
}
function resetEvaluated(it) {
    // TODO maybe some hook to execute it in the end to check whether props/items are Name, as in assignEvaluated
    const { gen, validateName } = it;
    it.evaluated = gen.const("evaluated", codegen_1._ `${validateName}.evaluated`);
    gen.if(codegen_1._ `${it.evaluated}.dynamicProps`, () => gen.assign(codegen_1._ `${it.evaluated}.props`, codegen_1._ `undefined`));
    gen.if(codegen_1._ `${it.evaluated}.dynamicItems`, () => gen.assign(codegen_1._ `${it.evaluated}.items`, codegen_1._ `undefined`));
}
function funcSourceUrl(schema, opts) {
    return typeof schema == "object" && schema.$id && (opts.code.source || opts.code.process)
        ? codegen_1._ `/*# sourceURL=${schema.$id} */`
        : codegen_1.nil;
}
// schema compilation - this function is used recursively to generate code for sub-schemas
function subschemaCode(it, valid) {
    if (isSchemaObj(it)) {
        checkKeywords(it);
        if (schemaCxtHasRules(it)) {
            subSchemaObjCode(it, valid);
            return;
        }
    }
    boolSchema_1.boolOrEmptySchema(it, valid);
}
exports.subschemaCode = subschemaCode;
function schemaCxtHasRules({ schema, self }) {
    if (typeof schema == "boolean")
        return !schema;
    for (const key in schema)
        if (self.RULES.all[key])
            return true;
    return false;
}
exports.schemaCxtHasRules = schemaCxtHasRules;
function isSchemaObj(it) {
    return typeof it.schema != "boolean";
}
function subSchemaObjCode(it, valid) {
    const { schema, gen, opts } = it;
    if (opts.$comment && schema.$comment)
        commentKeyword(it);
    updateContext(it);
    checkAsync(it);
    const errsCount = gen.const("_errs", names_1.default.errors);
    typeAndKeywords(it, errsCount);
    // TODO var
    gen.var(valid, codegen_1._ `${errsCount} === ${names_1.default.errors}`);
}
function checkKeywords(it) {
    util_1.checkUnknownRules(it);
    checkRefsAndKeywords(it);
}
function typeAndKeywords(it, errsCount) {
    if (it.opts.jtd)
        return iterate_1.schemaKeywords(it, [], false, errsCount);
    const types = dataType_1.getSchemaTypes(it.schema);
    const checkedTypes = dataType_1.coerceAndCheckDataType(it, types);
    iterate_1.schemaKeywords(it, types, !checkedTypes, errsCount);
}
function checkRefsAndKeywords(it) {
    const { schema, errSchemaPath, opts, self } = it;
    if (schema.$ref && opts.ignoreKeywordsWithRef && util_1.schemaHasRulesButRef(schema, self.RULES)) {
        self.logger.warn(`$ref: keywords ignored in schema at path "${errSchemaPath}"`);
    }
}
function checkNoDefault(it) {
    const { schema, opts } = it;
    if (schema.default !== undefined && opts.useDefaults && opts.strict) {
        checkStrictMode(it, "default is ignored in the schema root");
    }
}
function updateContext(it) {
    if (it.schema.$id)
        it.baseId = resolve_1.resolveUrl(it.baseId, it.schema.$id);
}
function checkAsync(it) {
    if (it.schema.$async && !it.schemaEnv.$async)
        throw new Error("async schema in sync schema");
}
function commentKeyword({ gen, schemaEnv, schema, errSchemaPath, opts }) {
    const msg = schema.$comment;
    if (opts.$comment === true) {
        gen.code(codegen_1._ `${names_1.default.self}.logger.log(${msg})`);
    }
    else if (typeof opts.$comment == "function") {
        const schemaPath = codegen_1.str `${errSchemaPath}/$comment`;
        const rootName = gen.scopeValue("root", { ref: schemaEnv.root });
        gen.code(codegen_1._ `${names_1.default.self}.opts.$comment(${msg}, ${schemaPath}, ${rootName}.schema)`);
    }
}
function returnResults(it) {
    const { gen, schemaEnv, validateName, ValidationError, opts } = it;
    if (schemaEnv.$async) {
        // TODO assign unevaluated
        gen.if(codegen_1._ `${names_1.default.errors} === 0`, () => gen.return(names_1.default.data), () => gen.throw(codegen_1._ `new ${ValidationError}(${names_1.default.vErrors})`));
    }
    else {
        gen.assign(codegen_1._ `${validateName}.errors`, names_1.default.vErrors);
        if (opts.unevaluated)
            assignEvaluated(it);
        gen.return(codegen_1._ `${names_1.default.errors} === 0`);
    }
}
function assignEvaluated({ gen, evaluated, props, items }) {
    if (props instanceof codegen_1.Name)
        gen.assign(codegen_1._ `${evaluated}.props`, props);
    if (items instanceof codegen_1.Name)
        gen.assign(codegen_1._ `${evaluated}.items`, items);
}
function checkStrictMode(it, msg, mode = it.opts.strict) {
    if (!mode)
        return;
    msg = `strict mode: ${msg}`;
    if (mode === true)
        throw new Error(msg);
    it.self.logger.warn(msg);
}
exports.checkStrictMode = checkStrictMode;
//# sourceMappingURL=index.js.map