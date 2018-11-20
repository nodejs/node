/**
 * @fileoverview Defines environment settings and globals.
 * @author Elan Shanker
 * @copyright 2014 Elan Shanker. All rights reserved.
 */
"use strict";

var globals = require("globals");

module.exports = {
    builtin: globals.builtin,
    browser: {
        globals: globals.browser
    },
    node: {
        globals: globals.node,
        ecmaFeatures: {
            globalReturn: true
        },
        rules: {
            "no-catch-shadow": 0,
            "no-console": 0,
            "no-mixed-requires": 2,
            "no-new-require": 2,
            "no-path-concat": 2,
            "no-process-exit": 2,
            "global-strict": [0, "always"],
            "handle-callback-err": [2, "err"]
        }
    },
    amd: {
        globals: globals.amd
    },
    mocha: {
        globals: globals.mocha
    },
    jasmine: {
        globals: globals.jasmine
    },
    phantomjs: {
        globals: globals.phantom
    },
    jquery: {
        globals: globals.jquery
    },
    prototypejs: {
        globals: globals.prototypejs
    },
    shelljs: {
        globals: globals.shelljs
    },
    meteor: {
        globals: globals.meteor
    },
    es6: {
        ecmaFeatures: {
            arrowFunctions: true,
            blockBindings: true,
            regexUFlag: true,
            regexYFlag: true,
            templateStrings: true,
            binaryLiterals: true,
            octalLiterals: true,
            unicodeCodePointEscapes: true,
            superInFunctions: true,
            defaultParams: true,
            restParams: true,
            forOf: true,
            objectLiteralComputedProperties: true,
            objectLiteralShorthandMethods: true,
            objectLiteralShorthandProperties: true,
            objectLiteralDuplicateProperties: true,
            generators: true,
            destructuring: true,
            classes: true
        }
    }
};
