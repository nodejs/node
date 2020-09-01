/*
 * STOP!!! DO NOT MODIFY.
 *
 * This file is part of the ongoing work to move the eslintrc-style config
 * system into the @eslint/eslintrc package. This file needs to remain
 * unchanged in order for this work to proceed.
 *
 * If you think you need to change this file, please contact @nzakas first.
 *
 * Thanks in advance for your cooperation.
 */

/**
 * @fileoverview `ConfigArray` class.
 * @author Toru Nagashima <https://github.com/mysticatea>
 */
"use strict";

const { ConfigArray, getUsedExtractedConfigs } = require("./config-array");
const { ConfigDependency } = require("./config-dependency");
const { ExtractedConfig } = require("./extracted-config");
const { IgnorePattern } = require("./ignore-pattern");
const { OverrideTester } = require("./override-tester");

module.exports = {
    ConfigArray,
    ConfigDependency,
    ExtractedConfig,
    IgnorePattern,
    OverrideTester,
    getUsedExtractedConfigs
};
