/**
 * @fileoverview Default CLIEngineOptions.
 * @author Ian VanSchooten
 * @copyright 2016 Ian VanSchooten. All rights reserved.
 * See LICENSE in root directory for full license.
 */

"use strict";

var DEFAULT_PARSER = require("../conf/eslint.json").parser;

module.exports = {
    configFile: null,
    baseConfig: false,
    rulePaths: [],
    useEslintrc: true,
    envs: [],
    globals: [],
    rules: {},
    extensions: [".js"],
    ignore: true,
    ignorePath: null,
    parser: DEFAULT_PARSER,
    cache: false,
    // in order to honor the cacheFile option if specified
    // this option should not have a default value otherwise
    // it will always be used
    cacheLocation: "",
    cacheFile: ".eslintcache",
    fix: false,
    allowInlineConfig: true,
    cwd: process.cwd()
};
