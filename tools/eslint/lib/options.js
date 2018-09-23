/**
 * @fileoverview Options configuration for optionator.
 * @author George Zahariev
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var optionator = require("optionator");

//------------------------------------------------------------------------------
// Initialization and Public Interface
//------------------------------------------------------------------------------

// exports "parse(args)", "generateHelp()", and "generateHelpForOption(optionName)"
module.exports = optionator({
    prepend: "eslint [options] file.js [file.js] [dir]",
    concatRepeatedArrays: true,
    mergeRepeatedObjects: true,
    options: [{
        heading: "Options"
    }, {
        option: "help",
        alias: "h",
        type: "Boolean",
        description: "Show help"
    }, {
        option: "config",
        alias: "c",
        type: "path::String",
        description: "Use configuration from this file"
    }, {
        option: "rulesdir",
        type: "[path::String]",
        description: "Use additional rules from this directory"
    }, {
        option: "format",
        alias: "f",
        type: "String",
        default: "stylish",
        description: "Use a specific output format"
    }, {
        option: "version",
        alias: "v",
        type: "Boolean",
        description: "Outputs the version number"
    }, {
        option: "reset",
        type: "Boolean",
        default: "false",
        description: "Set all default rules to off"
    }, {
        option: "eslintrc",
        type: "Boolean",
        default: "true",
        description: "Disable use of configuration from .eslintrc"
    }, {
        option: "env",
        type: "[String]",
        description: "Specify environments"
    }, {
        option: "ext",
        type: "[String]",
        default: ".js",
        description: "Specify JavaScript file extensions"
    }, {
        option: "plugin",
        type: "[String]",
        description: "Specify plugins"
    }, {
        option: "global",
        type: "[String]",
        description: "Define global variables"
    }, {
        option: "rule",
        type: "Object",
        description: "Specify rules"
    },
    {
        option: "ignore-path",
        type: "path::String",
        description: "Specify path of ignore file"
    },
    {
        option: "ignore",
        type: "Boolean",
        default: "true",
        description: "Disable use of .eslintignore"
    },
    {
        option: "ignore-pattern",
        type: "String",
        description: "Pattern of files to ignore (in addition to those in .eslintignore)"
    },
    {
        option: "color",
        type: "Boolean",
        default: "true",
        description: "Disable color in piped output"
    },
    {
        option: "output-file",
        alias: "o",
        type: "path::String",
        description: "Specify file to write report to"
    },
    {
        option: "quiet",
        type: "Boolean",
        default: "false",
        description: "Report errors only"
    },
    {
        option: "stdin",
        type: "Boolean",
        default: "false",
        description: "Lint code provided on <STDIN>"
    },
    {
        option: "stdin-filename",
        type: "String",
        description: "Specify filename to process STDIN as"
    },
    {
        option: "init",
        type: "Boolean",
        default: "false",
        description: "Run config initialization wizard"
    }]
});
