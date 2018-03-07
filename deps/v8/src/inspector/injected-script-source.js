/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

"use strict";

/**
 * @param {!InjectedScriptHostClass} InjectedScriptHost
 * @param {!Window|!WorkerGlobalScope} inspectedGlobalObject
 * @param {number} injectedScriptId
 * @suppress {uselessCode}
 */
(function (InjectedScriptHost, inspectedGlobalObject, injectedScriptId) {

/**
 * @param {!Array.<T>} array
 * @param {...} var_args
 * @template T
 */
function push(array, var_args)
{
    for (var i = 1; i < arguments.length; ++i)
        array[array.length] = arguments[i];
}

/**
 * @param {*} obj
 * @return {string}
 * @suppress {uselessCode}
 */
function toString(obj)
{
    // We don't use String(obj) because String could be overridden.
    // Also the ("" + obj) expression may throw.
    try {
        return "" + obj;
    } catch (e) {
        var name = InjectedScriptHost.internalConstructorName(obj) || InjectedScriptHost.subtype(obj) || (typeof obj);
        return "#<" + name + ">";
    }
}

/**
 * @param {*} obj
 * @return {string}
 */
function toStringDescription(obj)
{
    if (typeof obj === "number" && obj === 0 && 1 / obj < 0)
        return "-0"; // Negative zero.
    return toString(obj);
}

/**
 * FireBug's array detection.
 * @param {*} obj
 * @return {boolean}
 */
function isArrayLike(obj)
{
    if (typeof obj !== "object")
        return false;
    var splice = InjectedScriptHost.getProperty(obj, "splice");
    if (typeof splice === "function") {
        if (!InjectedScriptHost.objectHasOwnProperty(/** @type {!Object} */ (obj), "length"))
            return false;
        var len = InjectedScriptHost.getProperty(obj, "length");
        // is len uint32?
        return typeof len === "number" && len >>> 0 === len && (len > 0 || 1 / len > 0);
    }
    return false;
}

/**
 * @param {number} a
 * @param {number} b
 * @return {number}
 */
function max(a, b)
{
    return a > b ? a : b;
}

/**
 * FIXME: Remove once ES6 is supported natively by JS compiler.
 * @param {*} obj
 * @return {boolean}
 */
function isSymbol(obj)
{
    var type = typeof obj;
    return (type === "symbol");
}

/**
 * DOM Attributes which have observable side effect on getter, in the form of
 *   {interfaceName1: {attributeName1: true,
 *                     attributeName2: true,
 *                     ...},
 *    interfaceName2: {...},
 *    ...}
 * @type {!Object<string, !Object<string, boolean>>}
 * @const
 */
var domAttributesWithObservableSideEffectOnGet = {
    Request: { body: true, __proto__: null },
    Response: { body: true, __proto__: null },
    __proto__: null
}

/**
 * @param {!Object} object
 * @param {string} attribute
 * @return {boolean}
 */
function doesAttributeHaveObservableSideEffectOnGet(object, attribute)
{
    for (var interfaceName in domAttributesWithObservableSideEffectOnGet) {
        var interfaceFunction = inspectedGlobalObject[interfaceName];
        // Call to instanceOf looks safe after typeof check.
        var isInstance = typeof interfaceFunction === "function" && /* suppressBlacklist */ object instanceof interfaceFunction;
        if (isInstance)
            return attribute in domAttributesWithObservableSideEffectOnGet[interfaceName];
    }
    return false;
}

/**
 * @constructor
 */
var InjectedScript = function()
{
}
InjectedScriptHost.nullifyPrototype(InjectedScript);

/**
 * @type {!Object<string, boolean>}
 * @const
 */
InjectedScript.primitiveTypes = {
    "undefined": true,
    "boolean": true,
    "number": true,
    "string": true,
    __proto__: null
}

/**
 * @type {!Object<string, string>}
 * @const
 */
InjectedScript.closureTypes = {
    "local": "Local",
    "closure": "Closure",
    "catch": "Catch",
    "block": "Block",
    "script": "Script",
    "with": "With Block",
    "global": "Global",
    "eval": "Eval",
    "module": "Module",
    __proto__: null
};

InjectedScript.prototype = {
    /**
     * @param {*} object
     * @return {boolean}
     */
    isPrimitiveValue: function(object)
    {
        // FIXME(33716): typeof document.all is always 'undefined'.
        return InjectedScript.primitiveTypes[typeof object] && !this._isHTMLAllCollection(object);
    },

    /**
     * @param {*} object
     * @return {boolean}
     */
    _shouldPassByValue: function(object)
    {
        return typeof object === "object" && InjectedScriptHost.subtype(object) === "internal#location";
    },

    /**
     * @param {*} object
     * @param {string} groupName
     * @param {boolean} forceValueType
     * @param {boolean} generatePreview
     * @return {!RuntimeAgent.RemoteObject}
     */
    wrapObject: function(object, groupName, forceValueType, generatePreview)
    {
        return this._wrapObject(object, groupName, forceValueType, generatePreview);
    },

    /**
     * @param {!Object} table
     * @param {!Array.<string>|string|boolean} columns
     * @return {!RuntimeAgent.RemoteObject}
     */
    wrapTable: function(table, columns)
    {
        var columnNames = null;
        if (typeof columns === "string")
            columns = [columns];
        if (InjectedScriptHost.subtype(columns) === "array") {
            columnNames = [];
            InjectedScriptHost.nullifyPrototype(columnNames);
            for (var i = 0; i < columns.length; ++i)
                columnNames[i] = toString(columns[i]);
        }
        return this._wrapObject(table, "console", false, true, columnNames, true);
    },

    /**
     * This method cannot throw.
     * @param {*} object
     * @param {string=} objectGroupName
     * @param {boolean=} forceValueType
     * @param {boolean=} generatePreview
     * @param {?Array.<string>=} columnNames
     * @param {boolean=} isTable
     * @param {boolean=} doNotBind
     * @param {*=} customObjectConfig
     * @return {!RuntimeAgent.RemoteObject}
     * @suppress {checkTypes}
     */
    _wrapObject: function(object, objectGroupName, forceValueType, generatePreview, columnNames, isTable, doNotBind, customObjectConfig)
    {
        try {
            return new InjectedScript.RemoteObject(object, objectGroupName, doNotBind, forceValueType, generatePreview, columnNames, isTable, undefined, customObjectConfig);
        } catch (e) {
            try {
                var description = injectedScript._describe(e);
            } catch (ex) {
                var description = "<failed to convert exception to string>";
            }
            return new InjectedScript.RemoteObject(description);
        }
    },

    /**
     * @param {!Object|symbol} object
     * @param {string=} objectGroupName
     * @return {string}
     */
    _bind: function(object, objectGroupName)
    {
        var id = InjectedScriptHost.bind(object, objectGroupName || "");
        return "{\"injectedScriptId\":" + injectedScriptId + ",\"id\":" + id + "}";
    },

    /**
     * @param {!Object} object
     * @param {string} objectGroupName
     * @param {boolean} ownProperties
     * @param {boolean} accessorPropertiesOnly
     * @param {boolean} generatePreview
     * @return {!Array<!RuntimeAgent.PropertyDescriptor>|boolean}
     */
    getProperties: function(object, objectGroupName, ownProperties, accessorPropertiesOnly, generatePreview)
    {
        var subtype = this._subtype(object);
        if (subtype === "internal#scope") {
            // Internally, scope contains object with scope variables and additional information like type,
            // we use additional information for preview and would like to report variables as scope
            // properties.
            object = object.object;
        }

        // Go over properties, wrap object values.
        var descriptors = this._propertyDescriptors(object, addPropertyIfNeeded, ownProperties, accessorPropertiesOnly);
        for (var i = 0; i < descriptors.length; ++i) {
            var descriptor = descriptors[i];
            if ("get" in descriptor)
                descriptor.get = this._wrapObject(descriptor.get, objectGroupName);
            if ("set" in descriptor)
                descriptor.set = this._wrapObject(descriptor.set, objectGroupName);
            if ("value" in descriptor)
                descriptor.value = this._wrapObject(descriptor.value, objectGroupName, false, generatePreview);
            if (!("configurable" in descriptor))
                descriptor.configurable = false;
            if (!("enumerable" in descriptor))
                descriptor.enumerable = false;
            if ("symbol" in descriptor)
                descriptor.symbol = this._wrapObject(descriptor.symbol, objectGroupName);
        }
        return descriptors;

        /**
         * @param {!Array<!Object>} descriptors
         * @param {!Object} descriptor
         * @return {boolean}
         */
        function addPropertyIfNeeded(descriptors, descriptor) {
            push(descriptors, descriptor);
            return true;
        }
    },

    /**
     * @param {!Object} object
     * @return {?Object}
     */
    _objectPrototype: function(object)
    {
        if (InjectedScriptHost.subtype(object) === "proxy")
            return null;
        try {
            return InjectedScriptHost.getPrototypeOf(object);
        } catch (e) {
            return null;
        }
    },

    /**
     * @param {!Object} object
     * @param {!function(!Array<!Object>, !Object)} addPropertyIfNeeded
     * @param {boolean=} ownProperties
     * @param {boolean=} accessorPropertiesOnly
     * @param {?Array<string>=} propertyNamesOnly
     * @return {!Array<!Object>}
     */
    _propertyDescriptors: function(object, addPropertyIfNeeded, ownProperties, accessorPropertiesOnly, propertyNamesOnly)
    {
        var descriptors = [];
        InjectedScriptHost.nullifyPrototype(descriptors);
        var propertyProcessed = { __proto__: null };
        var subtype = InjectedScriptHost.subtype(object);

        /**
         * @param {!Object} o
         * @param {!Array<string|number|symbol>=} properties
         * @param {number=} objectLength
         * @return {boolean}
         */
        function process(o, properties, objectLength)
        {
            // When properties is not provided, iterate over the object's indices.
            var length = properties ? properties.length : objectLength;
            for (var i = 0; i < length; ++i) {
                var property = properties ? properties[i] : ("" + i);
                if (propertyProcessed[property])
                    continue;
                propertyProcessed[property] = true;
                var name;
                if (isSymbol(property))
                    name = /** @type {string} */ (injectedScript._describe(property));
                else
                    name = typeof property === "number" ? ("" + property) : /** @type {string} */(property);

                if (subtype === "internal#scopeList" && name === "length")
                    continue;

                var descriptor;
                try {
                    var nativeAccessorDescriptor = InjectedScriptHost.nativeAccessorDescriptor(o, property);
                    if (nativeAccessorDescriptor && !nativeAccessorDescriptor.isBuiltin) {
                        descriptor = { __proto__: null };
                        if (nativeAccessorDescriptor.hasGetter)
                            descriptor.get = function nativeGetter() { return o[property]; };
                        if (nativeAccessorDescriptor.hasSetter)
                            descriptor.set = function nativeSetter(v) { o[property] = v; };
                    } else {
                        descriptor = InjectedScriptHost.getOwnPropertyDescriptor(o, property);
                        if (descriptor) {
                            InjectedScriptHost.nullifyPrototype(descriptor);
                        }
                    }
                    var isAccessorProperty = descriptor && ("get" in descriptor || "set" in descriptor);
                    if (accessorPropertiesOnly && !isAccessorProperty)
                        continue;
                    if (descriptor && "get" in descriptor && "set" in descriptor && name !== "__proto__" &&
                            InjectedScriptHost.formatAccessorsAsProperties(object, descriptor.get) &&
                            !doesAttributeHaveObservableSideEffectOnGet(object, name)) {
                        descriptor.value = object[property];
                        descriptor.isOwn = true;
                        delete descriptor.get;
                        delete descriptor.set;
                    }
                } catch (e) {
                    if (accessorPropertiesOnly)
                        continue;
                    descriptor = { value: e, wasThrown: true, __proto__: null };
                }

                // Not all bindings provide proper descriptors. Fall back to the non-configurable, non-enumerable,
                // non-writable property.
                if (!descriptor) {
                    try {
                        descriptor = { value: o[property], writable: false, __proto__: null };
                    } catch (e) {
                        // Silent catch.
                        continue;
                    }
                }

                descriptor.name = name;
                if (o === object)
                    descriptor.isOwn = true;
                if (isSymbol(property))
                    descriptor.symbol = property;
                if (!addPropertyIfNeeded(descriptors, descriptor))
                    return false;
            }
            return true;
        }

        if (propertyNamesOnly) {
            for (var i = 0; i < propertyNamesOnly.length; ++i) {
                var name = propertyNamesOnly[i];
                for (var o = object; this._isDefined(o); o = this._objectPrototype(/** @type {!Object} */ (o))) {
                    o = /** @type {!Object} */ (o);
                    if (InjectedScriptHost.objectHasOwnProperty(o, name)) {
                        if (!process(o, [name]))
                            return descriptors;
                        break;
                    }
                    if (ownProperties)
                        break;
                }
            }
            return descriptors;
        }

        var skipGetOwnPropertyNames;
        try {
            skipGetOwnPropertyNames = subtype === "typedarray" && object.length > 500000;
        } catch (e) {
        }

        for (var o = object; this._isDefined(o); o = this._objectPrototype(/** @type {!Object} */ (o))) {
            o = /** @type {!Object} */ (o);
            if (InjectedScriptHost.subtype(o) === "proxy")
                continue;

            var typedArrays = subtype === "arraybuffer" ? InjectedScriptHost.typedArrayProperties(o) || [] : [];
            for (var i = 0; i < typedArrays.length; i += 2)
                addPropertyIfNeeded(descriptors, { name: typedArrays[i], value: typedArrays[i + 1], isOwn: true, enumerable: false, configurable: false, __proto__: null });

            try {
                if (skipGetOwnPropertyNames && o === object) {
                    if (!process(o, undefined, o.length))
                        return descriptors;
                } else {
                    // First call Object.keys() to enforce ordering of the property descriptors.
                    if (!process(o, InjectedScriptHost.keys(o)))
                        return descriptors;
                    if (!process(o, InjectedScriptHost.getOwnPropertyNames(o)))
                        return descriptors;
                }
                if (!process(o, InjectedScriptHost.getOwnPropertySymbols(o)))
                    return descriptors;

                if (ownProperties) {
                    var proto = this._objectPrototype(o);
                    if (proto && !accessorPropertiesOnly) {
                        var descriptor = { name: "__proto__", value: proto, writable: true, configurable: true, enumerable: false, isOwn: true, __proto__: null };
                        if (!addPropertyIfNeeded(descriptors, descriptor))
                            return descriptors;
                    }
                }
            } catch (e) {
            }

            if (ownProperties)
                break;
        }
        return descriptors;
    },

    /**
     * @param {string|undefined} objectGroupName
     * @param {*} jsonMLObject
     * @throws {string} error message
     */
    _substituteObjectTagsInCustomPreview: function(objectGroupName, jsonMLObject)
    {
        var maxCustomPreviewRecursionDepth = 20;
        this._customPreviewRecursionDepth = (this._customPreviewRecursionDepth || 0) + 1
        try {
            if (this._customPreviewRecursionDepth >= maxCustomPreviewRecursionDepth)
                throw new Error("Too deep hierarchy of inlined custom previews");

            if (!isArrayLike(jsonMLObject))
                return;

            if (jsonMLObject[0] === "object") {
                var attributes = jsonMLObject[1];
                var originObject = attributes["object"];
                var config = attributes["config"];
                if (typeof originObject === "undefined")
                    throw new Error("Illegal format: obligatory attribute \"object\" isn't specified");

                jsonMLObject[1] = this._wrapObject(originObject, objectGroupName, false, false, null, false, false, config);
                return;
            }

            for (var i = 0; i < jsonMLObject.length; ++i)
                this._substituteObjectTagsInCustomPreview(objectGroupName, jsonMLObject[i]);
        } finally {
            this._customPreviewRecursionDepth--;
        }
    },

    /**
     * @param {*} object
     * @return {boolean}
     */
    _isDefined: function(object)
    {
        return !!object || this._isHTMLAllCollection(object);
    },

    /**
     * @param {*} object
     * @return {boolean}
     */
    _isHTMLAllCollection: function(object)
    {
        // document.all is reported as undefined, but we still want to process it.
        return (typeof object === "undefined") && !!InjectedScriptHost.subtype(object);
    },

    /**
     * @param {*} obj
     * @return {?string}
     */
    _subtype: function(obj)
    {
        if (obj === null)
            return "null";

        if (this.isPrimitiveValue(obj))
            return null;

        var subtype = InjectedScriptHost.subtype(obj);
        if (subtype)
            return subtype;

        if (isArrayLike(obj))
            return "array";

        // If owning frame has navigated to somewhere else window properties will be undefined.
        return null;
    },

    /**
     * @param {*} obj
     * @return {?string}
     */
    _describe: function(obj)
    {
        if (this.isPrimitiveValue(obj))
            return null;

        var subtype = this._subtype(obj);

        if (subtype === "regexp")
            return toString(obj);

        if (subtype === "date")
            return toString(obj);

        if (subtype === "node") {
            var description = "";
            var nodeName = InjectedScriptHost.getProperty(obj, "nodeName");
            if (nodeName) {
                description = nodeName.toLowerCase();
            } else {
                var constructor = InjectedScriptHost.getProperty(obj, "constructor");
                if (constructor)
                    description = (InjectedScriptHost.getProperty(constructor, "name") || "").toLowerCase();
            }

            var nodeType = InjectedScriptHost.getProperty(obj, "nodeType");
            switch (nodeType) {
            case 1 /* Node.ELEMENT_NODE */:
                var id = InjectedScriptHost.getProperty(obj, "id");
                description += id ? "#" + id : "";
                var className = InjectedScriptHost.getProperty(obj, "className");
                description += (className && typeof className === "string") ? "." + className.trim().replace(/\s+/g, ".") : "";
                break;
            case 10 /*Node.DOCUMENT_TYPE_NODE */:
                description = "<!DOCTYPE " + description + ">";
                break;
            }
            return description;
        }

        if (subtype === "proxy")
            return "Proxy";

        var className = InjectedScriptHost.internalConstructorName(obj);
        if (subtype === "array" || subtype === "typedarray") {
            if (typeof obj.length === "number")
                return className + "(" + obj.length + ")";
            return className;
        }

        if (subtype === "map" || subtype === "set" || subtype === "blob") {
            if (typeof obj.size === "number")
                return className + "(" + obj.size + ")";
            return className;
        }

        if (subtype === "arraybuffer" || subtype === "dataview") {
            if (typeof obj.byteLength === "number")
                return className + "(" + obj.byteLength + ")";
            return className;
        }

        if (typeof obj === "function")
            return toString(obj);

        if (isSymbol(obj)) {
            try {
                // It isn't safe, because Symbol.prototype.toString can be overriden.
                return /* suppressBlacklist */ obj.toString() || "Symbol";
            } catch (e) {
                return "Symbol";
            }
        }

        if (InjectedScriptHost.subtype(obj) === "error") {
            try {
                var stack = obj.stack;
                var message = obj.message && obj.message.length ? ": " + obj.message : "";
                var firstCallFrame = /^\s+at\s/m.exec(stack);
                var stackMessageEnd = firstCallFrame ? firstCallFrame.index : -1;
                if (stackMessageEnd !== -1) {
                    var stackTrace = stack.substr(stackMessageEnd);
                    return className + message + "\n" + stackTrace;
                }
                return className + message;
            } catch(e) {
            }
        }

        if (subtype === "internal#entry") {
            if ("key" in obj)
                return "{" + this._describeIncludingPrimitives(obj.key) + " => " + this._describeIncludingPrimitives(obj.value) + "}";
            return this._describeIncludingPrimitives(obj.value);
        }

        if (subtype === "internal#scopeList")
            return "Scopes[" + obj.length + "]";

        if (subtype === "internal#scope")
            return (InjectedScript.closureTypes[obj.type] || "Unknown") + (obj.name ? " (" + obj.name + ")" : "");

        return className;
    },

    /**
     * @param {*} value
     * @return {string}
     */
    _describeIncludingPrimitives: function(value)
    {
        if (typeof value === "string")
            return "\"" + value.replace(/\n/g, "\u21B5") + "\"";
        if (value === null)
            return "" + value;
        return this.isPrimitiveValue(value) ? toStringDescription(value) : (this._describe(value) || "");
    },

    /**
     * @param {boolean} enabled
     */
    setCustomObjectFormatterEnabled: function(enabled)
    {
        this._customObjectFormatterEnabled = enabled;
    }
}

/**
 * @type {!InjectedScript}
 * @const
 */
var injectedScript = new InjectedScript();

/**
 * @constructor
 * @param {*} object
 * @param {string=} objectGroupName
 * @param {boolean=} doNotBind
 * @param {boolean=} forceValueType
 * @param {boolean=} generatePreview
 * @param {?Array.<string>=} columnNames
 * @param {boolean=} isTable
 * @param {boolean=} skipEntriesPreview
 * @param {*=} customObjectConfig
 */
InjectedScript.RemoteObject = function(object, objectGroupName, doNotBind, forceValueType, generatePreview, columnNames, isTable, skipEntriesPreview, customObjectConfig)
{
    this.type = typeof object;
    if (this.type === "undefined" && injectedScript._isHTMLAllCollection(object))
        this.type = "object";

    if (injectedScript.isPrimitiveValue(object) || object === null || forceValueType) {
        // We don't send undefined values over JSON.
        if (this.type !== "undefined")
            this.value = object;

        // Null object is object with 'null' subtype.
        if (object === null)
            this.subtype = "null";

        // Provide user-friendly number values.
        if (this.type === "number") {
            this.description = toStringDescription(object);
            switch (this.description) {
            case "NaN":
            case "Infinity":
            case "-Infinity":
            case "-0":
                delete this.value;
                this.unserializableValue = this.description;
                break;
            }
        }

        return;
    }

    if (injectedScript._shouldPassByValue(object)) {
        this.value = object;
        this.subtype = injectedScript._subtype(object);
        this.description = injectedScript._describeIncludingPrimitives(object);
        return;
    }

    object = /** @type {!Object} */ (object);

    if (!doNotBind)
        this.objectId = injectedScript._bind(object, objectGroupName);
    var subtype = injectedScript._subtype(object);
    if (subtype)
        this.subtype = subtype;
    var className = InjectedScriptHost.internalConstructorName(object);
    if (className)
        this.className = className;
    this.description = injectedScript._describe(object);

    if (generatePreview && this.type === "object") {
        if (this.subtype === "proxy")
            this.preview = this._generatePreview(InjectedScriptHost.proxyTargetValue(object), undefined, columnNames, isTable, skipEntriesPreview);
        else if (this.subtype !== "node")
            this.preview = this._generatePreview(object, undefined, columnNames, isTable, skipEntriesPreview);
    }

    if (injectedScript._customObjectFormatterEnabled) {
        var customPreview = this._customPreview(object, objectGroupName, customObjectConfig);
        if (customPreview)
            this.customPreview = customPreview;
    }
}

InjectedScript.RemoteObject.prototype = {

    /**
     * @param {*} object
     * @param {string=} objectGroupName
     * @param {*=} customObjectConfig
     * @return {?RuntimeAgent.CustomPreview}
     */
    _customPreview: function(object, objectGroupName, customObjectConfig)
    {
        /**
         * @param {!Error} error
         */
        function logError(error)
        {
            // We use user code to generate custom output for object, we can use user code for reporting error too.
            Promise.resolve().then(/* suppressBlacklist */ inspectedGlobalObject.console.error.bind(inspectedGlobalObject.console, "Custom Formatter Failed: " + error.message));
        }

        /**
         * @param {*} object
         * @param {*=} customObjectConfig
         * @return {*}
         */
        function wrap(object, customObjectConfig)
        {
            return injectedScript._wrapObject(object, objectGroupName, false, false, null, false, false, customObjectConfig);
        }

        try {
            var formatters = inspectedGlobalObject["devtoolsFormatters"];
            if (!formatters || !isArrayLike(formatters))
                return null;

            for (var i = 0; i < formatters.length; ++i) {
                try {
                    var formatted = formatters[i].header(object, customObjectConfig);
                    if (!formatted)
                        continue;

                    var hasBody = formatters[i].hasBody(object, customObjectConfig);
                    injectedScript._substituteObjectTagsInCustomPreview(objectGroupName, formatted);
                    var formatterObjectId = injectedScript._bind(formatters[i], objectGroupName);
                    var bindRemoteObjectFunctionId = injectedScript._bind(wrap, objectGroupName);
                    var result = {header: JSON.stringify(formatted), hasBody: !!hasBody, formatterObjectId: formatterObjectId, bindRemoteObjectFunctionId: bindRemoteObjectFunctionId};
                    if (customObjectConfig)
                        result["configObjectId"] = injectedScript._bind(customObjectConfig, objectGroupName);
                    return result;
                } catch (e) {
                    logError(e);
                }
            }
        } catch (e) {
            logError(e);
        }
        return null;
    },

    /**
     * @return {!RuntimeAgent.ObjectPreview} preview
     */
    _createEmptyPreview: function()
    {
        var preview = {
            type: /** @type {!RuntimeAgent.ObjectPreviewType.<string>} */ (this.type),
            description: this.description || toStringDescription(this.value),
            overflow: false,
            properties: [],
            __proto__: null
        };
        InjectedScriptHost.nullifyPrototype(preview.properties);
        if (this.subtype)
            preview.subtype = /** @type {!RuntimeAgent.ObjectPreviewSubtype.<string>} */ (this.subtype);
        return preview;
    },

    /**
     * @param {!Object} object
     * @param {?Array.<string>=} firstLevelKeys
     * @param {?Array.<string>=} secondLevelKeys
     * @param {boolean=} isTable
     * @param {boolean=} skipEntriesPreview
     * @return {!RuntimeAgent.ObjectPreview} preview
     */
    _generatePreview: function(object, firstLevelKeys, secondLevelKeys, isTable, skipEntriesPreview)
    {
        var preview = this._createEmptyPreview();
        var firstLevelKeysCount = firstLevelKeys ? firstLevelKeys.length : 0;
        var propertiesThreshold = {
            properties: isTable ? 1000 : max(5, firstLevelKeysCount),
            indexes: isTable ? 1000 : max(100, firstLevelKeysCount),
            __proto__: null
        };
        var subtype = this.subtype;
        var primitiveString;

        try {
            var descriptors = [];
            InjectedScriptHost.nullifyPrototype(descriptors);

            // Add internal properties to preview.
            var rawInternalProperties = InjectedScriptHost.getInternalProperties(object) || [];
            var internalProperties = [];
            InjectedScriptHost.nullifyPrototype(rawInternalProperties);
            InjectedScriptHost.nullifyPrototype(internalProperties);
            var entries = null;
            for (var i = 0; i < rawInternalProperties.length; i += 2) {
                if (rawInternalProperties[i] === "[[Entries]]") {
                    entries = /** @type {!Array<*>} */(rawInternalProperties[i + 1]);
                    continue;
                }
                if (rawInternalProperties[i] === "[[PrimitiveValue]]" && typeof rawInternalProperties[i + 1] === 'string')
                    primitiveString = rawInternalProperties[i + 1];
                var internalPropertyDescriptor = {
                    name: rawInternalProperties[i],
                    value: rawInternalProperties[i + 1],
                    isOwn: true,
                    enumerable: true,
                    __proto__: null
                };
                push(descriptors, internalPropertyDescriptor);
            }
            var naturalDescriptors = injectedScript._propertyDescriptors(object, addPropertyIfNeeded, false /* ownProperties */, undefined /* accessorPropertiesOnly */, firstLevelKeys);
            for (var i = 0; i < naturalDescriptors.length; i++)
                push(descriptors, naturalDescriptors[i]);

            this._appendPropertyPreviewDescriptors(preview, descriptors, secondLevelKeys, isTable);

            if (subtype === "map" || subtype === "set" || subtype === "weakmap" || subtype === "weakset" || subtype === "iterator")
                this._appendEntriesPreview(entries, preview, skipEntriesPreview);

        } catch (e) {}

        return preview;

        /**
         * @param {!Array<!Object>} descriptors
         * @param {!Object} descriptor
         * @return {boolean}
         */
        function addPropertyIfNeeded(descriptors, descriptor) {
            if (descriptor.wasThrown)
                return true;

            // Ignore __proto__ property.
            if (descriptor.name === "__proto__")
                return true;

            // Ignore length property of array.
            if ((subtype === "array" || subtype === "typedarray") && descriptor.name === "length")
                return true;

            // Ignore size property of map, set.
            if ((subtype === "map" || subtype === "set") && descriptor.name === "size")
                return true;

            // Ignore ArrayBuffer previews
            if (subtype === 'arraybuffer' && (descriptor.name === "[[Int8Array]]" || descriptor.name === "[[Uint8Array]]" || descriptor.name === "[[Int16Array]]" || descriptor.name === "[[Int32Array]]"))
                return true;

            // Never preview prototype properties.
            if (!descriptor.isOwn)
                return true;

            // Ignore computed properties unless they have getters.
            if (!("value" in descriptor) && !descriptor.get)
                return true;

            // Ignore index properties when there is a primitive string.
            if (primitiveString && primitiveString[descriptor.name] === descriptor.value)
                return true;

            if (toString(descriptor.name >>> 0) === descriptor.name)
                propertiesThreshold.indexes--;
            else
                propertiesThreshold.properties--;

            var canContinue = propertiesThreshold.indexes >= 0 && propertiesThreshold.properties >= 0;
            if (!canContinue) {
                preview.overflow = true;
                return false;
            }
            push(descriptors, descriptor);
            return true;
        }
    },

    /**
     * @param {!RuntimeAgent.ObjectPreview} preview
     * @param {!Array.<*>|!Iterable.<*>} descriptors
     * @param {?Array.<string>=} secondLevelKeys
     * @param {boolean=} isTable
     */
    _appendPropertyPreviewDescriptors: function(preview, descriptors, secondLevelKeys, isTable)
    {
        for (var i = 0; i < descriptors.length; ++i) {
            var descriptor = descriptors[i];
            var name = descriptor.name;
            var value = descriptor.value;
            var type = typeof value;

            // Special-case HTMLAll.
            if (type === "undefined" && injectedScript._isHTMLAllCollection(value))
                type = "object";

            // Ignore computed properties unless they have getters.
            if (descriptor.get && !("value" in descriptor)) {
                push(preview.properties, { name: name, type: "accessor", __proto__: null });
                continue;
            }

            // Render own properties.
            if (value === null) {
                push(preview.properties, { name: name, type: "object", subtype: "null", value: "null", __proto__: null });
                continue;
            }

            var maxLength = 100;
            if (InjectedScript.primitiveTypes[type]) {
                if (type === "string" && value.length > maxLength)
                    value = this._abbreviateString(value, maxLength, true);
                push(preview.properties, { name: name, type: type, value: toStringDescription(value), __proto__: null });
                continue;
            }

            var property = { name: name, type: type, __proto__: null };
            var subtype = injectedScript._subtype(value);
            if (subtype)
                property.subtype = subtype;

            if (secondLevelKeys === null || secondLevelKeys) {
                var subPreview = this._generatePreview(value, secondLevelKeys || undefined, undefined, isTable);
                property.valuePreview = subPreview;
                if (subPreview.overflow)
                    preview.overflow = true;
            } else {
                var description = "";
                if (type !== "function")
                    description = this._abbreviateString(/** @type {string} */ (injectedScript._describe(value)), maxLength, subtype === "regexp");
                property.value = description;
            }
            push(preview.properties, property);
        }
    },

    /**
     * @param {?Array<*>} entries
     * @param {!RuntimeAgent.ObjectPreview} preview
     * @param {boolean=} skipEntriesPreview
     */
    _appendEntriesPreview: function(entries, preview, skipEntriesPreview)
    {
        if (!entries)
            return;
        if (skipEntriesPreview) {
            if (entries.length)
                preview.overflow = true;
            return;
        }
        preview.entries = [];
        InjectedScriptHost.nullifyPrototype(preview.entries);
        var entriesThreshold = 5;
        for (var i = 0; i < entries.length; ++i) {
            if (preview.entries.length >= entriesThreshold) {
                preview.overflow = true;
                break;
            }
            var entry = entries[i];
            InjectedScriptHost.nullifyPrototype(entry);
            var previewEntry = {
                value: generateValuePreview(entry.value),
                __proto__: null
            };
            if ("key" in entry)
                previewEntry.key = generateValuePreview(entry.key);
            push(preview.entries, previewEntry);
        }

        /**
         * @param {*} value
         * @return {!RuntimeAgent.ObjectPreview}
         */
        function generateValuePreview(value)
        {
            var remoteObject = new InjectedScript.RemoteObject(value, undefined, true, undefined, true, undefined, undefined, true);
            var valuePreview = remoteObject.preview || remoteObject._createEmptyPreview();
            return valuePreview;
        }
    },

    /**
     * @param {string} string
     * @param {number} maxLength
     * @param {boolean=} middle
     * @return {string}
     */
    _abbreviateString: function(string, maxLength, middle)
    {
        if (string.length <= maxLength)
            return string;
        if (middle) {
            var leftHalf = maxLength >> 1;
            var rightHalf = maxLength - leftHalf - 1;
            return string.substr(0, leftHalf) + "\u2026" + string.substr(string.length - rightHalf, rightHalf);
        }
        return string.substr(0, maxLength) + "\u2026";
    },

    __proto__: null
}

return injectedScript;
})
