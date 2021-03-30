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
        "columns": {
            "$ref": "shared.json#/definitions/columns"
        },
        "columnDefault": {
            "$ref": "shared.json#/definitions/column"
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

function validate46(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
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
        } else {
            vErrors.push(err0);
        }
        errors++;
    }
    validate46.errors = vErrors;
    return errors === 0;
}

function validate45(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
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
                } else {
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
    } else {
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
        } else {
            vErrors.push(err1);
        }
        errors++;
    }
    validate45.errors = vErrors;
    return errors === 0;
}
const schema17 = {
    "type": "object",
    "patternProperties": {
        "^[0-9]+$": {
            "$ref": "#/definitions/column"
        }
    },
    "additionalProperties": false
};
const pattern0 = new RegExp("^[0-9]+$", "u");
const schema18 = {
    "type": "object",
    "properties": {
        "alignment": {
            "type": "string",
            "enum": ["left", "right", "center"]
        },
        "width": {
            "type": "number"
        },
        "wrapWord": {
            "type": "boolean"
        },
        "truncate": {
            "type": "number"
        },
        "paddingLeft": {
            "type": "number"
        },
        "paddingRight": {
            "type": "number"
        }
    },
    "additionalProperties": false
};
const func0 = require("ajv/dist/runtime/equal").default;

function validate64(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!((((((key0 === "alignment") || (key0 === "width")) || (key0 === "wrapWord")) || (key0 === "truncate")) || (key0 === "paddingLeft")) || (key0 === "paddingRight"))) {
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
                } else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.alignment !== undefined) {
            let data0 = data.alignment;
            if (typeof data0 !== "string") {
                const err1 = {
                    instancePath: instancePath + "/alignment",
                    schemaPath: "#/properties/alignment/type",
                    keyword: "type",
                    params: {
                        type: "string"
                    },
                    message: "must be string"
                };
                if (vErrors === null) {
                    vErrors = [err1];
                } else {
                    vErrors.push(err1);
                }
                errors++;
            }
            if (!(((data0 === "left") || (data0 === "right")) || (data0 === "center"))) {
                const err2 = {
                    instancePath: instancePath + "/alignment",
                    schemaPath: "#/properties/alignment/enum",
                    keyword: "enum",
                    params: {
                        allowedValues: schema18.properties.alignment.enum
                    },
                    message: "must be equal to one of the allowed values"
                };
                if (vErrors === null) {
                    vErrors = [err2];
                } else {
                    vErrors.push(err2);
                }
                errors++;
            }
        }
        if (data.width !== undefined) {
            let data1 = data.width;
            if (!((typeof data1 == "number") && (isFinite(data1)))) {
                const err3 = {
                    instancePath: instancePath + "/width",
                    schemaPath: "#/properties/width/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err3];
                } else {
                    vErrors.push(err3);
                }
                errors++;
            }
        }
        if (data.wrapWord !== undefined) {
            if (typeof data.wrapWord !== "boolean") {
                const err4 = {
                    instancePath: instancePath + "/wrapWord",
                    schemaPath: "#/properties/wrapWord/type",
                    keyword: "type",
                    params: {
                        type: "boolean"
                    },
                    message: "must be boolean"
                };
                if (vErrors === null) {
                    vErrors = [err4];
                } else {
                    vErrors.push(err4);
                }
                errors++;
            }
        }
        if (data.truncate !== undefined) {
            let data3 = data.truncate;
            if (!((typeof data3 == "number") && (isFinite(data3)))) {
                const err5 = {
                    instancePath: instancePath + "/truncate",
                    schemaPath: "#/properties/truncate/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err5];
                } else {
                    vErrors.push(err5);
                }
                errors++;
            }
        }
        if (data.paddingLeft !== undefined) {
            let data4 = data.paddingLeft;
            if (!((typeof data4 == "number") && (isFinite(data4)))) {
                const err6 = {
                    instancePath: instancePath + "/paddingLeft",
                    schemaPath: "#/properties/paddingLeft/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err6];
                } else {
                    vErrors.push(err6);
                }
                errors++;
            }
        }
        if (data.paddingRight !== undefined) {
            let data5 = data.paddingRight;
            if (!((typeof data5 == "number") && (isFinite(data5)))) {
                const err7 = {
                    instancePath: instancePath + "/paddingRight",
                    schemaPath: "#/properties/paddingRight/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err7];
                } else {
                    vErrors.push(err7);
                }
                errors++;
            }
        }
    } else {
        const err8 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err8];
        } else {
            vErrors.push(err8);
        }
        errors++;
    }
    validate64.errors = vErrors;
    return errors === 0;
}

