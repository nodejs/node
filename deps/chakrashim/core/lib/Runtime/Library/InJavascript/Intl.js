//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

"use strict";
// Core intl lib
(function (EngineInterface, InitType) {
    var platform = EngineInterface.Intl;

    // Built-Ins
    var setPrototype = platform.builtInSetPrototype;
    var getArrayLength = platform.builtInGetArrayLength;
    var callInstanceFunc = platform.builtInCallInstanceFunction;

    var Boolean = platform.Boolean;
    var Object = platform.Object;
    var RegExp = platform.RegExp;
    var Number = platform.Number;
    var String = platform.String;
    var Date = platform.Date;
    var Error = platform.Error;

    var RaiseAssert = platform.raiseAssert;

    var Math = setPrototype({
        abs: platform.builtInMathAbs,
        floor: platform.builtInMathFloor,
        max: platform.builtInMathMax,
        pow: platform.builtInMathPow
    }, null);

    var objectDefineProperty = platform.builtInJavascriptObjectEntryDefineProperty;
    var ObjectGetPrototypeOf = platform.builtInJavascriptObjectEntryGetPrototypeOf;
    var ObjectIsExtensible = platform.builtInJavascriptObjectEntryIsExtensible;
    var ObjectGetOwnPropertyNames = platform.builtInJavascriptObjectEntryGetOwnPropertyNames;
    var ObjectInstanceHasOwnProperty = platform.builtInJavascriptObjectEntryHasOwnProperty;

    // Because we don't keep track of the attributes object, and neither does the internals of Object.defineProperty;
    // We don't need to restore it's prototype.
    var ObjectDefineProperty = function (obj, prop, attributes) {
        objectDefineProperty(obj, prop, setPrototype(attributes, null));
    };

    var ArrayInstanceForEach = platform.builtInJavascriptArrayEntryForEach;
    var ArrayInstanceIndexOf = platform.builtInJavascriptArrayEntryIndexOf;
    var ArrayInstancePush = platform.builtInJavascriptArrayEntryPush;
    var ArrayInstanceJoin = platform.builtInJavascriptArrayEntryJoin;

    var FunctionInstanceBind = platform.builtInJavascriptFunctionEntryBind;
    var DateInstanceGetDate = platform.builtInJavascriptDateEntryGetDate;
    var DateNow = platform.builtInJavascriptDateEntryNow;

    var StringInstanceReplace = platform.builtInJavascriptStringEntryReplace;
    var StringInstanceToLowerCase = platform.builtInJavascriptStringEntryToLowerCase;
    var StringInstanceToUpperCase = platform.builtInJavascriptStringEntryToUpperCase;

    var ObjectPrototype = ObjectGetPrototypeOf({});

    var isFinite = platform.builtInGlobalObjectEntryIsFinite;
    var isNaN = platform.builtInGlobalObjectEntryIsNaN;

    var forEach = function (obj, length, func) {
        var current = 0;

        while (current < length) {
            func(obj[current]);
            current++;
        }
    };

    // A helper function that is meant to rethrow SOE and OOM exceptions allowing them to propagate.
    var throwExIfOOMOrSOE = function (ex) {
        if (ex.number === -2146828260 || ex.number === -2146828281) {
            throw ex;
        }
    };

    var tagPublicFunction = function (name, f) {
        return platform.tagPublicLibraryCode(f, name);
    };

    var resolveLocaleBestFit = function (locale, defaultLocale) {
        var resolvedLocale = platform.resolveLocaleBestFit(locale);

        if (defaultLocale === locale) return resolvedLocale;
        else if (defaultLocale === resolvedLocale) return undefined;
        return resolvedLocale;
    }

    var resolveLocaleHelper = function (locale, fitter, extensionFilter, defaultLocale) {
        var subTags = platform.getExtensions(locale);
        if (subTags) {
            callInstanceFunc(ArrayInstanceForEach, subTags, (function (subTag) { locale = callInstanceFunc(StringInstanceReplace, locale, "-" + subTag, ""); }));
        }

        // Instead of using replace, we will match two groups, one capturing, one not. The non capturing group just strips away -u if present.
        // We are substituting for the function replace; which will only make a change if /-u$/ was found (-u at the end of the line)
        // And because match will return null if we don't match entire sequence, we are using the two groups stated above.
        locale = platform.builtInRegexMatch(locale, /(.*?)(?:-u)?$/)[1];
        var resolved = fitter(locale, defaultLocale);

        if (extensionFilter !== undefined) { // Filter to expected sub-tags
            var filtered = [];
            callInstanceFunc(ArrayInstanceForEach, subTags, (function (subTag) {
                var parts = platform.builtInRegexMatch(subTag, /([^-]*)-?(.*)?/); // [0] entire thing; [1] key; [2] value
                var key = parts[1];
                if (callInstanceFunc(ArrayInstanceIndexOf, extensionFilter, key) !== -1) {
                    callInstanceFunc(ArrayInstancePush, filtered, subTag);
                }
            }));
            subTags = filtered;
        }

        return setPrototype({
            locale: resolved ? (resolved + ((subTags && getArrayLength(subTags) > 0) ? "-u-" : "") + callInstanceFunc(ArrayInstanceJoin, subTags, "-")) : undefined,
            subTags: subTags,
            localeWithoutSubtags: resolved
        }, null);
    }

    var resolveLocales = function (givenLocales, matcher, extensionFilter, defaultLocaleFunc) {
        var fitter = matcher === "lookup" ? platform.resolveLocaleLookup : resolveLocaleBestFit;
        var length = getArrayLength(givenLocales);

        var defaultLocale = defaultLocaleFunc();

        length = length !== undefined ? length : 0;
        for (var i = 0; i < length; i++) {
            var resolved = resolveLocaleHelper(givenLocales[i], fitter, extensionFilter, defaultLocale);
            if (resolved.locale !== undefined) {
                return resolved;
            }
        }
        return resolveLocaleHelper(defaultLocale, fitter, undefined, defaultLocale);
    }

    var strippedDefaultLocale = function () {
        return platform.builtInRegexMatch(platform.getDefaultLocale(), /([^_]*).*/)[1];
    };

    var Internal = (function () {
        return setPrototype({
            ToObject: function (o) {
                if (o === null) {
                    platform.raiseNeedObject();
                }
                return o !== undefined ? Object(o) : undefined;
            },

            ToString: function (s) {
                return s !== undefined ? String(s) : undefined;
            },


            ToNumber: function (n) {
                return n === undefined ? NaN : Number(n);
            },

            ToLogicalBoolean: function (v) {
                return v !== undefined ? Boolean(v) : undefined;
            },

            ToUint32: function (n) {
                var num = Number(n),
                    ret = 0;
                if (!isNaN(num) && isFinite(num)) {
                    ret = Math.abs(num % Math.pow(2, 32));
                }
                return ret;
            },

            HasProperty: function (o, p) {
                // Walk the prototype chain
                while (o) {
                    if (callInstanceFunc(ObjectInstanceHasOwnProperty, o, p)) {
                        return true;
                    }
                    o = ObjectGetPrototypeOf(o);
                }
            }
        }, null)
    })();

    // Common Regexps
    var currencyCodeRE = RegExp("^[A-Z]{3}$", "i");

    // Internal ops implemented in JS:
    function GetOption(options, property, type, values, fallback) {
        var value = options[property];

        if (value !== undefined) {
            if (type == "boolean") {
                value = Internal.ToLogicalBoolean(value);
            }

            if (type == "string") {
                value = Internal.ToString(value);
            }

            if (type == "number") {
                value = Internal.ToNumber(value);
            }

            if (values !== undefined && callInstanceFunc(ArrayInstanceIndexOf, values, value) == -1) {
                platform.raiseOptionValueOutOfRange_3(String(value), String(property), "['" + callInstanceFunc(ArrayInstanceJoin, values, "', '") + "']");
            }

            return value;
        }

        return fallback;
    }

    function GetNumberOption(options, property, minimum, maximum, fallback) {
        var rawValue = options[property];

        if (typeof rawValue !== 'undefined') {
            var formattedValue = Internal.ToNumber(rawValue);

            if (isNaN(formattedValue) || formattedValue < minimum || formattedValue > maximum) {
                platform.raiseOptionValueOutOfRange_3(String(rawValue), String(property), "[" + minimum + " - " + maximum + "]");
            }

            return Math.floor(formattedValue);
        } else {
            return fallback;
        }
    }

    function IsWellFormedCurrencyCode(code) {
        code = Internal.ToString(code);

        return platform.builtInRegexMatch(code, currencyCodeRE) !== null;
    }

    // Make sure locales is an array, remove duplicate locales, make sure each locale is valid, and canonicalize each.
    function CanonicalizeLocaleList(locales) {
        if (typeof locales === 'undefined') {
            return [];
        }

        if (typeof locales === 'string') {
            locales = [locales];
        }

        locales = Internal.ToObject(locales);
        var length = Internal.ToUint32(locales.length);

        // TODO: Use sets here to prevent duplicates
        var seen = [];

        forEach(locales, length, (function (locale) {
            if ((typeof locale !== 'string' && typeof locale !== 'object') || locale === null) {
                platform.raiseNeedObjectOrString("Locale");
            }

            var tag = Internal.ToString(locale);

            if (!platform.isWellFormedLanguageTag(tag)) {
                platform.raiseLocaleNotWellFormed(String(tag));
            }

            tag = platform.normalizeLanguageTag(tag);

            if (tag !== undefined && callInstanceFunc(ArrayInstanceIndexOf, seen, tag) === -1) {
                callInstanceFunc(ArrayInstancePush, seen, tag);
            }

        }));

        return seen;
    }

    function LookupSupportedLocales(requestedLocales, fitter, defaultLocale) {
        var subset = [];
        var count = 0;
        callInstanceFunc(ArrayInstanceForEach, requestedLocales, function (locale) {
            try {
                var resolved = resolveLocaleHelper(locale, fitter, undefined, defaultLocale);
                if (resolved.locale) {
                    ObjectDefineProperty(subset, count, { value: resolved.locale, writable: false, configurable: false, enumerable: true });
                    count = count + 1;
                }
            } catch (ex) {
                throwExIfOOMOrSOE(ex);
                // Expecting an error (other than OOM or SOE), same as fitter returning undefined
            }
        });
        ObjectDefineProperty(subset, "length", { value: count, writable: false, configurable: false });
        return subset;
    }

    var supportedLocalesOfWrapper = function (that, functionName, locales, options) {
        if (that === null || that === undefined) {
            platform.raiseNotAConstructor(functionName);
        }

        var hiddenObj = platform.getHiddenObject(that);
        if (hiddenObj === undefined || hiddenObj.isValid !== "Valid") {
            platform.raiseNotAConstructor(functionName);
        }

        return supportedLocalesOf(locales, options);
    }

    // Shared among all the constructors
    var supportedLocalesOf = function (locales, options) {
        var matcher;
        locales = CanonicalizeLocaleList(locales);

        if (typeof options !== 'undefined') {
            matcher = options.localeMatcher;

            if (typeof matcher !== 'undefined') {
                matcher = Internal.ToString(matcher);

                if (matcher !== 'lookup' && matcher !== 'best fit') {
                    platform.raiseOptionValueOutOfRange_3(String(matcher), "localeMatcher", "['best fit', 'lookup']");
                }
            }
        }

        if (typeof matcher === 'undefined' || matcher === 'best fit') {
            return LookupSupportedLocales(locales, resolveLocaleBestFit, platform.normalizeLanguageTag(strippedDefaultLocale()));
        } else {
            return LookupSupportedLocales(locales, platform.resolveLocaleLookup, strippedDefaultLocale());
        }
    };

    var supportedLocalesOfThisArg = setPrototype({}, null);
    platform.setHiddenObject(supportedLocalesOfThisArg, setPrototype({ isValid: "Valid" }, null));

    // Because we need to display correct error message for each Intl type, we have these helper functions
    var collator_supportedLocalesOf_name = "Intl.Collator.supportedLocalesOf";
    var collator_supportedLocalesOf = callInstanceFunc(FunctionInstanceBind, tagPublicFunction(collator_supportedLocalesOf_name, function (locales) {
        var options = arguments.length < 2 ? undefined : arguments[1];

        return supportedLocalesOfWrapper(this, collator_supportedLocalesOf_name, locales, options);
    }), supportedLocalesOfThisArg);

    var numberFormat_supportedLocalesOf_name = "Intl.NumberFormat.supportedLocalesOf";
    var numberFormat_supportedLocalesOf = callInstanceFunc(FunctionInstanceBind, tagPublicFunction(numberFormat_supportedLocalesOf_name, function (locales) {
        var options = arguments.length < 2 ? undefined : arguments[1];

        return supportedLocalesOfWrapper(this, numberFormat_supportedLocalesOf_name, locales, options);
    }), supportedLocalesOfThisArg);

    var dateTimeFormat_supportedLocalesOf_name = "Intl.DateTimeFormat.supportedLocalesOf";
    var dateTimeFormat_supportedLocalesOf = callInstanceFunc(FunctionInstanceBind, tagPublicFunction(dateTimeFormat_supportedLocalesOf_name, function (locales) {
        var options = arguments.length < 2 ? undefined : arguments[1];

        return supportedLocalesOfWrapper(this, dateTimeFormat_supportedLocalesOf_name, locales, options);
    }), supportedLocalesOfThisArg);

    // If an empty string is encountered for the value of the property; that means that is by default.
    // So in the case of zh-TW; "default" and "stroke" are the same.
    // This list was discussed with AnBorod, AnGlass and SureshJa.
    var localesAcceptingCollationValues = setPrototype({
        "es-ES": setPrototype({ "trad": "tradnl" }, null),
        "lv-LV": setPrototype({ "trad": "tradnl" }, null),
        "de-DE": setPrototype({ "phonebk": "phoneb" }, null),
        "ja-JP": setPrototype({ "unihan": "radstr" }, null),
        "zh-TW": setPrototype({ "phonetic": "pronun", "unihan": "radstr", "stroke": "" }, null),
        "zh-HK": setPrototype({ "unihan": "radstr", "stroke": "" }, null),
        "zh-MO": setPrototype({ "unihan": "radstr", "stroke": "" }, null),
        "zh-CN": setPrototype({ "stroke": "stroke", "pinyin": "" }, null),
        "zh-SG": setPrototype({ "stroke": "stroke", "pinyin": "" }, null)

        // The following locales are supported by Windows; however, no BCP47 equivalent collation values were found for these.
        // In future releases; this list (plus most of the Collator implementation) will be changed/removed as the platform support is expected to change.
        // "hu-HU": ["technl"],
        // "ka-GE": ["modern"],
        // "x-IV": ["mathan"]
    }, null);

    var reverseLocaleAcceptingCollationValues = (function () {
        var toReturn = setPrototype({}, null);
        callInstanceFunc(ArrayInstanceForEach, ObjectGetOwnPropertyNames(localesAcceptingCollationValues), function (locale) {
            var values = localesAcceptingCollationValues[locale];
            var newValues = setPrototype({}, null);

            callInstanceFunc(ArrayInstanceForEach, ObjectGetOwnPropertyNames(values), function (bcp47Tag) {
                var windowsTag = values[bcp47Tag];
                if (windowsTag !== "") {
                    newValues[windowsTag] = bcp47Tag;
                }
            });

            toReturn[locale] = newValues;
        });
        return toReturn;
    }());

    var mappedDefaultLocale = function () {
        var parts = platform.builtInRegexMatch(platform.getDefaultLocale(), /([^_]*)_?(.+)?/);
        var locale = parts[1];
        var collation = parts[2];
        var availableBcpTags = reverseLocaleAcceptingCollationValues[locale];

        if (collation === undefined || availableBcpTags === undefined) { return locale; }

        var bcpTag = availableBcpTags[collation];
        if (bcpTag !== undefined) {
            return locale + "-u-co-" + bcpTag;
        }

        return locale;
    };

    // Intl.Collator, String.prototype.localeCompare
    var Collator = (function () {

        if (InitType === 'Intl' || InitType === 'String') {

            function InitializeCollator(collator, localeList, options) {
                if (typeof collator != "object") {
                    platform.raiseNeedObject();
                }

                if (callInstanceFunc(ObjectInstanceHasOwnProperty, collator, '__initializedIntlObject') && collator.__initializedIntlObject) {
                    platform.raiseObjectIsAlreadyInitialized("Collator", "Collator");
                }

                collator.__initializedIntlObject = true;

                // Extract options
                if (typeof options === 'undefined') {
                    options = setPrototype({}, null);
                } else {
                    options = Internal.ToObject(options);
                }

                var matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
                var usage = GetOption(options, "usage", "string", ["sort", "search"], "sort");
                var sensitivity = GetOption(options, "sensitivity", "string", ["base", "accent", "case", "variant"], undefined);
                var ignorePunctuation = GetOption(options, "ignorePunctuation", "boolean", undefined, false);
                var caseFirst = GetOption(options, "caseFirst", "string", ["upper", "lower", "false"], undefined);
                var numeric = GetOption(options, "numeric", "boolean", [true, false], undefined);

                // Deal with the locales and extensions
                localeList = CanonicalizeLocaleList(localeList);
                var resolvedLocaleInfo = resolveLocales(localeList, matcher, undefined, mappedDefaultLocale);

                var collation = "default";
                var resolvedLocaleLookup = platform.resolveLocaleLookup(resolvedLocaleInfo.localeWithoutSubtags);
                var collationAugmentedLocale = resolvedLocaleLookup;

                if (resolvedLocaleInfo.subTags) {
                    callInstanceFunc(ArrayInstanceForEach, resolvedLocaleInfo.subTags, function (subTag) {
                        var parts = platform.builtInRegexMatch(subTag, /([^-]*)-?(.*)?/); // [0] entire thing; [1] key; [2] value
                        var key = parts[1];
                        var value = parts[2] === "" ? undefined : parts[2];
                        if (key === "kf" && caseFirst === undefined) {
                            caseFirst = GetOption(setPrototype({ caseFirst: value }, null), "caseFirst", "string", ["upper", "lower", "false"], undefined);
                        } else if (key === "kn" && numeric === undefined) {
                            if (value !== undefined) {
                                numeric = Internal.ToLogicalBoolean(callInstanceFunc(StringInstanceToLowerCase, value) === "true");
                            } else {
                                numeric = true;
                            }
                        } else if (key === "co" && value !== undefined && value !== "default" && value !== "search" && value !== "sort" && value !== "standard") {
                            // Ignore these collation values as they shouldn't have any impact
                            collation = value;
                        }
                    });
                }
                if (collation !== "default") {
                    var accepedCollationForLocale = localesAcceptingCollationValues[collationAugmentedLocale];
                    var windowsCollation = "";
                    if (accepedCollationForLocale !== undefined && (windowsCollation = accepedCollationForLocale[collation]) !== undefined) {
                        if (windowsCollation !== "") {
                            collationAugmentedLocale = collationAugmentedLocale + "_" + windowsCollation;
                        }
                    }
                    else {
                        collation = "default";
                    }
                }

                // Correct options if need be.
                if (caseFirst === undefined) {
                    try {
                        var num = platform.compareString('A', 'a', resolvedLocaleLookup, undefined, undefined, undefined);
                    }
                    catch (e) {
                        // Rethrow OOM or SOE
                        throwExIfOOMOrSOE(e);

                        // Otherwise, Generic message to cover the exception throw from the CompareStringEx api.
                        // The platform's exception is also generic and in most if not all cases specifies that "a" argument is invalid.
                        // We have no other information from the platform on the cause of the exception.
                        platform.raiseOptionValueOutOfRange();
                    }
                    if (num === 0) caseFirst = 'false';
                    else if (num === -1) caseFirst = 'upper';
                    else caseFirst = 'lower';
                }

                if (sensitivity === undefined) {
                    sensitivity = "variant";
                }

                if (numeric === undefined) numeric = false;

                // Set the options on the object
                collator.__matcher = matcher;
                collator.__locale = resolvedLocaleInfo.localeWithoutSubtags;
                collator.__localeForCompare = collationAugmentedLocale;
                collator.__usage = usage;
                collator.__sensitivity = sensitivity;
                collator.__ignorePunctuation = ignorePunctuation;
                collator.__caseFirst = caseFirst;
                collator.__numeric = numeric;
                collator.__collation = collation;
                collator.__initializedCollator = true;
            }

            platform.registerBuiltInFunction(tagPublicFunction("String.prototype.localeCompare", function () {
                    var that = arguments[0];
                    if (this === undefined || this === null) {
                        platform.raiseThis_NullOrUndefined("String.prototype.localeCompare");
                    }
                    else if (that === null) {
                        platform.raiseNeedObject();
                    }
                    // ToString must be called on this/that argument before we do any other operation, as other operations in InitializeCollator may also be observable
                    var thisArg = String(this);
                    var that = String(that);
                    var stateObject = setPrototype({}, stateObject);
                    InitializeCollator(stateObject, arguments[1], arguments[2]);
                    return Number(platform.compareString(thisArg, that, stateObject.__localeForCompare, stateObject.__sensitivity, stateObject.__ignorePunctuation, stateObject.__numeric));
                }), 4);

            if (InitType === 'Intl') {

                function Collator() {
                    // The function should have length of 0, hence we are getting arguments this way.
                    var locales = undefined;
                    var options = undefined;
                    if (arguments.length >= 1) locales = arguments[0];
                    if (arguments.length >= 2) options = arguments[1];

                    if (this === Intl || this === undefined) {
                        return new Collator(locales, options);
                    }

                    var obj = Internal.ToObject(this);
                    if (!ObjectIsExtensible(obj)) {
                        platform.raiseObjectIsNonExtensible("Collator");
                    }

                    // Use the hidden object to store data
                    var hiddenObject = platform.getHiddenObject(obj);

                    if (hiddenObject === undefined) {
                        hiddenObject = setPrototype({}, null);
                        platform.setHiddenObject(obj, hiddenObject);
                    }

                    InitializeCollator(hiddenObject, locales, options);

                    // Add the bound compare
                    hiddenObject.__boundCompare = callInstanceFunc(FunctionInstanceBind, compare, obj);

                    return obj;
                }
                tagPublicFunction("Intl.Collator", Collator);

                tagPublicFunction("Intl.Collator.prototype.compare", compare);

                function compare(a, b) {
                    if (typeof this !== 'object') {
                        platform.raiseNeedObjectOfType("Collator.prototype.compare", "Collator");
                    }

                    var hiddenObject = platform.getHiddenObject(this);
                    if (hiddenObject === undefined || !hiddenObject.__initializedCollator) {
                        platform.raiseNeedObjectOfType("Collator.prototype.compare", "Collator");
                    }

                    a = String(a);
                    b = String(b);

                    return Number(platform.compareString(a, b, hiddenObject.__localeForCompare, hiddenObject.__sensitivity, hiddenObject.__ignorePunctuation, hiddenObject.__numeric));
                }

                ObjectDefineProperty(Collator, 'supportedLocalesOf', { value: collator_supportedLocalesOf, writable: true, configurable: true });

                ObjectDefineProperty(Collator, 'prototype', { value: new Collator(), writable: false, enumerable: false, configurable: false });
                setPrototype(Collator.prototype, Object.prototype);

                ObjectDefineProperty(Collator.prototype, 'constructor', { value: Collator, writable: true, enumerable: false, configurable: true });

                ObjectDefineProperty(Collator.prototype, 'resolvedOptions', {
                    value: function () {
                        if (typeof this !== 'object') {
                            platform.raiseNeedObjectOfType("Collator.prototype.resolvedOptions", "Collator");
                        }
                        var hiddenObject = platform.getHiddenObject(this);
                        if (hiddenObject === undefined || !hiddenObject.__initializedCollator) {
                            platform.raiseNeedObjectOfType("Collator.prototype.resolvedOptions", "Collator");
                        }

                        return {
                            locale: hiddenObject.__locale,
                            usage: hiddenObject.__usage,
                            sensitivity: hiddenObject.__sensitivity,
                            ignorePunctuation: hiddenObject.__ignorePunctuation,
                            collation: hiddenObject.__collation, // "co" unicode extension
                            numeric: hiddenObject.__numeric,     // "ka" unicode extension TODO: Determine if this is supported (doesn't have to be)
                            caseFirst: hiddenObject.__caseFirst  // "kf" unicode extension TODO: Determine if this is supported (doesn't have to be)
                        }
                    }, writable: true, enumerable: false, configurable: true
                });

                ObjectDefineProperty(Collator.prototype, 'compare', {
                    get: function () {
                        if (typeof this !== 'object') {
                            platform.raiseNeedObjectOfType("Collator.prototype.compare", "Collator");
                        }

                        var hiddenObject = platform.getHiddenObject(this);
                        if (hiddenObject === undefined || !hiddenObject.__initializedCollator) {
                            platform.raiseNeedObjectOfType("Collator.prototype.compare", "Collator");
                        }

                        return hiddenObject.__boundCompare;
                    }, enumerable: false, configurable: true
                });

                return Collator;
            }
        }
        // 'Init.Collator' not defined if reached here. Return 'undefined'
        return undefined;
    })();

    // Intl.NumberFormat, Number.prototype.toLocaleString
    var NumberFormat = (function () {

        if (InitType === 'Intl' || InitType === 'Number') {

            function InitializeNumberFormat(numberFormat, localeList, options) {

                if (typeof numberFormat != "object") {
                    platform.raiseNeedObject();
                }

                if (callInstanceFunc(ObjectInstanceHasOwnProperty, numberFormat, '__initializedIntlObject') && numberFormat.__initializedIntlObject) {
                    platform.raiseObjectIsAlreadyInitialized("NumberFormat", "NumberFormat");
                }
                numberFormat.__initializedIntlObject = true;

                // Extract options
                if (typeof options === 'undefined') {
                    options = setPrototype({}, null);
                } else {
                    options = Internal.ToObject(options);
                }

                var matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
                var style = GetOption(options, "style", "string", ["decimal", "percent", "currency"], "decimal");
                var formatterToUse = 0;

                var currency = GetOption(options, "currency", "string", undefined, undefined);
                var currencyDisplay = GetOption(options, 'currencyDisplay', 'string', ['code', 'symbol', 'name'], 'symbol');
                var currencyDigits = undefined;

                var minimumIntegerDigits = GetNumberOption(options, 'minimumIntegerDigits', 1, 21, 1);
                var minimumFractionDigits = undefined;
                var maximumFractionDigits = undefined;
                var maximumFractionDigitsDefault = undefined;

                var minimumSignificantDigits = options.minimumSignificantDigits;
                var maximumSignificantDigits = options.maximumSignificantDigits;

                if (typeof minimumSignificantDigits !== 'undefined' || typeof maximumSignificantDigits !== 'undefined') {
                    minimumSignificantDigits = GetNumberOption(options, 'minimumSignificantDigits', 1, 21, 1);
                    maximumSignificantDigits = GetNumberOption(options, 'maximumSignificantDigits', minimumSignificantDigits, 21, 21);
                }

                var useGrouping = GetOption(options, 'useGrouping', 'boolean', undefined, true);

                // Deal with the locales and extensions
                localeList = CanonicalizeLocaleList(localeList);
                var resolvedLocaleInfo = resolveLocales(localeList, matcher, ["nu"], strippedDefaultLocale);

                // Correct the options if necessary
                if (typeof currency !== 'undefined' && !IsWellFormedCurrencyCode(currency)) {
                    platform.raiseInvalidCurrencyCode(String(currency));
                }

                if (style === "currency") {
                    if (typeof currency === 'undefined') {
                        platform.raiseMissingCurrencyCode();
                    }
                    currency = callInstanceFunc(StringInstanceToUpperCase, currency);
                    try {
                        currencyDigits = platform.currencyDigits(currency);
                    } catch (e) {
                        throwExIfOOMOrSOE(e);
                        platform.raiseInvalidCurrencyCode(String(currency));
                    }
                    minimumFractionDigits = GetNumberOption(options, 'minimumFractionDigits', 0, 20, currencyDigits);
                    maximumFractionDigitsDefault = Math.max(currencyDigits, minimumFractionDigits);
                } else {
                    currency = undefined;
                    currencyDisplay = undefined;
                    minimumFractionDigits = GetNumberOption(options, 'minimumFractionDigits', 0, 20, 0);
                    if (style === "percent") {
                        maximumFractionDigitsDefault = Math.max(minimumFractionDigits, 0);
                    } else {
                        maximumFractionDigitsDefault = Math.max(minimumFractionDigits, 3)
                    }
                }
                maximumFractionDigits = GetNumberOption(options, 'maximumFractionDigits', minimumFractionDigits, 20, maximumFractionDigitsDefault);

                if (style === 'percent') formatterToUse = 1;
                else if (style === 'currency') formatterToUse = 2;
                else formatterToUse = 0;

                // Set the options on the object
                numberFormat.__localeMatcher = matcher;
                numberFormat.__locale = resolvedLocaleInfo.locale;
                numberFormat.__style = style;
                if (currency !== undefined) numberFormat.__currency = currency;
                if (currencyDisplay !== undefined) {
                    numberFormat.__currencyDisplay = currencyDisplay;
                    numberFormat.__currencyDisplayToUse = currencyDisplay === "symbol" ? 0 : (currencyDisplay === "code" ? 1 : 2);
                }
                numberFormat.__minimumIntegerDigits = minimumIntegerDigits;
                numberFormat.__minimumFractionDigits = minimumFractionDigits;
                numberFormat.__maximumFractionDigits = maximumFractionDigits;

                if (maximumSignificantDigits !== undefined) {
                    numberFormat.__minimumSignificantDigits = minimumSignificantDigits;
                    numberFormat.__maximumSignificantDigits = maximumSignificantDigits;
                }
                numberFormat.__formatterToUse = formatterToUse;
                numberFormat.__useGrouping = useGrouping;

                try {
                    // Cache api instance and update numbering system on the object
                    platform.cacheNumberFormat(numberFormat);
                }
                catch (e) {
                    throwExIfOOMOrSOE(e);
                    // Generic message to cover the exception throw from the platform.
                    // The platform's exception is also generic and in most if not all cases specifies that "a" argument is invalid.
                    // We have no other information from the platform on the cause of the exception.
                    platform.raiseOptionValueOutOfRange();
                }

                numberFormat.__numberingSystem = callInstanceFunc(StringInstanceToLowerCase, numberFormat.__numberingSystem);
                numberFormat.__initializedNumberFormat = true;
            }

            platform.registerBuiltInFunction(tagPublicFunction("Number.prototype.toLocaleString", function () {
                    if ((typeof this) !== 'number' && !(this instanceof Number)) {
                        platform.raiseNeedObjectOfType("Number.prototype.toLocaleString", "Number");
                    }

                    var stateObject = setPrototype({}, null);
                    InitializeNumberFormat(stateObject, arguments[0], arguments[1]);

                    var n = Internal.ToNumber(this);
                    // Need to special case the '-0' case to format as 0 instead of -0.
                    return String(platform.formatNumber(n === -0 ? 0 : n, stateObject));
                }), 3);

            if (InitType === 'Intl') {
                function NumberFormat() {
                    // The function should have length of 0, hence we are getting arguments this way.
                    var locales = undefined;
                    var options = undefined;
                    if (arguments.length >= 1) locales = arguments[0];
                    if (arguments.length >= 2) options = arguments[1];

                    if (this === Intl || this === undefined) {
                        return new NumberFormat(locales, options);
                    }

                    var obj = Internal.ToObject(this);

                    if (!ObjectIsExtensible(obj)) {
                        platform.raiseObjectIsNonExtensible("NumberFormat");
                    }

                    // Use the hidden object to store data
                    var hiddenObject = platform.getHiddenObject(obj);

                    if (hiddenObject === undefined) {
                        hiddenObject = setPrototype({}, null);
                        platform.setHiddenObject(obj, hiddenObject);
                    }

                    InitializeNumberFormat(hiddenObject, locales, options);

                    hiddenObject.__boundFormat = callInstanceFunc(FunctionInstanceBind, format, obj)

                    return obj;
                }
                tagPublicFunction("Intl.NumberFormat", NumberFormat);

                function format(n) {
                    n = Internal.ToNumber(n);

                    if (typeof this !== 'object') {
                        platform.raiseNeedObjectOfType("NumberFormat.prototype.format", "NumberFormat");
                    }

                    var hiddenObject = platform.getHiddenObject(this);
                    if (hiddenObject === undefined || !hiddenObject.__initializedNumberFormat) {
                        platform.raiseNeedObjectOfType("NumberFormat.prototype.format", "NumberFormat");
                    }

                    // Need to special case the '-0' case to format as 0 instead of -0.
                    return String(platform.formatNumber(n === -0 ? 0 : n, hiddenObject));
                }
                tagPublicFunction("Intl.NumberFormat.prototype.format", format);

                ObjectDefineProperty(NumberFormat, 'supportedLocalesOf', { value: numberFormat_supportedLocalesOf, writable: true, configurable: true });

                var options = ['locale', 'numberingSystem', 'style', 'currency', 'currencyDisplay', 'minimumIntegerDigits',
                    'minimumFractionDigits', 'maximumFractionDigits', 'minimumSignificantDigits', 'maximumSignificantDigits',
                    'useGrouping'];

                ObjectDefineProperty(NumberFormat, 'prototype', { value: new NumberFormat(), writable: false, enumerable: false, configurable: false });
                setPrototype(NumberFormat.prototype, Object.prototype);
                ObjectDefineProperty(NumberFormat.prototype, 'constructor', { value: NumberFormat, writable: true, enumerable: false, configurable: true });

                ObjectDefineProperty(NumberFormat.prototype, 'resolvedOptions', {
                    value: function () {
                        if (typeof this !== 'object') {
                            platform.raiseNeedObjectOfType("NumberFormat.prototype.resolvedOptions", "NumberFormat");
                        }
                        var hiddenObject = platform.getHiddenObject(this);
                        if (hiddenObject === undefined || !hiddenObject.__initializedNumberFormat) {
                            platform.raiseNeedObjectOfType("NumberFormat.prototype.resolvedOptions", "NumberFormat");
                        }

                        var resolvedOptions = setPrototype({}, null);

                        callInstanceFunc(ArrayInstanceForEach, options, function (option) {
                            if (typeof hiddenObject['__' + option] !== 'undefined') {
                                resolvedOptions[option] = hiddenObject['__' + option];
                            }
                        });

                        return setPrototype(resolvedOptions, {});
                    }, writable: true, enumerable: false, configurable: true
                });


                ObjectDefineProperty(NumberFormat.prototype, 'format', {
                    get: function () {

                        if (typeof this !== 'object') {
                            platform.raiseNeedObjectOfType("NumberFormat.prototype.format", "NumberFormat");
                        }

                        var hiddenObject = platform.getHiddenObject(this);
                        if (hiddenObject === undefined || !hiddenObject.__initializedNumberFormat) {
                            platform.raiseNeedObjectOfType("NumberFormat.prototype.format", "NumberFormat");
                        }

                        return hiddenObject.__boundFormat;
                    }, enumerable: false, configurable: true
                });


                return NumberFormat;
            }
        }
        // 'Init.NumberFormat' not defined if reached here. Return 'undefined'
        return undefined;
    })();

    // Intl.DateTimeFormat, Date.prototype.toLocaleString, Date.prototype.toLocaleDateString, Date.prototype.toLocaleTimeString
    var DateTimeFormat = (function () {

        if (InitType === 'Intl' || InitType === 'Date') {
            function ToDateTimeOptions(options, required, defaults) {
                if (options === undefined) {
                    options = setPrototype({}, null);
                } else {
                    options = Internal.ToObject(options);
                }

                var needDefaults = true;
                if (required === "date" || required === "any") {
                    if (options.weekday !== undefined || options.year !== undefined || options.month !== undefined || options.day !== undefined)
                        needDefaults = false;
                }
                if (required === "time" || required === "any") {
                    if (options.hour !== undefined || options.minute !== undefined || options.second !== undefined)
                        needDefaults = false;
                }

                if (needDefaults && (defaults === "date" || defaults === "all")) {
                    ObjectDefineProperty(options, "year", {
                        value: "numeric", writable: true,
                        enumerable: true, configurable: true
                    });
                    ObjectDefineProperty(options, "month", {
                        value: "numeric", writable: true,
                        enumerable: true, configurable: true
                    });
                    ObjectDefineProperty(options, "day", {
                        value: "numeric", writable: true,
                        enumerable: true, configurable: true
                    });
                }
                if (needDefaults && (defaults === "time" || defaults === "all")) {
                    ObjectDefineProperty(options, "hour", {
                        value: "numeric", writable: true,
                        enumerable: true, configurable: true
                    });
                    ObjectDefineProperty(options, "minute", {
                        value: "numeric", writable: true,
                        enumerable: true, configurable: true
                    });
                    ObjectDefineProperty(options, "second", {
                        value: "numeric", writable: true,
                        enumerable: true, configurable: true
                    });
                }
                return options;
            }

            // Currently you cannot format date pieces and time pieces together, so this builds up a format template for each separately.
            function EcmaOptionsToWindowsTemplate(options) {
                var template = [];

                if (options.weekday) {
                    if (options.weekday === 'narrow' || options.weekday === 'short') {
                        callInstanceFunc(ArrayInstancePush, template, 'dayofweek.abbreviated');
                    } else if (options.weekday === 'long') {
                        callInstanceFunc(ArrayInstancePush, template, 'dayofweek.full');
                    }
                }

                // TODO: Era not supported
                if (options.year) {
                    if (options.year === '2-digit') {
                        callInstanceFunc(ArrayInstancePush, template, 'year.abbreviated');
                    } else if (options.year === 'numeric') {
                        callInstanceFunc(ArrayInstancePush, template, 'year.full');
                    }
                }

                if (options.month) {
                    if (options.month === '2-digit' || options.month === 'numeric') {
                        callInstanceFunc(ArrayInstancePush, template, 'month.numeric')
                    } else if (options.month === 'short' || options.month === 'narrow') {
                        callInstanceFunc(ArrayInstancePush, template, 'month.abbreviated');
                    } else if (options.month === 'long') {
                        callInstanceFunc(ArrayInstancePush, template, 'month.full');
                    }
                }

                if (options.day) {
                    callInstanceFunc(ArrayInstancePush, template, 'day');
                }

                if (options.timeZoneName) {
                    if (options.timeZoneName === "short") {
                        callInstanceFunc(ArrayInstancePush, template, 'timezone.abbreviated');
                    } else if (options.timeZoneName === "long") {
                        callInstanceFunc(ArrayInstancePush, template, 'timezone.full');
                    }
                }

                callInstanceFunc(ArrayInstanceForEach, ['hour', 'minute', 'second'], function (opt) {
                    if (options[opt]) {
                        callInstanceFunc(ArrayInstancePush, template, opt);
                    }
                });

                // TODO: Timezone Name not supported.
                return getArrayLength(template) > 0 ? callInstanceFunc(ArrayInstanceJoin, template, ' ') : undefined;
            }

            var WindowsToEcmaCalendarMap = {
                'GregorianCalendar': 'gregory',
                'HebrewCalendar': 'hebrew',
                'HijriCalendar': 'islamic',
                'JapaneseCalendar': 'japanese',
                'JulianCalendar': 'julian',
                'KoreanCalendar': 'korean',
                'UmAlQuraCalendar': 'islamic-civil',
                'ThaiCalendar': 'thai',
                'TaiwanCalendar': 'taiwan'
            };

            function WindowsToEcmaCalendar(calendar) {
                if (typeof calendar === 'undefined') {
                    return '';
                }

                return WindowsToEcmaCalendarMap[calendar] || 'gregory';
            }

            // Certain formats have similar patterns on both ecma and windows; will use helper methods for them
            function correctWeekdayEraMonthPattern(patternString, userValue, searchParam) {
                // parts[1] is either dayofweek.solo, dayofweek, era or month; parts[3] is either abbreviated or full
                var parts = platform.builtInRegexMatch(patternString, RegExp("{(" + searchParam + "(\\.solo)?)\\.([a-z]*)(\\([0-9]\\))?}"));
                // If this happens that means windows removed the specific pattern (which isn't expected; but better be safe)
                if (parts === null) {
                    RaiseAssert(new Error("Error when correcting windows returned weekday/Era/Month pattern; regex returned null. \nInput was: '" + patternString + "'\nRegex: '" + "{(" + searchParam + "(\\.solo)?)\\.([a-z]*)(\\([0-9]\\))?}'"));
                    return patternString;
                }

                if (parts[3] !== "full" && userValue === "long") {
                    return callInstanceFunc(StringInstanceReplace, patternString, parts[0], "{" + parts[1] + "." + "full" + "}");
                } else if (userValue !== "long") {
                    return callInstanceFunc(StringInstanceReplace, patternString, parts[0], "{" + parts[1] + "." + (userValue === "short" ? "abbreviated" : "abbreviated(1)") + "}");
                }
                return patternString;
            }
            function correctDayHourMinuteSecondMonthPattern(patternString, userValue, searchParam) {
                // parts[1] is either month, day, hour, minute, or second
                var parts = platform.builtInRegexMatch(patternString, RegExp("{(" + searchParam + ")(?:\\.solo)?\\.([a-z]*)(\\([0-9]\\))?}"));
                if (parts === null) {
                    RaiseAssert(new Error("Error when correcting windows returned day/hour/minute/second/month pattern; regex returned null. \nInput was: '" + patternString + "'\nRegex: '" + "{(" + searchParam + "(\\.solo)?)\\.([a-z]*)(\\([0-9]\\))?}'"));
                    return patternString;
                }

                if (userValue === "2-digit") { // Only correct the 2 digit; unless part[2] isn't integer
                    return callInstanceFunc(StringInstanceReplace, patternString, parts[0], "{" + parts[1] + ".integer(2)}");
                } else if (parts[2] !== "integer") {
                    return callInstanceFunc(StringInstanceReplace, patternString, parts[0], "{" + parts[1] + ".integer}");
                }

                return patternString;
            }

            // Perhaps the level of validation that we have might not be required for this method
            function updatePatternStrings(patternString, dateTimeFormat) {

                if (dateTimeFormat.__weekday !== undefined) {
                    patternString = correctWeekdayEraMonthPattern(patternString, dateTimeFormat.__weekday, "dayofweek");
                }

                if (dateTimeFormat.__era !== undefined) {
                    // This is commented because not all options are supported for locales that do have era;
                    // In addition, we can't force era to be part of a locale using templates.
                    // patternString = correctWeekdayEraMonthPattern(patternString, dateTimeFormat.__era, "era", 2);
                }

                if (dateTimeFormat.__year === "2-digit") {
                    var parts = platform.builtInRegexMatch(patternString, /\{year\.[a-z]*(\([0-9]\))?\}/);
                    if (parts === null) {
                        RaiseAssert(new Error("Error when correcting windows returned year; regex returned null"));
                    } else {
                        patternString = callInstanceFunc(StringInstanceReplace, patternString, parts[0], "{year.abbreviated(2)}");
                    }
                } else if (dateTimeFormat.__year === "full") {
                    var parts = platform.builtInRegexMatch(patternString, /\{year\.[a-z]*(\([0-9]\))?\}/);
                    if (parts === null) {
                        RaiseAssert(new Error("Error when correcting windows returned year; regex returned null"));
                    } else {
                        patternString = callInstanceFunc(StringInstanceReplace, patternString, parts[0], "{year.full}");
                    }
                }

                // Month partially overlaps with weekday/month; unless it's 2-digit or numeric in which case it overlaps with day/hour/minute/second
                if (dateTimeFormat.__month !== undefined && dateTimeFormat.__month !== "2-digit" && dateTimeFormat.__month !== "numeric") {
                    patternString = correctWeekdayEraMonthPattern(patternString, dateTimeFormat.__month, "month");
                } else if (dateTimeFormat.__month !== undefined) {
                    patternString = correctDayHourMinuteSecondMonthPattern(patternString, dateTimeFormat.__month, "month");
                }

                if (dateTimeFormat.__day !== undefined) {
                    patternString = correctDayHourMinuteSecondMonthPattern(patternString, dateTimeFormat.__day, "day");
                }

                if (dateTimeFormat.__hour !== undefined) {
                    patternString = correctDayHourMinuteSecondMonthPattern(patternString, dateTimeFormat.__hour, "hour");
                }

                if (dateTimeFormat.__minute !== undefined) {
                    patternString = correctDayHourMinuteSecondMonthPattern(patternString, dateTimeFormat.__minute, "minute");
                }

                if (dateTimeFormat.__second !== undefined) {
                    patternString = correctDayHourMinuteSecondMonthPattern(patternString, dateTimeFormat.__second, "second");
                }

                if (dateTimeFormat.__timeZoneName !== undefined) {
                    patternString = correctWeekdayEraMonthPattern(patternString, dateTimeFormat.__timeZoneName, "timezone");
                }

                return patternString;
            }

            function InitializeDateTimeFormat(dateTimeFormat, localeList, options) {
                if (typeof dateTimeFormat != "object") {
                    platform.raiseNeedObject();
                }

                if (callInstanceFunc(ObjectInstanceHasOwnProperty, dateTimeFormat, '__initializedIntlObject') && dateTimeFormat.__initializedIntlObject) {
                    platform.raiseObjectIsAlreadyInitialized("DateTimeFormat", "DateTimeFormat");
                }

                dateTimeFormat.__initializedIntlObject = true;

                // Extract the options
                options = ToDateTimeOptions(options, "any", "date");

                var matcher = GetOption(options, "localeMatcher", "string", ["lookup", "best fit"], "best fit");
                var timeZone = GetOption(options, "timeZone", "string", undefined, undefined);
                if (timeZone !== undefined) {
                    timeZone = platform.validateAndCanonicalizeTimeZone(timeZone);
                    if (timeZone === undefined) {
                        platform.raiseOptionValueOutOfRange();
                    }
                } else {
                    timeZone = platform.getDefaultTimeZone();
                }

                // Format options
                var weekday = GetOption(options, "weekday", "string", ['narrow', 'short', 'long'], undefined);
                var era = GetOption(options, "era", "string", ['narrow', 'short', 'long'], undefined);
                var year = GetOption(options, "year", "string", ['2-digit', 'numeric'], undefined);
                var month = GetOption(options, "month", "string", ['2-digit', 'numeric', 'narrow', 'short', 'long'], undefined);
                var day = GetOption(options, "day", "string", ['2-digit', 'numeric'], undefined);
                var hour = GetOption(options, "hour", "string", ['2-digit', 'numeric'], undefined);
                var minute = GetOption(options, "minute", "string", ['2-digit', 'numeric'], undefined);
                var second = GetOption(options, "second", "string", ['2-digit', 'numeric'], undefined);
                var timeZoneName = GetOption(options, "timeZoneName", "string", ['short', 'long'], undefined);

                var hour12 = hour ? GetOption(options, "hour12", "boolean", undefined, undefined) : undefined;
                var formatMatcher = GetOption(options, "formatMatcher", "string", ["basic", "best fit"], "best fit");

                var windowsClock = hour12 !== undefined ? (hour12 ? "12HourClock" : "24HourClock") : undefined;

                var templateString = EcmaOptionsToWindowsTemplate(setPrototype({
                    weekday: weekday,
                    era: era,
                    year: year,
                    month: month,
                    day: day,
                    hour: hour,
                    minute: minute,
                    second: second,
                    timeZoneName: timeZoneName
                }, null));

                // Deal with the locale
                localeList = CanonicalizeLocaleList(localeList);
                var resolvedLocaleInfo = resolveLocales(localeList, matcher, ["nu", "ca"], strippedDefaultLocale);

                // Assign the options
                dateTimeFormat.__matcher = matcher;
                dateTimeFormat.__timeZone = timeZone;
                dateTimeFormat.__locale = resolvedLocaleInfo.locale;

                // Format options
                dateTimeFormat.__weekday = weekday;
                dateTimeFormat.__era = era;
                dateTimeFormat.__year = year;
                dateTimeFormat.__month = month;
                dateTimeFormat.__day = day;
                dateTimeFormat.__hour = hour;
                dateTimeFormat.__minute = minute;
                dateTimeFormat.__second = second;
                dateTimeFormat.__timeZoneName = timeZoneName;

                dateTimeFormat.__hour12 = hour12;
                dateTimeFormat.__formatMatcher = formatMatcher;
                dateTimeFormat.__windowsClock = windowsClock;

                dateTimeFormat.__templateString = templateString;

                /*
                 * NOTE:
                 * Pattern string's are position-sensitive; while templates are not.
                 * If we specify {hour.integer(2)}:{minute.integer(2)} pattern string; we will always format as HH:mm.
                 * On the other hand, template strings don't give as fine granularity for options; and the platform decides how long month.abbreviated should be.
                 * Therefore, we have to create using template strings; and then change the .abbreivated/.integer values to have correct digits count if necessary.
                 * Thus, this results in this redundant looking code to create dateTimeFormat twice.
                 */
                var errorThrown = false;

                try {
                    // Create the DateTimeFormatter to extract pattern strings
                    platform.createDateTimeFormat(dateTimeFormat, false);
                } catch (e) {
                    // Rethrow SOE or OOM
                    throwExIfOOMOrSOE(e);

                    // We won't throw for the first exception, but assume the template strings were rejected.
                    // Instead, we will try to fall back to default template strings.
                    var defaultOptions = ToDateTimeOptions(options, "none", "all");
                    dateTimeFormat.__templateString = EcmaOptionsToWindowsTemplate(defaultOptions, null);
                    errorThrown = true;
                }
                if (!errorThrown) {
                    // Update the pattern strings
                    dateTimeFormat.__templateString = updatePatternStrings(dateTimeFormat.__patternStrings[0], dateTimeFormat);
                }

                try {
                    // Cache the date time formatter
                    platform.createDateTimeFormat(dateTimeFormat, true);
                } catch (e) {
                    // Rethrow SOE or OOM
                    throwExIfOOMOrSOE(e);

                    // Otherwise, Generic message to cover the exception throw from the platform.
                    // The platform's exception is also generic and in most if not all cases specifies that "a" argument is invalid.
                    // We have no other information from the platform on the cause of the exception.
                    platform.raiseOptionValueOutOfRange();
                }

                // Correct the api's updated
                dateTimeFormat.__calendar = WindowsToEcmaCalendar(dateTimeFormat.__windowsCalendar);

                dateTimeFormat.__numberingSystem = callInstanceFunc(StringInstanceToLowerCase, dateTimeFormat.__numberingSystem);
                if (dateTimeFormat.__hour !== undefined) {
                    dateTimeFormat.__hour12 = dateTimeFormat.__windowsClock === "12HourClock";
                }
                dateTimeFormat.__initializedDateTimeFormat = true;
            }

            
            platform.registerBuiltInFunction(tagPublicFunction("Date.prototype.toLocaleString", function () {
                if (typeof this !== 'object' || !(this instanceof Date)) {
                    platform.raiseNeedObjectOfType("Date.prototype.toLocaleString", "Date");
                }
                var value = callInstanceFunc(DateInstanceGetDate, new Date(this));
                if (isNaN(value) || !isFinite(value)) {
                    return "Invalid Date";
                }
                var stateObject = setPrototype({}, null);
                InitializeDateTimeFormat(stateObject, arguments[0], ToDateTimeOptions(arguments[1], "any", "all"));
                return String(platform.formatDateTime(Internal.ToNumber(this), stateObject));
            }), 0);

            platform.registerBuiltInFunction(tagPublicFunction("Date.prototype.toLocaleDateString", function () {
                if (typeof this !== 'object' || !(this instanceof Date)) {
                    platform.raiseNeedObjectOfType("Date.prototype.toLocaleDateString", "Date");
                }
                var value = callInstanceFunc(DateInstanceGetDate, new Date(this));
                if (isNaN(value) || !isFinite(value)) {
                    return "Invalid Date";
                }
                var stateObject = setPrototype({}, null);
                InitializeDateTimeFormat(stateObject, arguments[0], ToDateTimeOptions(arguments[1], "date", "date"));
                return String(platform.formatDateTime(Internal.ToNumber(this), stateObject));
            }), 1);

            platform.registerBuiltInFunction(tagPublicFunction("Date.prototype.toLocaleTimeString", function () {
                if (typeof this !== 'object' || !(this instanceof Date)) {
                    platform.raiseNeedObjectOfType("Date.prototype.toLocaleTimeString", "Date");
                }
                var value = callInstanceFunc(DateInstanceGetDate, new Date(this));
                if (isNaN(value) || !isFinite(value)) {
                    return "Invalid Date";
                }
                var stateObject = setPrototype({}, null);
                InitializeDateTimeFormat(stateObject, arguments[0], ToDateTimeOptions(arguments[1], "time", "time"));
                return String(platform.formatDateTime(Internal.ToNumber(this), stateObject));
            }), 2);
            if (InitType === 'Intl') {
                function DateTimeFormat() {
                    // The function should have length of 0, hence we are getting arguments this way.
                    var locales = undefined;
                    var options = undefined;
                    if (arguments.length >= 1) locales = arguments[0];
                    if (arguments.length >= 2) options = arguments[1];
                    if (this === Intl || this === undefined) {
                        return new DateTimeFormat(locales, options);
                    }

                    var obj = Internal.ToObject(this);
                    if (!ObjectIsExtensible(obj)) {
                        platform.raiseObjectIsNonExtensible("DateTimeFormat");
                    }

                    // Use the hidden object to store data
                    var hiddenObject = platform.getHiddenObject(obj);

                    if (hiddenObject === undefined) {
                        hiddenObject = setPrototype({}, null);
                        platform.setHiddenObject(obj, hiddenObject);
                    }

                    InitializeDateTimeFormat(hiddenObject, locales, options);

                    hiddenObject.__boundFormat = callInstanceFunc(FunctionInstanceBind, format, obj);

                    return obj;
                }
                tagPublicFunction("Intl.DateTimeFormat", DateTimeFormat);

                function format() {

                    if (typeof this !== 'object') {
                        platform.raiseNeedObjectOfType("DateTimeFormat.prototype.format", "DateTimeFormat");
                    }
                    var hiddenObject = platform.getHiddenObject(this);
                    if (hiddenObject === undefined || !hiddenObject.__initializedDateTimeFormat) {
                        platform.raiseNeedObjectOfType("DateTimeFormat.prototype.format", "DateTimeFormat");
                    }

                    if (arguments.length >= 1) {
                        if (isNaN(arguments[0]) || !isFinite(arguments[0])) {
                            platform.raiseInvalidDate();
                        }
                        return String(platform.formatDateTime(Internal.ToNumber(arguments[0]), hiddenObject));
                    }

                    return String(platform.formatDateTime(DateNow(), hiddenObject));
                }
                tagPublicFunction("Intl.DateTimeFormat.prototype.format", format);

                DateTimeFormat.__relevantExtensionKeys = ['ca', 'nu'];

                ObjectDefineProperty(DateTimeFormat, 'prototype', { value: new DateTimeFormat(), writable: false, enumerable: false, configurable: false });
                setPrototype(DateTimeFormat.prototype, Object.prototype);
                ObjectDefineProperty(DateTimeFormat.prototype, 'constructor', { value: DateTimeFormat, writable: true, enumerable: false, configurable: true });

                ObjectDefineProperty(DateTimeFormat.prototype, 'format', {
                    get: function () {
                        if (typeof this !== 'object') {
                            platform.raiseNeedObjectOfType("DateTimeFormat.prototype.format", "DateTimeFormat");
                        }

                        var hiddenObject = platform.getHiddenObject(this);
                        if (hiddenObject === undefined || !hiddenObject.__initializedDateTimeFormat) {
                            platform.raiseNeedObjectOfType("DateTimeFormat.prototype.format", "DateTimeFormat");
                        }

                        return hiddenObject.__boundFormat;
                    }, enumerable: false, configurable: true
                });

                ObjectDefineProperty(DateTimeFormat.prototype, 'resolvedOptions', {
                    value: function () {
                        if (typeof this !== 'object') {
                            platform.raiseNeedObjectOfType("DateTimeFormat.prototype.resolvedOptions", "DateTimeFormat");
                        }
                        var hiddenObject = platform.getHiddenObject(this);
                        if (hiddenObject === undefined || !hiddenObject.__initializedDateTimeFormat) {
                            platform.raiseNeedObjectOfType("DateTimeFormat.prototype.resolvedOptions", "DateTimeFormat");
                        }
                        var temp = setPrototype({
                            locale: hiddenObject.__locale,
                            calendar: hiddenObject.__calendar, // ca unicode extension
                            numberingSystem: hiddenObject.__numberingSystem, // nu unicode extension
                            timeZone: hiddenObject.__timeZone,
                            hour12: hiddenObject.__hour12,
                            weekday: hiddenObject.__weekday,
                            era: hiddenObject.__era,
                            year: hiddenObject.__year,
                            month: hiddenObject.__month,
                            day: hiddenObject.__day,
                            hour: hiddenObject.__hour,
                            minute: hiddenObject.__minute,
                            second: hiddenObject.__second,
                            timeZoneName: hiddenObject.__timeZoneName
                        }, null)
                        var options = setPrototype({}, null);
                        callInstanceFunc(ArrayInstanceForEach, ObjectGetOwnPropertyNames(temp), function (prop) {
                            if ((temp[prop] !== undefined || prop === 'timeZone') && callInstanceFunc(ObjectInstanceHasOwnProperty, hiddenObject, "__" + prop)) options[prop] = temp[prop];
                        }, hiddenObject);
                        return setPrototype(options, Object.prototype);
                    }, writable: true, enumerable: false, configurable: true
                });

                ObjectDefineProperty(DateTimeFormat, 'supportedLocalesOf', { value: dateTimeFormat_supportedLocalesOf, writable: true, configurable: true });



                return DateTimeFormat;
            }
        }
        // 'Init.DateTimeFormat' not defined if reached here. Return 'undefined'
        return undefined;
    })();

    // Initialize Intl properties only if needed
    if (InitType === 'Intl') {
        ObjectDefineProperty(Intl, "Collator", { value: Collator, writable: true, enumerable: false, configurable: true });
        ObjectDefineProperty(Intl, "NumberFormat", { value: NumberFormat, writable: true, enumerable: false, configurable: true });
        ObjectDefineProperty(Intl, "DateTimeFormat", { value: DateTimeFormat, writable: true, enumerable: false, configurable: true });
    }
});
