"use strict";
exports["config.json"] = validate43;
const schema13 = {
    "$id": "config.json",
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "border": {
            "$ref": "shared.json#/definitions/borders"
        },
        "header": {
            "type": "object",
            "properties": {
                "content": {
                    "type": "string"
                },
                "alignment": {
                    "$ref": "shared.json#/definitions/alignment"
                },
                "wrapWord": {
                    "type": "boolean"
                },
                "truncate": {
                    "type": "integer"
                },
                "paddingLeft": {
                    "type": "integer"
                },
                "paddingRight": {
                    "type": "integer"
                }
            },
            "required": ["content"],
            "additionalProperties": false
        },
        "columns": {
            "$ref": "shared.json#/definitions/columns"
        },
        "columnDefault": {
            "$ref": "shared.json#/definitions/column"
        },
        "drawVerticalLine": {
            "typeof": "function"
        },
        "drawHorizontalLine": {
            "typeof": "function"
        },
        "singleLine": {
            "typeof": "boolean"
        }
    },
    "additionalProperties": false
};
const schema15 = {
    "type": "object",
    "properties": {
        "topBody": {
            "$ref": "#/definitions/border"
        },
        "topJoin": {
            "$ref": "#/definitions/border"
        },
        "topLeft": {
            "$ref": "#/definitions/border"
        },
        "topRight": {
            "$ref": "#/definitions/border"
        },
        "bottomBody": {
            "$ref": "#/definitions/border"
        },
        "bottomJoin": {
            "$ref": "#/definitions/border"
        },
        "bottomLeft": {
            "$ref": "#/definitions/border"
        },
        "bottomRight": {
            "$ref": "#/definitions/border"
        },
        "bodyLeft": {
            "$ref": "#/definitions/border"
        },
        "bodyRight": {
            "$ref": "#/definitions/border"
        },
        "bodyJoin": {
            "$ref": "#/definitions/border"
        },
        "headerJoin": {
            "$ref": "#/definitions/border"
        },
        "joinBody": {
            "$ref": "#/definitions/border"
        },
        "joinLeft": {
            "$ref": "#/definitions/border"
        },
        "joinRight": {
            "$ref": "#/definitions/border"
        },
        "joinJoin": {
            "$ref": "#/definitions/border"
        }
    },
    "additionalProperties": false
};
const func8 = Object.prototype.hasOwnProperty;
const schema16 = {
    "type": "string"
};
function validate46(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    if (typeof data !== "string") {
        const err0 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "string"
            },
            message: "must be string"
        };
        if (vErrors === null) {
            vErrors = [err0];
        }
        else {
            vErrors.push(err0);
        }
        errors++;
    }
    validate46.errors = vErrors;
    return errors === 0;
}
function validate45(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(func8.call(schema15.properties, key0))) {
                const err0 = {
                    instancePath,
                    schemaPath: "#/additionalProperties",
                    keyword: "additionalProperties",
                    params: {
                        additionalProperty: key0
                    },
                    message: "must NOT have additional properties"
                };
                if (vErrors === null) {
                    vErrors = [err0];
                }
                else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.topBody !== undefined) {
            if (!(validate46(data.topBody, {
                instancePath: instancePath + "/topBody",
                parentData: data,
                parentDataProperty: "topBody",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.topJoin !== undefined) {
            if (!(validate46(data.topJoin, {
                instancePath: instancePath + "/topJoin",
                parentData: data,
                parentDataProperty: "topJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.topLeft !== undefined) {
            if (!(validate46(data.topLeft, {
                instancePath: instancePath + "/topLeft",
                parentData: data,
                parentDataProperty: "topLeft",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.topRight !== undefined) {
            if (!(validate46(data.topRight, {
                instancePath: instancePath + "/topRight",
                parentData: data,
                parentDataProperty: "topRight",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bottomBody !== undefined) {
            if (!(validate46(data.bottomBody, {
                instancePath: instancePath + "/bottomBody",
                parentData: data,
                parentDataProperty: "bottomBody",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bottomJoin !== undefined) {
            if (!(validate46(data.bottomJoin, {
                instancePath: instancePath + "/bottomJoin",
                parentData: data,
                parentDataProperty: "bottomJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bottomLeft !== undefined) {
            if (!(validate46(data.bottomLeft, {
                instancePath: instancePath + "/bottomLeft",
                parentData: data,
                parentDataProperty: "bottomLeft",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bottomRight !== undefined) {
            if (!(validate46(data.bottomRight, {
                instancePath: instancePath + "/bottomRight",
                parentData: data,
                parentDataProperty: "bottomRight",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bodyLeft !== undefined) {
            if (!(validate46(data.bodyLeft, {
                instancePath: instancePath + "/bodyLeft",
                parentData: data,
                parentDataProperty: "bodyLeft",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bodyRight !== undefined) {
            if (!(validate46(data.bodyRight, {
                instancePath: instancePath + "/bodyRight",
                parentData: data,
                parentDataProperty: "bodyRight",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bodyJoin !== undefined) {
            if (!(validate46(data.bodyJoin, {
                instancePath: instancePath + "/bodyJoin",
                parentData: data,
                parentDataProperty: "bodyJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.headerJoin !== undefined) {
            if (!(validate46(data.headerJoin, {
                instancePath: instancePath + "/headerJoin",
                parentData: data,
                parentDataProperty: "headerJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.joinBody !== undefined) {
            if (!(validate46(data.joinBody, {
                instancePath: instancePath + "/joinBody",
                parentData: data,
                parentDataProperty: "joinBody",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.joinLeft !== undefined) {
            if (!(validate46(data.joinLeft, {
                instancePath: instancePath + "/joinLeft",
                parentData: data,
                parentDataProperty: "joinLeft",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.joinRight !== undefined) {
            if (!(validate46(data.joinRight, {
                instancePath: instancePath + "/joinRight",
                parentData: data,
                parentDataProperty: "joinRight",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.joinJoin !== undefined) {
            if (!(validate46(data.joinJoin, {
                instancePath: instancePath + "/joinJoin",
                parentData: data,
                parentDataProperty: "joinJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
    }
    else {
        const err1 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err1];
        }
        else {
            vErrors.push(err1);
        }
        errors++;
    }
    validate45.errors = vErrors;
    return errors === 0;
}
const schema17 = {
    "type": "string",
    "enum": ["left", "right", "center", "justify"]
};
const func0 = require("ajv/dist/runtime/equal").default;
function validate64(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    if (typeof data !== "string") {
        const err0 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "string"
            },
            message: "must be string"
        };
        if (vErrors === null) {
            vErrors = [err0];
        }
        else {
            vErrors.push(err0);
        }
        errors++;
    }
    if (!((((data === "left") || (data === "right")) || (data === "center")) || (data === "justify"))) {
        const err1 = {
            instancePath,
            schemaPath: "#/enum",
            keyword: "enum",
            params: {
                allowedValues: schema17.enum
            },
            message: "must be equal to one of the allowed values"
        };
        if (vErrors === null) {
            vErrors = [err1];
        }
        else {
            vErrors.push(err1);
        }
        errors++;
    }
    validate64.errors = vErrors;
    return errors === 0;
}
const schema18 = {
    "oneOf": [{
            "type": "object",
            "patternProperties": {
                "^[0-9]+$": {
                    "$ref": "#/definitions/column"
                }
            },
            "additionalProperties": false
        }, {
            "type": "array",
            "items": {
                "$ref": "#/definitions/column"
            }
        }]
};
const pattern0 = new RegExp("^[0-9]+$", "u");
const schema19 = {
    "type": "object",
    "properties": {
        "alignment": {
            "$ref": "#/definitions/alignment"
        },
        "verticalAlignment": {
            "type": "string",
            "enum": ["top", "middle", "bottom"]
        },
        "width": {
            "type": "integer",
            "minimum": 1
        },
        "wrapWord": {
            "type": "boolean"
        },
        "truncate": {
            "type": "integer"
        },
        "paddingLeft": {
            "type": "integer"
        },
        "paddingRight": {
            "type": "integer"
        }
    },
    "additionalProperties": false
};
function validate68(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    if (typeof data !== "string") {
        const err0 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "string"
            },
            message: "must be string"
        };
        if (vErrors === null) {
            vErrors = [err0];
        }
        else {
            vErrors.push(err0);
        }
        errors++;
    }
    if (!((((data === "left") || (data === "right")) || (data === "center")) || (data === "justify"))) {
        const err1 = {
            instancePath,
            schemaPath: "#/enum",
            keyword: "enum",
            params: {
                allowedValues: schema17.enum
            },
            message: "must be equal to one of the allowed values"
        };
        if (vErrors === null) {
            vErrors = [err1];
        }
        else {
            vErrors.push(err1);
        }
        errors++;
    }
    validate68.errors = vErrors;
    return errors === 0;
}
function validate67(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(((((((key0 === "alignment") || (key0 === "verticalAlignment")) || (key0 === "width")) || (key0 === "wrapWord")) || (key0 === "truncate")) || (key0 === "paddingLeft")) || (key0 === "paddingRight"))) {
                const err0 = {
                    instancePath,
                    schemaPath: "#/additionalProperties",
                    keyword: "additionalProperties",
                    params: {
                        additionalProperty: key0
                    },
                    message: "must NOT have additional properties"
                };
                if (vErrors === null) {
                    vErrors = [err0];
                }
                else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.alignment !== undefined) {
            if (!(validate68(data.alignment, {
                instancePath: instancePath + "/alignment",
                parentData: data,
                parentDataProperty: "alignment",
                rootData
            }))) {
                vErrors = vErrors === null ? validate68.errors : vErrors.concat(validate68.errors);
                errors = vErrors.length;
            }
        }
        if (data.verticalAlignment !== undefined) {
            let data1 = data.verticalAlignment;
            if (typeof data1 !== "string") {
                const err1 = {
                    instancePath: instancePath + "/verticalAlignment",
                    schemaPath: "#/properties/verticalAlignment/type",
                    keyword: "type",
                    params: {
                        type: "string"
                    },
                    message: "must be string"
                };
                if (vErrors === null) {
                    vErrors = [err1];
                }
                else {
                    vErrors.push(err1);
                }
                errors++;
            }
            if (!(((data1 === "top") || (data1 === "middle")) || (data1 === "bottom"))) {
                const err2 = {
                    instancePath: instancePath + "/verticalAlignment",
                    schemaPath: "#/properties/verticalAlignment/enum",
                    keyword: "enum",
                    params: {
                        allowedValues: schema19.properties.verticalAlignment.enum
                    },
                    message: "must be equal to one of the allowed values"
                };
                if (vErrors === null) {
                    vErrors = [err2];
                }
                else {
                    vErrors.push(err2);
                }
                errors++;
            }
        }
        if (data.width !== undefined) {
            let data2 = data.width;
            if (!(((typeof data2 == "number") && (!(data2 % 1) && !isNaN(data2))) && (isFinite(data2)))) {
                const err3 = {
                    instancePath: instancePath + "/width",
                    schemaPath: "#/properties/width/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err3];
                }
                else {
                    vErrors.push(err3);
                }
                errors++;
            }
            if ((typeof data2 == "number") && (isFinite(data2))) {
                if (data2 < 1 || isNaN(data2)) {
                    const err4 = {
                        instancePath: instancePath + "/width",
                        schemaPath: "#/properties/width/minimum",
                        keyword: "minimum",
                        params: {
                            comparison: ">=",
                            limit: 1
                        },
                        message: "must be >= 1"
                    };
                    if (vErrors === null) {
                        vErrors = [err4];
                    }
                    else {
                        vErrors.push(err4);
                    }
                    errors++;
                }
            }
        }
        if (data.wrapWord !== undefined) {
            if (typeof data.wrapWord !== "boolean") {
                const err5 = {
                    instancePath: instancePath + "/wrapWord",
                    schemaPath: "#/properties/wrapWord/type",
                    keyword: "type",
                    params: {
                        type: "boolean"
                    },
                    message: "must be boolean"
                };
                if (vErrors === null) {
                    vErrors = [err5];
                }
                else {
                    vErrors.push(err5);
                }
                errors++;
            }
        }
        if (data.truncate !== undefined) {
            let data4 = data.truncate;
            if (!(((typeof data4 == "number") && (!(data4 % 1) && !isNaN(data4))) && (isFinite(data4)))) {
                const err6 = {
                    instancePath: instancePath + "/truncate",
                    schemaPath: "#/properties/truncate/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err6];
                }
                else {
                    vErrors.push(err6);
                }
                errors++;
            }
        }
        if (data.paddingLeft !== undefined) {
            let data5 = data.paddingLeft;
            if (!(((typeof data5 == "number") && (!(data5 % 1) && !isNaN(data5))) && (isFinite(data5)))) {
                const err7 = {
                    instancePath: instancePath + "/paddingLeft",
                    schemaPath: "#/properties/paddingLeft/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err7];
                }
                else {
                    vErrors.push(err7);
                }
                errors++;
            }
        }
        if (data.paddingRight !== undefined) {
            let data6 = data.paddingRight;
            if (!(((typeof data6 == "number") && (!(data6 % 1) && !isNaN(data6))) && (isFinite(data6)))) {
                const err8 = {
                    instancePath: instancePath + "/paddingRight",
                    schemaPath: "#/properties/paddingRight/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err8];
                }
                else {
                    vErrors.push(err8);
                }
                errors++;
            }
        }
    }
    else {
        const err9 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err9];
        }
        else {
            vErrors.push(err9);
        }
        errors++;
    }
    validate67.errors = vErrors;
    return errors === 0;
}
function validate66(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    const _errs0 = errors;
    let valid0 = false;
    let passing0 = null;
    const _errs1 = errors;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(pattern0.test(key0))) {
                const err0 = {
                    instancePath,
                    schemaPath: "#/oneOf/0/additionalProperties",
                    keyword: "additionalProperties",
                    params: {
                        additionalProperty: key0
                    },
                    message: "must NOT have additional properties"
                };
                if (vErrors === null) {
                    vErrors = [err0];
                }
                else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        for (const key1 in data) {
            if (pattern0.test(key1)) {
                if (!(validate67(data[key1], {
                    instancePath: instancePath + "/" + key1.replace(/~/g, "~0").replace(/\//g, "~1"),
                    parentData: data,
                    parentDataProperty: key1,
                    rootData
                }))) {
                    vErrors = vErrors === null ? validate67.errors : vErrors.concat(validate67.errors);
                    errors = vErrors.length;
                }
            }
        }
    }
    else {
        const err1 = {
            instancePath,
            schemaPath: "#/oneOf/0/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err1];
        }
        else {
            vErrors.push(err1);
        }
        errors++;
    }
    var _valid0 = _errs1 === errors;
    if (_valid0) {
        valid0 = true;
        passing0 = 0;
    }
    const _errs5 = errors;
    if (Array.isArray(data)) {
        const len0 = data.length;
        for (let i0 = 0; i0 < len0; i0++) {
            if (!(validate67(data[i0], {
                instancePath: instancePath + "/" + i0,
                parentData: data,
                parentDataProperty: i0,
                rootData
            }))) {
                vErrors = vErrors === null ? validate67.errors : vErrors.concat(validate67.errors);
                errors = vErrors.length;
            }
        }
    }
    else {
        const err2 = {
            instancePath,
            schemaPath: "#/oneOf/1/type",
            keyword: "type",
            params: {
                type: "array"
            },
            message: "must be array"
        };
        if (vErrors === null) {
            vErrors = [err2];
        }
        else {
            vErrors.push(err2);
        }
        errors++;
    }
    var _valid0 = _errs5 === errors;
    if (_valid0 && valid0) {
        valid0 = false;
        passing0 = [passing0, 1];
    }
    else {
        if (_valid0) {
            valid0 = true;
            passing0 = 1;
        }
    }
    if (!valid0) {
        const err3 = {
            instancePath,
            schemaPath: "#/oneOf",
            keyword: "oneOf",
            params: {
                passingSchemas: passing0
            },
            message: "must match exactly one schema in oneOf"
        };
        if (vErrors === null) {
            vErrors = [err3];
        }
        else {
            vErrors.push(err3);
        }
        errors++;
    }
    else {
        errors = _errs0;
        if (vErrors !== null) {
            if (_errs0) {
                vErrors.length = _errs0;
            }
            else {
                vErrors = null;
            }
        }
    }
    validate66.errors = vErrors;
    return errors === 0;
}
function validate73(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(((((((key0 === "alignment") || (key0 === "verticalAlignment")) || (key0 === "width")) || (key0 === "wrapWord")) || (key0 === "truncate")) || (key0 === "paddingLeft")) || (key0 === "paddingRight"))) {
                const err0 = {
                    instancePath,
                    schemaPath: "#/additionalProperties",
                    keyword: "additionalProperties",
                    params: {
                        additionalProperty: key0
                    },
                    message: "must NOT have additional properties"
                };
                if (vErrors === null) {
                    vErrors = [err0];
                }
                else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.alignment !== undefined) {
            if (!(validate68(data.alignment, {
                instancePath: instancePath + "/alignment",
                parentData: data,
                parentDataProperty: "alignment",
                rootData
            }))) {
                vErrors = vErrors === null ? validate68.errors : vErrors.concat(validate68.errors);
                errors = vErrors.length;
            }
        }
        if (data.verticalAlignment !== undefined) {
            let data1 = data.verticalAlignment;
            if (typeof data1 !== "string") {
                const err1 = {
                    instancePath: instancePath + "/verticalAlignment",
                    schemaPath: "#/properties/verticalAlignment/type",
                    keyword: "type",
                    params: {
                        type: "string"
                    },
                    message: "must be string"
                };
                if (vErrors === null) {
                    vErrors = [err1];
                }
                else {
                    vErrors.push(err1);
                }
                errors++;
            }
            if (!(((data1 === "top") || (data1 === "middle")) || (data1 === "bottom"))) {
                const err2 = {
                    instancePath: instancePath + "/verticalAlignment",
                    schemaPath: "#/properties/verticalAlignment/enum",
                    keyword: "enum",
                    params: {
                        allowedValues: schema19.properties.verticalAlignment.enum
                    },
                    message: "must be equal to one of the allowed values"
                };
                if (vErrors === null) {
                    vErrors = [err2];
                }
                else {
                    vErrors.push(err2);
                }
                errors++;
            }
        }
        if (data.width !== undefined) {
            let data2 = data.width;
            if (!(((typeof data2 == "number") && (!(data2 % 1) && !isNaN(data2))) && (isFinite(data2)))) {
                const err3 = {
                    instancePath: instancePath + "/width",
                    schemaPath: "#/properties/width/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err3];
                }
                else {
                    vErrors.push(err3);
                }
                errors++;
            }
            if ((typeof data2 == "number") && (isFinite(data2))) {
                if (data2 < 1 || isNaN(data2)) {
                    const err4 = {
                        instancePath: instancePath + "/width",
                        schemaPath: "#/properties/width/minimum",
                        keyword: "minimum",
                        params: {
                            comparison: ">=",
                            limit: 1
                        },
                        message: "must be >= 1"
                    };
                    if (vErrors === null) {
                        vErrors = [err4];
                    }
                    else {
                        vErrors.push(err4);
                    }
                    errors++;
                }
            }
        }
        if (data.wrapWord !== undefined) {
            if (typeof data.wrapWord !== "boolean") {
                const err5 = {
                    instancePath: instancePath + "/wrapWord",
                    schemaPath: "#/properties/wrapWord/type",
                    keyword: "type",
                    params: {
                        type: "boolean"
                    },
                    message: "must be boolean"
                };
                if (vErrors === null) {
                    vErrors = [err5];
                }
                else {
                    vErrors.push(err5);
                }
                errors++;
            }
        }
        if (data.truncate !== undefined) {
            let data4 = data.truncate;
            if (!(((typeof data4 == "number") && (!(data4 % 1) && !isNaN(data4))) && (isFinite(data4)))) {
                const err6 = {
                    instancePath: instancePath + "/truncate",
                    schemaPath: "#/properties/truncate/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err6];
                }
                else {
                    vErrors.push(err6);
                }
                errors++;
            }
        }
        if (data.paddingLeft !== undefined) {
            let data5 = data.paddingLeft;
            if (!(((typeof data5 == "number") && (!(data5 % 1) && !isNaN(data5))) && (isFinite(data5)))) {
                const err7 = {
                    instancePath: instancePath + "/paddingLeft",
                    schemaPath: "#/properties/paddingLeft/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err7];
                }
                else {
                    vErrors.push(err7);
                }
                errors++;
            }
        }
        if (data.paddingRight !== undefined) {
            let data6 = data.paddingRight;
            if (!(((typeof data6 == "number") && (!(data6 % 1) && !isNaN(data6))) && (isFinite(data6)))) {
                const err8 = {
                    instancePath: instancePath + "/paddingRight",
                    schemaPath: "#/properties/paddingRight/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err8];
                }
                else {
                    vErrors.push(err8);
                }
                errors++;
            }
        }
    }
    else {
        const err9 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err9];
        }
        else {
            vErrors.push(err9);
        }
        errors++;
    }
    validate73.errors = vErrors;
    return errors === 0;
}
function validate43(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    /*# sourceURL="config.json" */ ;
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(((((((key0 === "border") || (key0 === "header")) || (key0 === "columns")) || (key0 === "columnDefault")) || (key0 === "drawVerticalLine")) || (key0 === "drawHorizontalLine")) || (key0 === "singleLine"))) {
                const err0 = {
                    instancePath,
                    schemaPath: "#/additionalProperties",
                    keyword: "additionalProperties",
                    params: {
                        additionalProperty: key0
                    },
                    message: "must NOT have additional properties"
                };
                if (vErrors === null) {
                    vErrors = [err0];
                }
                else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.border !== undefined) {
            if (!(validate45(data.border, {
                instancePath: instancePath + "/border",
                parentData: data,
                parentDataProperty: "border",
                rootData
            }))) {
                vErrors = vErrors === null ? validate45.errors : vErrors.concat(validate45.errors);
                errors = vErrors.length;
            }
        }
        if (data.header !== undefined) {
            let data1 = data.header;
            if (data1 && typeof data1 == "object" && !Array.isArray(data1)) {
                if (data1.content === undefined) {
                    const err1 = {
                        instancePath: instancePath + "/header",
                        schemaPath: "#/properties/header/required",
                        keyword: "required",
                        params: {
                            missingProperty: "content"
                        },
                        message: "must have required property '" + "content" + "'"
                    };
                    if (vErrors === null) {
                        vErrors = [err1];
                    }
                    else {
                        vErrors.push(err1);
                    }
                    errors++;
                }
                for (const key1 in data1) {
                    if (!((((((key1 === "content") || (key1 === "alignment")) || (key1 === "wrapWord")) || (key1 === "truncate")) || (key1 === "paddingLeft")) || (key1 === "paddingRight"))) {
                        const err2 = {
                            instancePath: instancePath + "/header",
                            schemaPath: "#/properties/header/additionalProperties",
                            keyword: "additionalProperties",
                            params: {
                                additionalProperty: key1
                            },
                            message: "must NOT have additional properties"
                        };
                        if (vErrors === null) {
                            vErrors = [err2];
                        }
                        else {
                            vErrors.push(err2);
                        }
                        errors++;
                    }
                }
                if (data1.content !== undefined) {
                    if (typeof data1.content !== "string") {
                        const err3 = {
                            instancePath: instancePath + "/header/content",
                            schemaPath: "#/properties/header/properties/content/type",
                            keyword: "type",
                            params: {
                                type: "string"
                            },
                            message: "must be string"
                        };
                        if (vErrors === null) {
                            vErrors = [err3];
                        }
                        else {
                            vErrors.push(err3);
                        }
                        errors++;
                    }
                }
                if (data1.alignment !== undefined) {
                    if (!(validate64(data1.alignment, {
                        instancePath: instancePath + "/header/alignment",
                        parentData: data1,
                        parentDataProperty: "alignment",
                        rootData
                    }))) {
                        vErrors = vErrors === null ? validate64.errors : vErrors.concat(validate64.errors);
                        errors = vErrors.length;
                    }
                }
                if (data1.wrapWord !== undefined) {
                    if (typeof data1.wrapWord !== "boolean") {
                        const err4 = {
                            instancePath: instancePath + "/header/wrapWord",
                            schemaPath: "#/properties/header/properties/wrapWord/type",
                            keyword: "type",
                            params: {
                                type: "boolean"
                            },
                            message: "must be boolean"
                        };
                        if (vErrors === null) {
                            vErrors = [err4];
                        }
                        else {
                            vErrors.push(err4);
                        }
                        errors++;
                    }
                }
                if (data1.truncate !== undefined) {
                    let data5 = data1.truncate;
                    if (!(((typeof data5 == "number") && (!(data5 % 1) && !isNaN(data5))) && (isFinite(data5)))) {
                        const err5 = {
                            instancePath: instancePath + "/header/truncate",
                            schemaPath: "#/properties/header/properties/truncate/type",
                            keyword: "type",
                            params: {
                                type: "integer"
                            },
                            message: "must be integer"
                        };
                        if (vErrors === null) {
                            vErrors = [err5];
                        }
                        else {
                            vErrors.push(err5);
                        }
                        errors++;
                    }
                }
                if (data1.paddingLeft !== undefined) {
                    let data6 = data1.paddingLeft;
                    if (!(((typeof data6 == "number") && (!(data6 % 1) && !isNaN(data6))) && (isFinite(data6)))) {
                        const err6 = {
                            instancePath: instancePath + "/header/paddingLeft",
                            schemaPath: "#/properties/header/properties/paddingLeft/type",
                            keyword: "type",
                            params: {
                                type: "integer"
                            },
                            message: "must be integer"
                        };
                        if (vErrors === null) {
                            vErrors = [err6];
                        }
                        else {
                            vErrors.push(err6);
                        }
                        errors++;
                    }
                }
                if (data1.paddingRight !== undefined) {
                    let data7 = data1.paddingRight;
                    if (!(((typeof data7 == "number") && (!(data7 % 1) && !isNaN(data7))) && (isFinite(data7)))) {
                        const err7 = {
                            instancePath: instancePath + "/header/paddingRight",
                            schemaPath: "#/properties/header/properties/paddingRight/type",
                            keyword: "type",
                            params: {
                                type: "integer"
                            },
                            message: "must be integer"
                        };
                        if (vErrors === null) {
                            vErrors = [err7];
                        }
                        else {
                            vErrors.push(err7);
                        }
                        errors++;
                    }
                }
            }
            else {
                const err8 = {
                    instancePath: instancePath + "/header",
                    schemaPath: "#/properties/header/type",
                    keyword: "type",
                    params: {
                        type: "object"
                    },
                    message: "must be object"
                };
                if (vErrors === null) {
                    vErrors = [err8];
                }
                else {
                    vErrors.push(err8);
                }
                errors++;
            }
        }
        if (data.columns !== undefined) {
            if (!(validate66(data.columns, {
                instancePath: instancePath + "/columns",
                parentData: data,
                parentDataProperty: "columns",
                rootData
            }))) {
                vErrors = vErrors === null ? validate66.errors : vErrors.concat(validate66.errors);
                errors = vErrors.length;
            }
        }
        if (data.columnDefault !== undefined) {
            if (!(validate73(data.columnDefault, {
                instancePath: instancePath + "/columnDefault",
                parentData: data,
                parentDataProperty: "columnDefault",
                rootData
            }))) {
                vErrors = vErrors === null ? validate73.errors : vErrors.concat(validate73.errors);
                errors = vErrors.length;
            }
        }
        if (data.drawVerticalLine !== undefined) {
            if (typeof data.drawVerticalLine != "function") {
                const err9 = {
                    instancePath: instancePath + "/drawVerticalLine",
                    schemaPath: "#/properties/drawVerticalLine/typeof",
                    keyword: "typeof",
                    params: {},
                    message: "should pass \"typeof\" keyword validation"
                };
                if (vErrors === null) {
                    vErrors = [err9];
                }
                else {
                    vErrors.push(err9);
                }
                errors++;
            }
        }
        if (data.drawHorizontalLine !== undefined) {
            if (typeof data.drawHorizontalLine != "function") {
                const err10 = {
                    instancePath: instancePath + "/drawHorizontalLine",
                    schemaPath: "#/properties/drawHorizontalLine/typeof",
                    keyword: "typeof",
                    params: {},
                    message: "should pass \"typeof\" keyword validation"
                };
                if (vErrors === null) {
                    vErrors = [err10];
                }
                else {
                    vErrors.push(err10);
                }
                errors++;
            }
        }
        if (data.singleLine !== undefined) {
            if (typeof data.singleLine != "boolean") {
                const err11 = {
                    instancePath: instancePath + "/singleLine",
                    schemaPath: "#/properties/singleLine/typeof",
                    keyword: "typeof",
                    params: {},
                    message: "should pass \"typeof\" keyword validation"
                };
                if (vErrors === null) {
                    vErrors = [err11];
                }
                else {
                    vErrors.push(err11);
                }
                errors++;
            }
        }
    }
    else {
        const err12 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err12];
        }
        else {
            vErrors.push(err12);
        }
        errors++;
    }
    validate43.errors = vErrors;
    return errors === 0;
}
exports["streamConfig.json"] = validate76;
const schema22 = {
    "$id": "streamConfig.json",
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        "border": {
            "$ref": "shared.json#/definitions/borders"
        },
        "columns": {
            "$ref": "shared.json#/definitions/columns"
        },
        "columnDefault": {
            "$ref": "shared.json#/definitions/column"
        },
        "columnCount": {
            "type": "integer",
            "minimum": 1
        },
        "drawVerticalLine": {
            "typeof": "function"
        }
    },
    "required": ["columnDefault", "columnCount"],
    "additionalProperties": false
};
function validate77(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(func8.call(schema15.properties, key0))) {
                const err0 = {
                    instancePath,
                    schemaPath: "#/additionalProperties",
                    keyword: "additionalProperties",
                    params: {
                        additionalProperty: key0
                    },
                    message: "must NOT have additional properties"
                };
                if (vErrors === null) {
                    vErrors = [err0];
                }
                else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.topBody !== undefined) {
            if (!(validate46(data.topBody, {
                instancePath: instancePath + "/topBody",
                parentData: data,
                parentDataProperty: "topBody",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.topJoin !== undefined) {
            if (!(validate46(data.topJoin, {
                instancePath: instancePath + "/topJoin",
                parentData: data,
                parentDataProperty: "topJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.topLeft !== undefined) {
            if (!(validate46(data.topLeft, {
                instancePath: instancePath + "/topLeft",
                parentData: data,
                parentDataProperty: "topLeft",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.topRight !== undefined) {
            if (!(validate46(data.topRight, {
                instancePath: instancePath + "/topRight",
                parentData: data,
                parentDataProperty: "topRight",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bottomBody !== undefined) {
            if (!(validate46(data.bottomBody, {
                instancePath: instancePath + "/bottomBody",
                parentData: data,
                parentDataProperty: "bottomBody",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bottomJoin !== undefined) {
            if (!(validate46(data.bottomJoin, {
                instancePath: instancePath + "/bottomJoin",
                parentData: data,
                parentDataProperty: "bottomJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bottomLeft !== undefined) {
            if (!(validate46(data.bottomLeft, {
                instancePath: instancePath + "/bottomLeft",
                parentData: data,
                parentDataProperty: "bottomLeft",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bottomRight !== undefined) {
            if (!(validate46(data.bottomRight, {
                instancePath: instancePath + "/bottomRight",
                parentData: data,
                parentDataProperty: "bottomRight",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bodyLeft !== undefined) {
            if (!(validate46(data.bodyLeft, {
                instancePath: instancePath + "/bodyLeft",
                parentData: data,
                parentDataProperty: "bodyLeft",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bodyRight !== undefined) {
            if (!(validate46(data.bodyRight, {
                instancePath: instancePath + "/bodyRight",
                parentData: data,
                parentDataProperty: "bodyRight",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.bodyJoin !== undefined) {
            if (!(validate46(data.bodyJoin, {
                instancePath: instancePath + "/bodyJoin",
                parentData: data,
                parentDataProperty: "bodyJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.headerJoin !== undefined) {
            if (!(validate46(data.headerJoin, {
                instancePath: instancePath + "/headerJoin",
                parentData: data,
                parentDataProperty: "headerJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.joinBody !== undefined) {
            if (!(validate46(data.joinBody, {
                instancePath: instancePath + "/joinBody",
                parentData: data,
                parentDataProperty: "joinBody",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.joinLeft !== undefined) {
            if (!(validate46(data.joinLeft, {
                instancePath: instancePath + "/joinLeft",
                parentData: data,
                parentDataProperty: "joinLeft",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.joinRight !== undefined) {
            if (!(validate46(data.joinRight, {
                instancePath: instancePath + "/joinRight",
                parentData: data,
                parentDataProperty: "joinRight",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
        if (data.joinJoin !== undefined) {
            if (!(validate46(data.joinJoin, {
                instancePath: instancePath + "/joinJoin",
                parentData: data,
                parentDataProperty: "joinJoin",
                rootData
            }))) {
                vErrors = vErrors === null ? validate46.errors : vErrors.concat(validate46.errors);
                errors = vErrors.length;
            }
        }
    }
    else {
        const err1 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err1];
        }
        else {
            vErrors.push(err1);
        }
        errors++;
    }
    validate77.errors = vErrors;
    return errors === 0;
}
function validate95(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    const _errs0 = errors;
    let valid0 = false;
    let passing0 = null;
    const _errs1 = errors;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(pattern0.test(key0))) {
                const err0 = {
                    instancePath,
                    schemaPath: "#/oneOf/0/additionalProperties",
                    keyword: "additionalProperties",
                    params: {
                        additionalProperty: key0
                    },
                    message: "must NOT have additional properties"
                };
                if (vErrors === null) {
                    vErrors = [err0];
                }
                else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        for (const key1 in data) {
            if (pattern0.test(key1)) {
                if (!(validate67(data[key1], {
                    instancePath: instancePath + "/" + key1.replace(/~/g, "~0").replace(/\//g, "~1"),
                    parentData: data,
                    parentDataProperty: key1,
                    rootData
                }))) {
                    vErrors = vErrors === null ? validate67.errors : vErrors.concat(validate67.errors);
                    errors = vErrors.length;
                }
            }
        }
    }
    else {
        const err1 = {
            instancePath,
            schemaPath: "#/oneOf/0/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err1];
        }
        else {
            vErrors.push(err1);
        }
        errors++;
    }
    var _valid0 = _errs1 === errors;
    if (_valid0) {
        valid0 = true;
        passing0 = 0;
    }
    const _errs5 = errors;
    if (Array.isArray(data)) {
        const len0 = data.length;
        for (let i0 = 0; i0 < len0; i0++) {
            if (!(validate67(data[i0], {
                instancePath: instancePath + "/" + i0,
                parentData: data,
                parentDataProperty: i0,
                rootData
            }))) {
                vErrors = vErrors === null ? validate67.errors : vErrors.concat(validate67.errors);
                errors = vErrors.length;
            }
        }
    }
    else {
        const err2 = {
            instancePath,
            schemaPath: "#/oneOf/1/type",
            keyword: "type",
            params: {
                type: "array"
            },
            message: "must be array"
        };
        if (vErrors === null) {
            vErrors = [err2];
        }
        else {
            vErrors.push(err2);
        }
        errors++;
    }
    var _valid0 = _errs5 === errors;
    if (_valid0 && valid0) {
        valid0 = false;
        passing0 = [passing0, 1];
    }
    else {
        if (_valid0) {
            valid0 = true;
            passing0 = 1;
        }
    }
    if (!valid0) {
        const err3 = {
            instancePath,
            schemaPath: "#/oneOf",
            keyword: "oneOf",
            params: {
                passingSchemas: passing0
            },
            message: "must match exactly one schema in oneOf"
        };
        if (vErrors === null) {
            vErrors = [err3];
        }
        else {
            vErrors.push(err3);
        }
        errors++;
    }
    else {
        errors = _errs0;
        if (vErrors !== null) {
            if (_errs0) {
                vErrors.length = _errs0;
            }
            else {
                vErrors = null;
            }
        }
    }
    validate95.errors = vErrors;
    return errors === 0;
}
function validate99(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(((((((key0 === "alignment") || (key0 === "verticalAlignment")) || (key0 === "width")) || (key0 === "wrapWord")) || (key0 === "truncate")) || (key0 === "paddingLeft")) || (key0 === "paddingRight"))) {
                const err0 = {
                    instancePath,
                    schemaPath: "#/additionalProperties",
                    keyword: "additionalProperties",
                    params: {
                        additionalProperty: key0
                    },
                    message: "must NOT have additional properties"
                };
                if (vErrors === null) {
                    vErrors = [err0];
                }
                else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.alignment !== undefined) {
            if (!(validate68(data.alignment, {
                instancePath: instancePath + "/alignment",
                parentData: data,
                parentDataProperty: "alignment",
                rootData
            }))) {
                vErrors = vErrors === null ? validate68.errors : vErrors.concat(validate68.errors);
                errors = vErrors.length;
            }
        }
        if (data.verticalAlignment !== undefined) {
            let data1 = data.verticalAlignment;
            if (typeof data1 !== "string") {
                const err1 = {
                    instancePath: instancePath + "/verticalAlignment",
                    schemaPath: "#/properties/verticalAlignment/type",
                    keyword: "type",
                    params: {
                        type: "string"
                    },
                    message: "must be string"
                };
                if (vErrors === null) {
                    vErrors = [err1];
                }
                else {
                    vErrors.push(err1);
                }
                errors++;
            }
            if (!(((data1 === "top") || (data1 === "middle")) || (data1 === "bottom"))) {
                const err2 = {
                    instancePath: instancePath + "/verticalAlignment",
                    schemaPath: "#/properties/verticalAlignment/enum",
                    keyword: "enum",
                    params: {
                        allowedValues: schema19.properties.verticalAlignment.enum
                    },
                    message: "must be equal to one of the allowed values"
                };
                if (vErrors === null) {
                    vErrors = [err2];
                }
                else {
                    vErrors.push(err2);
                }
                errors++;
            }
        }
        if (data.width !== undefined) {
            let data2 = data.width;
            if (!(((typeof data2 == "number") && (!(data2 % 1) && !isNaN(data2))) && (isFinite(data2)))) {
                const err3 = {
                    instancePath: instancePath + "/width",
                    schemaPath: "#/properties/width/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err3];
                }
                else {
                    vErrors.push(err3);
                }
                errors++;
            }
            if ((typeof data2 == "number") && (isFinite(data2))) {
                if (data2 < 1 || isNaN(data2)) {
                    const err4 = {
                        instancePath: instancePath + "/width",
                        schemaPath: "#/properties/width/minimum",
                        keyword: "minimum",
                        params: {
                            comparison: ">=",
                            limit: 1
                        },
                        message: "must be >= 1"
                    };
                    if (vErrors === null) {
                        vErrors = [err4];
                    }
                    else {
                        vErrors.push(err4);
                    }
                    errors++;
                }
            }
        }
        if (data.wrapWord !== undefined) {
            if (typeof data.wrapWord !== "boolean") {
                const err5 = {
                    instancePath: instancePath + "/wrapWord",
                    schemaPath: "#/properties/wrapWord/type",
                    keyword: "type",
                    params: {
                        type: "boolean"
                    },
                    message: "must be boolean"
                };
                if (vErrors === null) {
                    vErrors = [err5];
                }
                else {
                    vErrors.push(err5);
                }
                errors++;
            }
        }
        if (data.truncate !== undefined) {
            let data4 = data.truncate;
            if (!(((typeof data4 == "number") && (!(data4 % 1) && !isNaN(data4))) && (isFinite(data4)))) {
                const err6 = {
                    instancePath: instancePath + "/truncate",
                    schemaPath: "#/properties/truncate/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err6];
                }
                else {
                    vErrors.push(err6);
                }
                errors++;
            }
        }
        if (data.paddingLeft !== undefined) {
            let data5 = data.paddingLeft;
            if (!(((typeof data5 == "number") && (!(data5 % 1) && !isNaN(data5))) && (isFinite(data5)))) {
                const err7 = {
                    instancePath: instancePath + "/paddingLeft",
                    schemaPath: "#/properties/paddingLeft/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err7];
                }
                else {
                    vErrors.push(err7);
                }
                errors++;
            }
        }
        if (data.paddingRight !== undefined) {
            let data6 = data.paddingRight;
            if (!(((typeof data6 == "number") && (!(data6 % 1) && !isNaN(data6))) && (isFinite(data6)))) {
                const err8 = {
                    instancePath: instancePath + "/paddingRight",
                    schemaPath: "#/properties/paddingRight/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err8];
                }
                else {
                    vErrors.push(err8);
                }
                errors++;
            }
        }
    }
    else {
        const err9 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err9];
        }
        else {
            vErrors.push(err9);
        }
        errors++;
    }
    validate99.errors = vErrors;
    return errors === 0;
}
function validate76(data, { instancePath = "", parentData, parentDataProperty, rootData = data } = {}) {
    /*# sourceURL="streamConfig.json" */ ;
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        if (data.columnDefault === undefined) {
            const err0 = {
                instancePath,
                schemaPath: "#/required",
                keyword: "required",
                params: {
                    missingProperty: "columnDefault"
                },
                message: "must have required property '" + "columnDefault" + "'"
            };
            if (vErrors === null) {
                vErrors = [err0];
            }
            else {
                vErrors.push(err0);
            }
            errors++;
        }
        if (data.columnCount === undefined) {
            const err1 = {
                instancePath,
                schemaPath: "#/required",
                keyword: "required",
                params: {
                    missingProperty: "columnCount"
                },
                message: "must have required property '" + "columnCount" + "'"
            };
            if (vErrors === null) {
                vErrors = [err1];
            }
            else {
                vErrors.push(err1);
            }
            errors++;
        }
        for (const key0 in data) {
            if (!(((((key0 === "border") || (key0 === "columns")) || (key0 === "columnDefault")) || (key0 === "columnCount")) || (key0 === "drawVerticalLine"))) {
                const err2 = {
                    instancePath,
                    schemaPath: "#/additionalProperties",
                    keyword: "additionalProperties",
                    params: {
                        additionalProperty: key0
                    },
                    message: "must NOT have additional properties"
                };
                if (vErrors === null) {
                    vErrors = [err2];
                }
                else {
                    vErrors.push(err2);
                }
                errors++;
            }
        }
        if (data.border !== undefined) {
            if (!(validate77(data.border, {
                instancePath: instancePath + "/border",
                parentData: data,
                parentDataProperty: "border",
                rootData
            }))) {
                vErrors = vErrors === null ? validate77.errors : vErrors.concat(validate77.errors);
                errors = vErrors.length;
            }
        }
        if (data.columns !== undefined) {
            if (!(validate95(data.columns, {
                instancePath: instancePath + "/columns",
                parentData: data,
                parentDataProperty: "columns",
                rootData
            }))) {
                vErrors = vErrors === null ? validate95.errors : vErrors.concat(validate95.errors);
                errors = vErrors.length;
            }
        }
        if (data.columnDefault !== undefined) {
            if (!(validate99(data.columnDefault, {
                instancePath: instancePath + "/columnDefault",
                parentData: data,
                parentDataProperty: "columnDefault",
                rootData
            }))) {
                vErrors = vErrors === null ? validate99.errors : vErrors.concat(validate99.errors);
                errors = vErrors.length;
            }
        }
        if (data.columnCount !== undefined) {
            let data3 = data.columnCount;
            if (!(((typeof data3 == "number") && (!(data3 % 1) && !isNaN(data3))) && (isFinite(data3)))) {
                const err3 = {
                    instancePath: instancePath + "/columnCount",
                    schemaPath: "#/properties/columnCount/type",
                    keyword: "type",
                    params: {
                        type: "integer"
                    },
                    message: "must be integer"
                };
                if (vErrors === null) {
                    vErrors = [err3];
                }
                else {
                    vErrors.push(err3);
                }
                errors++;
            }
            if ((typeof data3 == "number") && (isFinite(data3))) {
                if (data3 < 1 || isNaN(data3)) {
                    const err4 = {
                        instancePath: instancePath + "/columnCount",
                        schemaPath: "#/properties/columnCount/minimum",
                        keyword: "minimum",
                        params: {
                            comparison: ">=",
                            limit: 1
                        },
                        message: "must be >= 1"
                    };
                    if (vErrors === null) {
                        vErrors = [err4];
                    }
                    else {
                        vErrors.push(err4);
                    }
                    errors++;
                }
            }
        }
        if (data.drawVerticalLine !== undefined) {
            if (typeof data.drawVerticalLine != "function") {
                const err5 = {
                    instancePath: instancePath + "/drawVerticalLine",
                    schemaPath: "#/properties/drawVerticalLine/typeof",
                    keyword: "typeof",
                    params: {},
                    message: "should pass \"typeof\" keyword validation"
                };
                if (vErrors === null) {
                    vErrors = [err5];
                }
                else {
                    vErrors.push(err5);
                }
                errors++;
            }
        }
    }
    else {
        const err6 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err6];
        }
        else {
            vErrors.push(err6);
        }
        errors++;
    }
    validate76.errors = vErrors;
    return errors === 0;
}