function validate63(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(pattern0.test(key0))) {
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
                } else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        for (const key1 in data) {
            if (pattern0.test(key1)) {
                if (!(validate64(data[key1], {
                        instancePath: instancePath + "/" + key1.replace(/~/g, "~0").replace(/\//g, "~1"),
                        parentData: data,
                        parentDataProperty: key1,
                        rootData
                    }))) {
                    vErrors = vErrors === null ? validate64.errors : vErrors.concat(validate64.errors);
                    errors = vErrors.length;
                }
            }
        }
    } else {
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
        } else {
            vErrors.push(err1);
        }
        errors++;
    }
    validate63.errors = vErrors;
    return errors === 0;
}

function validate67(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!((((((key0 === "alignment") || (key0 === "width")) || (key0 === "wrapWord")) || (key0 === "truncate")) || (key0 === "paddingLeft")) || (key0 === "paddingRight"))) {
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
                } else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.alignment !== undefined) {
            let data0 = data.alignment;
            if (typeof data0 !== "string") {
                const err1 = {
                    instancePath: instancePath + "/alignment",
                    schemaPath: "#/properties/alignment/type",
                    keyword: "type",
                    params: {
                        type: "string"
                    },
                    message: "must be string"
                };
                if (vErrors === null) {
                    vErrors = [err1];
                } else {
                    vErrors.push(err1);
                }
                errors++;
            }
            if (!(((data0 === "left") || (data0 === "right")) || (data0 === "center"))) {
                const err2 = {
                    instancePath: instancePath + "/alignment",
                    schemaPath: "#/properties/alignment/enum",
                    keyword: "enum",
                    params: {
                        allowedValues: schema18.properties.alignment.enum
                    },
                    message: "must be equal to one of the allowed values"
                };
                if (vErrors === null) {
                    vErrors = [err2];
                } else {
                    vErrors.push(err2);
                }
                errors++;
            }
        }
        if (data.width !== undefined) {
            let data1 = data.width;
            if (!((typeof data1 == "number") && (isFinite(data1)))) {
                const err3 = {
                    instancePath: instancePath + "/width",
                    schemaPath: "#/properties/width/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err3];
                } else {
                    vErrors.push(err3);
                }
                errors++;
            }
        }
        if (data.wrapWord !== undefined) {
            if (typeof data.wrapWord !== "boolean") {
                const err4 = {
                    instancePath: instancePath + "/wrapWord",
                    schemaPath: "#/properties/wrapWord/type",
                    keyword: "type",
                    params: {
                        type: "boolean"
                    },
                    message: "must be boolean"
                };
                if (vErrors === null) {
                    vErrors = [err4];
                } else {
                    vErrors.push(err4);
                }
                errors++;
            }
        }
        if (data.truncate !== undefined) {
            let data3 = data.truncate;
            if (!((typeof data3 == "number") && (isFinite(data3)))) {
                const err5 = {
                    instancePath: instancePath + "/truncate",
                    schemaPath: "#/properties/truncate/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err5];
                } else {
                    vErrors.push(err5);
                }
                errors++;
            }
        }
        if (data.paddingLeft !== undefined) {
            let data4 = data.paddingLeft;
            if (!((typeof data4 == "number") && (isFinite(data4)))) {
                const err6 = {
                    instancePath: instancePath + "/paddingLeft",
                    schemaPath: "#/properties/paddingLeft/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err6];
                } else {
                    vErrors.push(err6);
                }
                errors++;
            }
        }
        if (data.paddingRight !== undefined) {
            let data5 = data.paddingRight;
            if (!((typeof data5 == "number") && (isFinite(data5)))) {
                const err7 = {
                    instancePath: instancePath + "/paddingRight",
                    schemaPath: "#/properties/paddingRight/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err7];
                } else {
                    vErrors.push(err7);
                }
                errors++;
            }
        }
    } else {
        const err8 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err8];
        } else {
            vErrors.push(err8);
        }
        errors++;
    }
    validate67.errors = vErrors;
    return errors === 0;
}

