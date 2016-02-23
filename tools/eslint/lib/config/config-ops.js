/**
 * @fileoverview Config file operations. This file must be usable in the browser,
 * so no Node-specific code can be here.
 * @author Nicholas C. Zakas
 * @copyright 2015 Nicholas C. Zakas. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var lodash = require("lodash"),
    debug = require("debug"),
    Environments = require("./environments");

//------------------------------------------------------------------------------
// Private
//------------------------------------------------------------------------------

debug = debug("eslint:config-ops");

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {

    /**
     * Creates an empty configuration object suitable for merging as a base.
     * @returns {Object} A configuration object.
     */
    createEmptyConfig: function() {
        return {
            globals: {},
            env: {},
            rules: {},
            parserOptions: {}
        };
    },

    /**
     * Creates an environment config based on the specified environments.
     * @param {Object<string,boolean>} env The environment settings.
     * @returns {Object} A configuration object with the appropriate rules and globals
     *      set.
     */
    createEnvironmentConfig: function(env) {

        var envConfig = this.createEmptyConfig();

        if (env) {

            envConfig.env = env;

            Object.keys(env).filter(function(name) {
                return env[name];
            }).forEach(function(name) {
                var environment = Environments.get(name);

                if (environment) {
                    debug("Creating config for environment " + name);
                    if (environment.globals) {
                        lodash.assign(envConfig.globals, environment.globals);
                    }

                    if (environment.parserOptions) {
                        lodash.assign(envConfig.parserOptions, environment.parserOptions);
                    }
                }
            });
        }

        return envConfig;
    },

    /**
     * Given a config with environment settings, applies the globals and
     * ecmaFeatures to the configuration and returns the result.
     * @param {Object} config The configuration information.
     * @returns {Object} The updated configuration information.
     */
    applyEnvironments: function(config) {
        if (config.env && typeof config.env === "object") {
            debug("Apply environment settings to config");
            return this.merge(this.createEnvironmentConfig(config.env), config);
        }

        return config;
    },

    /**
     * Merges two config objects. This will not only add missing keys, but will also modify values to match.
     * @param {Object} target config object
     * @param {Object} src config object. Overrides in this config object will take priority over base.
     * @param {boolean} [combine] Whether to combine arrays or not
     * @param {boolean} [isRule] Whether its a rule
     * @returns {Object} merged config object.
     */
    merge: function deepmerge(target, src, combine, isRule) {
        /*
         The MIT License (MIT)

         Copyright (c) 2012 Nicholas Fisher

         Permission is hereby granted, free of charge, to any person obtaining a copy
         of this software and associated documentation files (the "Software"), to deal
         in the Software without restriction, including without limitation the rights
         to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
         copies of the Software, and to permit persons to whom the Software is
         furnished to do so, subject to the following conditions:

         The above copyright notice and this permission notice shall be included in
         all copies or substantial portions of the Software.

         THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
         IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
         FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
         AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
         LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
         OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
         THE SOFTWARE.
         */
        // This code is taken from deepmerge repo (https://github.com/KyleAMathews/deepmerge) and modified to meet our needs.
        var array = Array.isArray(src) || Array.isArray(target);
        var dst = array && [] || {};

        combine = !!combine;
        isRule = !!isRule;
        if (array) {
            target = target || [];
            if (isRule && src.length > 1) {
                dst = dst.concat(src);
            } else {
                dst = dst.concat(target);
            }
            if (typeof src !== "object" && !Array.isArray(src)) {
                src = [src];
            }
            Object.keys(src).forEach(function(e, i) {
                e = src[i];
                if (typeof dst[i] === "undefined") {
                    dst[i] = e;
                } else if (typeof e === "object") {
                    if (isRule) {
                        dst[i] = e;
                    } else {
                        dst[i] = deepmerge(target[i], e, combine, isRule);
                    }
                } else {
                    if (!combine) {
                        dst[i] = e;
                    } else {
                        if (dst.indexOf(e) === -1) {
                            dst.push(e);
                        }
                    }
                }
            });
        } else {
            if (target && typeof target === "object") {
                Object.keys(target).forEach(function(key) {
                    dst[key] = target[key];
                });
            }
            Object.keys(src).forEach(function(key) {
                if (Array.isArray(src[key]) || Array.isArray(target[key])) {
                    dst[key] = deepmerge(target[key], src[key], key === "plugins", isRule);
                } else if (typeof src[key] !== "object" || !src[key] || key === "exported" || key === "astGlobals") {
                    dst[key] = src[key];
                } else {
                    dst[key] = deepmerge(target[key] || {}, src[key], combine, key === "rules");
                }
            });
        }

        return dst;
    }


};
