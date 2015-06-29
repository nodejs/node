/**
 * @fileoverview Config initialization wizard.
 * @author Ilya Volodin
 * @copyright 2015 Ilya Volodin. All rights reserved.
 */

"use strict";

var exec = require("child_process").exec,
    fs = require("fs"),
    inquirer = require("inquirer"),
    yaml = require("js-yaml");

/* istanbul ignore next: hard to test fs function */
/**
 * Create .eslintrc file in the current working directory
 * @param {object} config object that contains user's answers
 * @param {bool} isJson should config file be json or yaml
 * @param {function} callback function to call once the file is written.
 * @returns {void}
 */
function writeFile(config, isJson, callback) {
    try {
        fs.writeFileSync("./.eslintrc", isJson ? JSON.stringify(config, null, 4) : yaml.safeDump(config));
    } catch (e) {
        callback(e);
        return;
    }
    if (config.plugins && config.plugins.indexOf("react") >= 0) {
        exec("npm i eslint-plugin-react --save-dev", callback);
    } else {
        callback();
    }
}

/**
 * process user's answers and create config object
 * @param {object} answers answers received from inquirer
 * @returns {object} config object
 */
function processAnswers(answers) {
    var config = {rules: {}, env: {}};
    config.rules.indent = [2, answers.indent];
    config.rules.quotes = [2, answers.quotes];
    config.rules["linebreak-style"] = [2, answers.linebreak];
    config.rules.semi = [2, answers.semi ? "always" : "never"];
    if (answers.es6) {
        config.env.es6 = true;
    }
    answers.env.forEach(function(env) {
        config.env[env] = true;
    });
    if (answers.jsx) {
        config.ecmaFeatures = {jsx: true};
        if (answers.react) {
            config.plugins = ["react"];
        }
    }
    return config;
}

/* istanbul ignore next: no need to test inquirer*/
/**
 * Ask use a few questions on command prompt
 * @param {function} callback callback function when file has been written
 * @returns {void}
 */
function promptUser(callback) {
    inquirer.prompt([
        {
            type: "list",
            name: "indent",
            message: "What style of indentation do you use?",
            default: "tabs",
            choices: [{name: "Tabs", value: "tab"}, {name: "Spaces", value: 4}]
        },
        {
            type: "list",
            name: "quotes",
            message: "What quotes do you use for strings?",
            default: "double",
            choices: [{name: "Double", value: "double"}, {name: "Single", value: "single"}]
        },
        {
            type: "list",
            name: "linebreak",
            message: "What line endings do you use?",
            default: "unix",
            choices: [{name: "Unix", value: "unix"}, {name: "Windows", value: "windows"}]
        },
        {
            type: "confirm",
            name: "semi",
            message: "Do you require semicolons?",
            default: true
        },
        {
            type: "confirm",
            name: "es6",
            message: "Are you using ECMAScript 6 features?",
            default: false
        },
        {
            type: "checkbox",
            name: "env",
            message: "Where will your code run?",
            default: ["browser"],
            choices: [{name: "Node", value: "node"}, {name: "Browser", value: "browser"}]
        },
        {
            type: "confirm",
            name: "jsx",
            message: "Do you use JSX?",
            default: false
        },
        {
            type: "confirm",
            name: "react",
            message: "Do you use React",
            default: false,
            when: function (answers) {
                return answers.jsx;
            }
        },
        {
            type: "list",
            name: "format",
            message: "What format do you want your config file to be in?",
            default: "JSON",
            choices: ["JSON", "YAML"]
        }
    ], function(answers) {
        var config = processAnswers(answers);
        writeFile(config, answers.format === "JSON", callback);
    });
}

var init = {
    processAnswers: processAnswers,
    initializeConfig: /* istanbul ignore next */ function(callback) {
        promptUser(callback);
    }
};

module.exports = init;