function validate43(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
    /*# sourceURL="config.json" */ ;
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(((((key0 === "border") || (key0 === "columns")) || (key0 === "columnDefault")) || (key0 === "drawHorizontalLine")) || (key0 === "singleLine"))) {
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
                } else {
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
        if (data.columns !== undefined) {
            if (!(validate63(data.columns, {
                    instancePath: instancePath + "/columns",
                    parentData: data,
                    parentDataProperty: "columns",
                    rootData
                }))) {
                vErrors = vErrors === null ? validate63.errors : vErrors.concat(validate63.errors);
                errors = vErrors.length;
            }
        }
        if (data.columnDefault !== undefined) {
            if (!(validate67(data.columnDefault, {
                    instancePath: instancePath + "/columnDefault",
                    parentData: data,
                    parentDataProperty: "columnDefault",
                    rootData
                }))) {
                vErrors = vErrors === null ? validate67.errors : vErrors.concat(validate67.errors);
                errors = vErrors.length;
            }
        }
        if (data.drawHorizontalLine !== undefined) {
            if (typeof data.drawHorizontalLine != "function") {
                const err1 = {
                    instancePath: instancePath + "/drawHorizontalLine",
                    schemaPath: "#/properties/drawHorizontalLine/typeof",
                    keyword: "typeof",
                    params: {},
                    message: "should pass \"typeof\" keyword validation"
                };
                if (vErrors === null) {
                    vErrors = [err1];
                } else {
                    vErrors.push(err1);
                }
                errors++;
            }
        }
        if (data.singleLine !== undefined) {
            if (typeof data.singleLine != "boolean") {
                const err2 = {
                    instancePath: instancePath + "/singleLine",
                    schemaPath: "#/properties/singleLine/typeof",
                    keyword: "typeof",
                    params: {},
                    message: "should pass \"typeof\" keyword validation"
                };
                if (vErrors === null) {
                    vErrors = [err2];
                } else {
                    vErrors.push(err2);
                }
                errors++;
            }
        }
    } else {
        const err3 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err3];
        } else {
            vErrors.push(err3);
        }
        errors++;
    }
    validate43.errors = vErrors;
    return errors === 0;
}
exports["streamConfig.json"] = validate69;
const schema20 = {
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
            "type": "number"
        }
    },
    "additionalProperties": false
};

function validate70(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
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
                } else {
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
    } else {
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
        } else {
            vErrors.push(err1);
        }
        errors++;
    }
    validate70.errors = vErrors;
    return errors === 0;
}

function validate87(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!(pattern0.test(key0))) {
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
                } else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        for (const key1 in data) {
            if (pattern0.test(key1)) {
                if (!(validate64(data[key1], {
                        instancePath: instancePath + "/" + key1.replace(/~/g, "~0").replace(/\//g, "~1"),
                        parentData: data,
                        parentDataProperty: key1,
                        rootData
                    }))) {
                    vErrors = vErrors === null ? validate64.errors : vErrors.concat(validate64.errors);
                    errors = vErrors.length;
                }
            }
        }
    } else {
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
        } else {
            vErrors.push(err1);
        }
        errors++;
    }
    validate87.errors = vErrors;
    return errors === 0;
}

