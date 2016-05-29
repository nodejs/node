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
 */
(function (InjectedScriptHost, inspectedGlobalObject, injectedScriptId) {

/**
 * Protect against Object overwritten by the user code.
 * @suppress {duplicate}
 */
var Object = /** @type {function(new:Object, *=)} */ ({}.constructor);

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
 * @param {(!Arguments.<T>|!NodeList)} array
 * @param {number=} index
 * @return {!Array.<T>}
 * @template T
 */
function slice(array, index)
{
    var result = [];
    for (var i = index || 0, j = 0; i < array.length; ++i, ++j)
        result[j] = array[i];
    return result;
}

/**
 * @param {*} obj
 * @return {string}
 * @suppress {uselessCode}
 */
function toString(obj)
{
    // We don't use String(obj) because String16 could be overridden.
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
 * @param {T} obj
 * @return {T}
 * @template T
 */
function nullifyObjectProto(obj)
{
    if (obj && typeof obj === "object")
        obj.__proto__ = null;
    return obj;
}

/**
 * @param {number|string} obj
 * @return {boolean}
 */
function isUInt32(obj)
{
    if (typeof obj === "number")
        return obj >>> 0 === obj && (obj > 0 || 1 / obj > 0);
    return "" + (obj >>> 0) === obj;
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
    try {
        if (typeof obj.splice === "function") {
            if (!InjectedScriptHost.suppressWarningsAndCallFunction(Object.prototype.hasOwnProperty, obj, ["length"]))
                return false;
            var len = obj.length;
            return typeof len === "number" && isUInt32(len);
        }
    } catch (e) {
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
var domAttributesWithObservableSideEffectOnGet = nullifyObjectProto({});
domAttributesWithObservableSideEffectOnGet["Request"] = nullifyObjectProto({});
domAttributesWithObservableSideEffectOnGet["Request"]["body"] = true;
domAttributesWithObservableSideEffectOnGet["Response"] = nullifyObjectProto({});
domAttributesWithObservableSideEffectOnGet["Response"]["body"] = true;

/**
 * @param {!Object} object
 * @param {string} attribute
 * @return {boolean}
 */
function doesAttributeHaveObservableSideEffectOnGet(object, attribute)
{
    for (var interfaceName in domAttributesWithObservableSideEffectOnGet) {
        var isInstance = InjectedScriptHost.suppressWarningsAndCallFunction(function(object, interfaceName) {
            return /* suppressBlacklist */ typeof inspectedGlobalObject[interfaceName] === "function" && object instanceof inspectedGlobalObject[interfaceName];
        }, null, [object, interfaceName]);
        if (isInstance) {
            return attribute in domAttributesWithObservableSideEffectOnGet[interfaceName];
        }
    }
    return false;
}

/**
 * @constructor
 */
var InjectedScript = function()
{
}

/**
 * @type {!Object.<string, boolean>}
 * @const
 */
InjectedScript.primitiveTypes = {
    "undefined": true,
    "boolean": true,
    "number": true,
    "string": true,
    __proto__: null
}

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
     * @param {string} groupName
     * @param {boolean} canAccessInspectedGlobalObject
     * @param {boolean} forceValueType
     * @param {boolean} generatePreview
     * @return {!RuntimeAgent.RemoteObject}
     */
    wrapObject: function(object, groupName, canAccessInspectedGlobalObject, forceValueType, generatePreview)
    {
        if (canAccessInspectedGlobalObject)
            return this._wrapObject(object, groupName, forceValueType, generatePreview);
        return this._fallbackWrapper(object);
    },

    /**
     * @param {!Array<!Object>} array
     * @param {string} property
     * @param {string} groupName
     * @param {boolean} canAccessInspectedGlobalObject
     * @param {boolean} forceValueType
     * @param {boolean} generatePreview
     */
    wrapPropertyInArray: function(array, property, groupName, canAccessInspectedGlobalObject, forceValueType, generatePreview)
    {
        for (var i = 0; i < array.length; ++i) {
            if (typeof array[i] === "object" && property in array[i])
                array[i][property] = this.wrapObject(array[i][property], groupName, canAccessInspectedGlobalObject, forceValueType, generatePreview);
        }
    },

    /**
     * @param {!Array<*>} array
     * @param {string} groupName
     * @param {boolean} canAccessInspectedGlobalObject
     * @param {boolean} forceValueType
     * @param {boolean} generatePreview
     */
    wrapObjectsInArray: function(array, groupName, canAccessInspectedGlobalObject, forceValueType, generatePreview)
    {
        for (var i = 0; i < array.length; ++i)
            array[i] = this.wrapObject(array[i], groupName, canAccessInspectedGlobalObject, forceValueType, generatePreview);
    },

    /**
     * @param {*} object
     * @return {!RuntimeAgent.RemoteObject}
     */
    _fallbackWrapper: function(object)
    {
        var result = { __proto__: null };
        result.type = typeof object;
        if (this.isPrimitiveValue(object))
            result.value = object;
        else
            result.description = toString(object);
        return /** @type {!RuntimeAgent.RemoteObject} */ (result);
    },

    /**
     * @param {boolean} canAccessInspectedGlobalObject
     * @param {!Object} table
     * @param {!Array.<string>|string|boolean} columns
     * @return {!RuntimeAgent.RemoteObject}
     */
    wrapTable: function(canAccessInspectedGlobalObject, table, columns)
    {
        if (!canAccessInspectedGlobalObject)
            return this._fallbackWrapper(table);
        var columnNames = null;
        if (typeof columns === "string")
            columns = [columns];
        if (InjectedScriptHost.subtype(columns) === "array") {
            columnNames = [];
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
        var descriptors = [];
        var iter = this._propertyDescriptors(object, ownProperties, accessorPropertiesOnly, undefined);
        // Go over properties, wrap object values.
        for (var descriptor of iter) {
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
            push(descriptors, descriptor);
        }
        return descriptors;
    },

    /**
     * @param {!Object} object
     * @param {boolean=} ownProperties
     * @param {boolean=} accessorPropertiesOnly
     * @param {?Array.<string>=} propertyNamesOnly
     */
    _propertyDescriptors: function*(object, ownProperties, accessorPropertiesOnly, propertyNamesOnly)
    {
        var propertyProcessed = { __proto__: null };

        /**
         * @param {?Object} o
         * @param {!Iterable<string|symbol|number>|!Array<string|number|symbol>} properties
         */
        function* process(o, properties)
        {
            for (var property of properties) {
                var name;
                if (isSymbol(property))
                    name = /** @type {string} */ (injectedScript._describe(property));
                else
                    name = typeof property === "number" ? ("" + property) : /** @type {string} */(property);

                if (propertyProcessed[property])
                    continue;

                try {
                    propertyProcessed[property] = true;
                    var descriptor = nullifyObjectProto(InjectedScriptHost.suppressWarningsAndCallFunction(Object.getOwnPropertyDescriptor, Object, [o, property]));
                    if (descriptor) {
                        if (accessorPropertiesOnly && !("get" in descriptor || "set" in descriptor))
                            continue;
                        if ("get" in descriptor && "set" in descriptor && name != "__proto__" && InjectedScriptHost.formatAccessorsAsProperties(object) && !doesAttributeHaveObservableSideEffectOnGet(object, name)) {
                            descriptor.value = InjectedScriptHost.suppressWarningsAndCallFunction(function(attribute) { return this[attribute]; }, object, [property]);
                            descriptor.isOwn = true;
                            delete descriptor.get;
                            delete descriptor.set;
                        }
                    } else {
                        // Not all bindings provide proper descriptors. Fall back to the writable, configurable property.
                        if (accessorPropertiesOnly)
                            continue;
                        try {
                            descriptor = { name: name, value: o[property], writable: false, configurable: false, enumerable: false, __proto__: null };
                            if (o === object)
                                descriptor.isOwn = true;
                            yield descriptor;
                        } catch (e) {
                            // Silent catch.
                        }
                        continue;
                    }
                } catch (e) {
                    if (accessorPropertiesOnly)
                        continue;
                    var descriptor = { __proto__: null };
                    descriptor.value = e;
                    descriptor.wasThrown = true;
                }

                descriptor.name = name;
                if (o === object)
                    descriptor.isOwn = true;
                if (isSymbol(property))
                    descriptor.symbol = property;
                yield descriptor;
            }
        }

        if (propertyNamesOnly) {
            for (var i = 0; i < propertyNamesOnly.length; ++i) {
                var name = propertyNamesOnly[i];
                for (var o = object; this._isDefined(o); o = InjectedScriptHost.prototype(o)) {
                    if (InjectedScriptHost.suppressWarningsAndCallFunction(Object.prototype.hasOwnProperty, o, [name])) {
                        for (var descriptor of process(o, [name]))
                            yield descriptor;
                        break;
                    }
                    if (ownProperties)
                        break;
                }
            }
            return;
        }

        /**
         * @param {number} length
         */
        function* arrayIndexNames(length)
        {
            for (var i = 0; i < length; ++i)
                yield "" + i;
        }

        var skipGetOwnPropertyNames;
        try {
            skipGetOwnPropertyNames = InjectedScriptHost.isTypedArray(object) && object.length > 500000;
        } catch (e) {
        }

        for (var o = object; this._isDefined(o); o = InjectedScriptHost.prototype(o)) {
            if (InjectedScriptHost.subtype(o) === "proxy")
                continue;
            if (skipGetOwnPropertyNames && o === object) {
                // Avoid OOM crashes from getting all own property names of a large TypedArray.
                for (var descriptor of process(o, arrayIndexNames(o.length)))
                    yield descriptor;
            } else {
                // First call Object.keys() to enforce ordering of the property descriptors.
                for (var descriptor of process(o, Object.keys(/** @type {!Object} */ (o))))
                    yield descriptor;
                for (var descriptor of process(o, Object.getOwnPropertyNames(/** @type {!Object} */ (o))))
                    yield descriptor;
            }
            if (Object.getOwnPropertySymbols) {
                for (var descriptor of process(o, Object.getOwnPropertySymbols(/** @type {!Object} */ (o))))
                    yield descriptor;
            }
            if (ownProperties) {
                var proto = InjectedScriptHost.prototype(o);
                if (proto && !accessorPropertiesOnly)
                    yield { name: "__proto__", value: proto, writable: true, configurable: true, enumerable: false, isOwn: true, __proto__: null };
                break;
            }
        }
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
     * @param {!Object} nativeCommandLineAPI
     * @return {!Object}
     */
    installCommandLineAPI: function(nativeCommandLineAPI)
    {
        // NOTE: This list contains only not native Command Line API methods. For full list: V8Console.
        // NOTE: Argument names of these methods will be printed in the console, so use pretty names!
        var members = [ "$", "$$", "$x", "monitorEvents", "unmonitorEvents", "getEventListeners" ];
        for (var member of members)
            nativeCommandLineAPI[member] = CommandLineAPIImpl[member];
        var functionToStringMap = new Map([
            ["$",          "function $(selector, [startNode]) { [Command Line API] }"],
            ["$$",         "function $$(selector, [startNode]) { [Command Line API] }"],
            ["$x",         "function $x(xpath, [startNode]) { [Command Line API] }"],
            ["monitorEvents",   "function monitorEvents(object, [types]) { [Command Line API] }"],
            ["unmonitorEvents", "function unmonitorEvents(object, [types]) { [Command Line API] }"],
            ["getEventListeners", "function getEventListeners(node) { [Command Line API] }"]
        ]);
        for (let entry of functionToStringMap)
            nativeCommandLineAPI[entry[0]].toString = (() => entry[1]);
        return nativeCommandLineAPI;
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
            var description = obj.nodeName.toLowerCase();
            switch (obj.nodeType) {
            case 1 /* Node.ELEMENT_NODE */:
                description += obj.id ? "#" + obj.id : "";
                var className = obj.className;
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
        if (subtype === "array") {
            if (typeof obj.length === "number")
                className += "[" + obj.length + "]";
            return className;
        }

        if (typeof obj === "function")
            return toString(obj);

        if (isSymbol(obj)) {
            try {
                return /** @type {string} */ (InjectedScriptHost.suppressWarningsAndCallFunction(Symbol.prototype.toString, obj)) || "Symbol";
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

        return className;
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
            // Override "value" property for values that can not be JSON-stringified.
            switch (this.description) {
            case "NaN":
            case "Infinity":
            case "-Infinity":
            case "-0":
                this.value = this.description;
                break;
            }
        }

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
            Promise.resolve().then(inspectedGlobalObject.console.error.bind(inspectedGlobalObject.console, "Custom Formatter Failed: " + error.message));
        }

        /**
         * @suppressReceiverCheck
         * @param {*} object
         * @param {*=} customObjectConfig
         * @return {*}
         */
        function wrap(object, customObjectConfig)
        {
            return InjectedScriptHost.suppressWarningsAndCallFunction(injectedScript._wrapObject, injectedScript, [ object, objectGroupName, false, false, null, false, false, customObjectConfig ]);
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

        try {
            var descriptors = injectedScript._propertyDescriptors(object, undefined, undefined, firstLevelKeys);

            this._appendPropertyDescriptors(preview, descriptors, propertiesThreshold, secondLevelKeys, isTable);
            if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0)
                return preview;

            // Add internal properties to preview.
            var rawInternalProperties = InjectedScriptHost.getInternalProperties(object) || [];
            var internalProperties = [];
            for (var i = 0; i < rawInternalProperties.length; i += 2) {
                push(internalProperties, {
                    name: rawInternalProperties[i],
                    value: rawInternalProperties[i + 1],
                    isOwn: true,
                    enumerable: true,
                    __proto__: null
                });
            }
            this._appendPropertyDescriptors(preview, internalProperties, propertiesThreshold, secondLevelKeys, isTable);

            if (this.subtype === "map" || this.subtype === "set" || this.subtype === "iterator")
                this._appendEntriesPreview(object, preview, skipEntriesPreview);

        } catch (e) {}

        return preview;
    },

    /**
     * @param {!RuntimeAgent.ObjectPreview} preview
     * @param {!Array.<*>|!Iterable.<*>} descriptors
     * @param {!Object} propertiesThreshold
     * @param {?Array.<string>=} secondLevelKeys
     * @param {boolean=} isTable
     */
    _appendPropertyDescriptors: function(preview, descriptors, propertiesThreshold, secondLevelKeys, isTable)
    {
        for (var descriptor of descriptors) {
            if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0)
                break;
            if (!descriptor || descriptor.wasThrown)
                continue;

            var name = descriptor.name;

            // Ignore __proto__ property.
            if (name === "__proto__")
                continue;

            // Ignore length property of array.
            if (this.subtype === "array" && name === "length")
                continue;

            // Ignore size property of map, set.
            if ((this.subtype === "map" || this.subtype === "set") && name === "size")
                continue;

            // Never preview prototype properties.
            if (!descriptor.isOwn)
                continue;

            // Ignore computed properties.
            if (!("value" in descriptor))
                continue;

            var value = descriptor.value;
            var type = typeof value;

            // Never render functions in object preview.
            if (type === "function" && (this.subtype !== "array" || !isUInt32(name)))
                continue;

            // Special-case HTMLAll.
            if (type === "undefined" && injectedScript._isHTMLAllCollection(value))
                type = "object";

            // Render own properties.
            if (value === null) {
                this._appendPropertyPreview(preview, { name: name, type: "object", subtype: "null", value: "null", __proto__: null }, propertiesThreshold);
                continue;
            }

            var maxLength = 100;
            if (InjectedScript.primitiveTypes[type]) {
                if (type === "string" && value.length > maxLength)
                    value = this._abbreviateString(value, maxLength, true);
                this._appendPropertyPreview(preview, { name: name, type: type, value: toStringDescription(value), __proto__: null }, propertiesThreshold);
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
            this._appendPropertyPreview(preview, property, propertiesThreshold);
        }
    },

    /**
     * @param {!RuntimeAgent.ObjectPreview} preview
     * @param {!Object} property
     * @param {!Object} propertiesThreshold
     */
    _appendPropertyPreview: function(preview, property, propertiesThreshold)
    {
        if (toString(property.name >>> 0) === property.name)
            propertiesThreshold.indexes--;
        else
            propertiesThreshold.properties--;
        if (propertiesThreshold.indexes < 0 || propertiesThreshold.properties < 0) {
            preview.overflow = true;
        } else {
            push(preview.properties, property);
        }
    },

    /**
     * @param {!Object} object
     * @param {!RuntimeAgent.ObjectPreview} preview
     * @param {boolean=} skipEntriesPreview
     */
    _appendEntriesPreview: function(object, preview, skipEntriesPreview)
    {
        var entries = InjectedScriptHost.collectionEntries(object);
        if (!entries)
            return;
        if (skipEntriesPreview) {
            if (entries.length)
                preview.overflow = true;
            return;
        }
        preview.entries = [];
        var entriesThreshold = 5;
        for (var i = 0; i < entries.length; ++i) {
            if (preview.entries.length >= entriesThreshold) {
                preview.overflow = true;
                break;
            }
            var entry = nullifyObjectProto(entries[i]);
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

var CommandLineAPIImpl = { __proto__: null }

/**
 * @param {string} selector
 * @param {!Node=} opt_startNode
 * @return {*}
 */
CommandLineAPIImpl.$ = function (selector, opt_startNode)
{
    if (CommandLineAPIImpl._canQuerySelectorOnNode(opt_startNode))
        return opt_startNode.querySelector(selector);

    return inspectedGlobalObject.document.querySelector(selector);
}

/**
 * @param {string} selector
 * @param {!Node=} opt_startNode
 * @return {*}
 */
CommandLineAPIImpl.$$ = function (selector, opt_startNode)
{
    if (CommandLineAPIImpl._canQuerySelectorOnNode(opt_startNode))
        return slice(opt_startNode.querySelectorAll(selector));
    return slice(inspectedGlobalObject.document.querySelectorAll(selector));
}

/**
 * @param {!Node=} node
 * @return {boolean}
 */
CommandLineAPIImpl._canQuerySelectorOnNode = function(node)
{
    return !!node && InjectedScriptHost.subtype(node) === "node" && (node.nodeType === Node.ELEMENT_NODE || node.nodeType === Node.DOCUMENT_NODE || node.nodeType === Node.DOCUMENT_FRAGMENT_NODE);
}

/**
 * @param {string} xpath
 * @param {!Node=} opt_startNode
 * @return {*}
 */
CommandLineAPIImpl.$x = function(xpath, opt_startNode)
{
    var doc = (opt_startNode && opt_startNode.ownerDocument) || inspectedGlobalObject.document;
    var result = doc.evaluate(xpath, opt_startNode || doc, null, XPathResult.ANY_TYPE, null);
    switch (result.resultType) {
    case XPathResult.NUMBER_TYPE:
        return result.numberValue;
    case XPathResult.STRING_TYPE:
        return result.stringValue;
    case XPathResult.BOOLEAN_TYPE:
        return result.booleanValue;
    default:
        var nodes = [];
        var node;
        while (node = result.iterateNext())
            push(nodes, node);
        return nodes;
    }
}

/**
 * @param {!Object} object
 * @param {!Array.<string>|string=} opt_types
 */
CommandLineAPIImpl.monitorEvents = function(object, opt_types)
{
    if (!object || !object.addEventListener || !object.removeEventListener)
        return;
    var types = CommandLineAPIImpl._normalizeEventTypes(opt_types);
    for (var i = 0; i < types.length; ++i) {
        object.removeEventListener(types[i], CommandLineAPIImpl._logEvent, false);
        object.addEventListener(types[i], CommandLineAPIImpl._logEvent, false);
    }
}

/**
 * @param {!Object} object
 * @param {!Array.<string>|string=} opt_types
 */
CommandLineAPIImpl.unmonitorEvents = function(object, opt_types)
{
    if (!object || !object.addEventListener || !object.removeEventListener)
        return;
    var types = CommandLineAPIImpl._normalizeEventTypes(opt_types);
    for (var i = 0; i < types.length; ++i)
        object.removeEventListener(types[i], CommandLineAPIImpl._logEvent, false);
}

/**
 * @param {!Node} node
 * @return {!Object|undefined}
 */
CommandLineAPIImpl.getEventListeners = function(node)
{
    var result = nullifyObjectProto(InjectedScriptHost.getEventListeners(node));
    if (!result)
        return;

    // TODO(dtapuska): Remove this one closure compiler is updated
    // to handle EventListenerOptions and passive event listeners
    // has shipped. Don't JSDoc these otherwise it will fail.
    // @param {boolean} capture
    // @param {boolean} passive
    // @return {boolean|undefined|{capture: (boolean|undefined), passive: boolean}}
    function eventListenerOptions(capture, passive)
    {
        return {"capture": capture, "passive": passive};
    }

    /**
     * @param {!Node} node
     * @param {string} type
     * @param {function()} listener
     * @param {boolean} capture
     * @param {boolean} passive
     */
    function removeEventListenerWrapper(node, type, listener, capture, passive)
    {
        node.removeEventListener(type, listener, eventListenerOptions(capture, passive));
    }

    /** @this {{type: string, listener: function(), useCapture: boolean, passive: boolean}} */
    var removeFunc = function()
    {
        removeEventListenerWrapper(node, this.type, this.listener, this.useCapture, this.passive);
    }
    for (var type in result) {
        var listeners = result[type];
        for (var i = 0, listener; listener = listeners[i]; ++i) {
            listener["type"] = type;
            listener["remove"] = removeFunc;
        }
    }
    return result;
}

/**
 * @param {!Array.<string>|string=} types
 * @return {!Array.<string>}
 */
CommandLineAPIImpl._normalizeEventTypes = function(types)
{
    if (typeof types === "undefined")
        types = ["mouse", "key", "touch", "pointer", "control", "load", "unload", "abort", "error", "select", "input", "change", "submit", "reset", "focus", "blur", "resize", "scroll", "search", "devicemotion", "deviceorientation"];
    else if (typeof types === "string")
        types = [types];

    var result = [];
    for (var i = 0; i < types.length; ++i) {
        if (types[i] === "mouse")
            push(result, "click", "dblclick", "mousedown", "mouseeenter", "mouseleave", "mousemove", "mouseout", "mouseover", "mouseup", "mouseleave", "mousewheel");
        else if (types[i] === "key")
            push(result, "keydown", "keyup", "keypress", "textInput");
        else if (types[i] === "touch")
            push(result, "touchstart", "touchmove", "touchend", "touchcancel");
        else if (types[i] === "pointer")
            push(result, "pointerover", "pointerout", "pointerenter", "pointerleave", "pointerdown", "pointerup", "pointermove", "pointercancel", "gotpointercapture", "lostpointercapture");
        else if (types[i] === "control")
            push(result, "resize", "scroll", "zoom", "focus", "blur", "select", "input", "change", "submit", "reset");
        else
            push(result, types[i]);
    }
    return result;
}

/**
 * @param {!Event} event
 */
CommandLineAPIImpl._logEvent = function(event)
{
    inspectedGlobalObject.console.log(event.type, event);
}

return injectedScript;
})