function validate90(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!((((((key0 === "alignment") || (key0 === "width")) || (key0 === "wrapWord")) || (key0 === "truncate")) || (key0 === "paddingLeft")) || (key0 === "paddingRight"))) {
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
                } else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.alignment !== undefined) {
            let data0 = data.alignment;
            if (typeof data0 !== "string") {
                const err1 = {
                    instancePath: instancePath + "/alignment",
                    schemaPath: "#/properties/alignment/type",
                    keyword: "type",
                    params: {
                        type: "string"
                    },
                    message: "must be string"
                };
                if (vErrors === null) {
                    vErrors = [err1];
                } else {
                    vErrors.push(err1);
                }
                errors++;
            }
            if (!(((data0 === "left") || (data0 === "right")) || (data0 === "center"))) {
                const err2 = {
                    instancePath: instancePath + "/alignment",
                    schemaPath: "#/properties/alignment/enum",
                    keyword: "enum",
                    params: {
                        allowedValues: schema18.properties.alignment.enum
                    },
                    message: "must be equal to one of the allowed values"
                };
                if (vErrors === null) {
                    vErrors = [err2];
                } else {
                    vErrors.push(err2);
                }
                errors++;
            }
        }
        if (data.width !== undefined) {
            let data1 = data.width;
            if (!((typeof data1 == "number") && (isFinite(data1)))) {
                const err3 = {
                    instancePath: instancePath + "/width",
                    schemaPath: "#/properties/width/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err3];
                } else {
                    vErrors.push(err3);
                }
                errors++;
            }
        }
        if (data.wrapWord !== undefined) {
            if (typeof data.wrapWord !== "boolean") {
                const err4 = {
                    instancePath: instancePath + "/wrapWord",
                    schemaPath: "#/properties/wrapWord/type",
                    keyword: "type",
                    params: {
                        type: "boolean"
                    },
                    message: "must be boolean"
                };
                if (vErrors === null) {
                    vErrors = [err4];
                } else {
                    vErrors.push(err4);
                }
                errors++;
            }
        }
        if (data.truncate !== undefined) {
            let data3 = data.truncate;
            if (!((typeof data3 == "number") && (isFinite(data3)))) {
                const err5 = {
                    instancePath: instancePath + "/truncate",
                    schemaPath: "#/properties/truncate/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err5];
                } else {
                    vErrors.push(err5);
                }
                errors++;
            }
        }
        if (data.paddingLeft !== undefined) {
            let data4 = data.paddingLeft;
            if (!((typeof data4 == "number") && (isFinite(data4)))) {
                const err6 = {
                    instancePath: instancePath + "/paddingLeft",
                    schemaPath: "#/properties/paddingLeft/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err6];
                } else {
                    vErrors.push(err6);
                }
                errors++;
            }
        }
        if (data.paddingRight !== undefined) {
            let data5 = data.paddingRight;
            if (!((typeof data5 == "number") && (isFinite(data5)))) {
                const err7 = {
                    instancePath: instancePath + "/paddingRight",
                    schemaPath: "#/properties/paddingRight/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err7];
                } else {
                    vErrors.push(err7);
                }
                errors++;
            }
        }
    } else {
        const err8 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err8];
        } else {
            vErrors.push(err8);
        }
        errors++;
    }
    validate90.errors = vErrors;
    return errors === 0;
}

function validate69(data, {
    instancePath = "",
    parentData,
    parentDataProperty,
    rootData = data
} = {}) {
    /*# sourceURL="streamConfig.json" */ ;
    let vErrors = null;
    let errors = 0;
    if (data && typeof data == "object" && !Array.isArray(data)) {
        for (const key0 in data) {
            if (!((((key0 === "border") || (key0 === "columns")) || (key0 === "columnDefault")) || (key0 === "columnCount"))) {
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
                } else {
                    vErrors.push(err0);
                }
                errors++;
            }
        }
        if (data.border !== undefined) {
            if (!(validate70(data.border, {
                    instancePath: instancePath + "/border",
                    parentData: data,
                    parentDataProperty: "border",
                    rootData
                }))) {
                vErrors = vErrors === null ? validate70.errors : vErrors.concat(validate70.errors);
                errors = vErrors.length;
            }
        }
        if (data.columns !== undefined) {
            if (!(validate87(data.columns, {
                    instancePath: instancePath + "/columns",
                    parentData: data,
                    parentDataProperty: "columns",
                    rootData
                }))) {
                vErrors = vErrors === null ? validate87.errors : vErrors.concat(validate87.errors);
                errors = vErrors.length;
            }
        }
        if (data.columnDefault !== undefined) {
            if (!(validate90(data.columnDefault, {
                    instancePath: instancePath + "/columnDefault",
                    parentData: data,
                    parentDataProperty: "columnDefault",
                    rootData
                }))) {
                vErrors = vErrors === null ? validate90.errors : vErrors.concat(validate90.errors);
                errors = vErrors.length;
            }
        }
        if (data.columnCount !== undefined) {
            let data3 = data.columnCount;
            if (!((typeof data3 == "number") && (isFinite(data3)))) {
                const err1 = {
                    instancePath: instancePath + "/columnCount",
                    schemaPath: "#/properties/columnCount/type",
                    keyword: "type",
                    params: {
                        type: "number"
                    },
                    message: "must be number"
                };
                if (vErrors === null) {
                    vErrors = [err1];
                } else {
                    vErrors.push(err1);
                }
                errors++;
            }
        }
    } else {
        const err2 = {
            instancePath,
            schemaPath: "#/type",
            keyword: "type",
            params: {
                type: "object"
            },
            message: "must be object"
        };
        if (vErrors === null) {
            vErrors = [err2];
        } else {
            vErrors.push(err2);
        }
        errors++;
    }
    validate69.errors = vErrors;
    return errors === 0;
}