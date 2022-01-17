#!/usr/bin/env node
/* eslint-disable */
/******/ (() => { // webpackBootstrap
/******/ 	var __webpack_modules__ = ({

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Cli.js":
/*!*********************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Cli.js ***!
  \*********************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var constants = __webpack_require__(/*! ../constants.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/constants.js");
var Command = __webpack_require__(/*! ./Command.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Command.js");
var core = __webpack_require__(/*! ../core.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/core.js");
var format = __webpack_require__(/*! ../format.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/format.js");
var HelpCommand = __webpack_require__(/*! ./HelpCommand.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/HelpCommand.js");

const errorCommandSymbol = Symbol(`clipanion/errorCommand`);
function getDefaultColorSettings() {
    if (process.env.FORCE_COLOR === `0`)
        return false;
    if (process.env.FORCE_COLOR === `1`)
        return true;
    if (typeof process.stdout !== `undefined` && process.stdout.isTTY)
        return true;
    return false;
}
/**
 * @template Context The context shared by all commands. Contexts are a set of values, defined when calling the `run`/`runExit` functions from the CLI instance, that will be made available to the commands via `this.context`.
 */
class Cli {
    constructor({ binaryLabel, binaryName: binaryNameOpt = `...`, binaryVersion, enableColors = getDefaultColorSettings() } = {}) {
        this.registrations = new Map();
        this.builder = new core.CliBuilder({ binaryName: binaryNameOpt });
        this.binaryLabel = binaryLabel;
        this.binaryName = binaryNameOpt;
        this.binaryVersion = binaryVersion;
        this.enableColors = enableColors;
    }
    /**
     * Creates a new Cli and registers all commands passed as parameters.
     *
     * @param commandClasses The Commands to register
     * @returns The created `Cli` instance
     */
    static from(commandClasses, options = {}) {
        const cli = new Cli(options);
        for (const commandClass of commandClasses)
            cli.register(commandClass);
        return cli;
    }
    /**
     * Registers a command inside the CLI.
     */
    register(commandClass) {
        var _a;
        const specs = new Map();
        const command = new commandClass();
        for (const key in command) {
            const value = command[key];
            if (typeof value === `object` && value !== null && value[Command.Command.isOption]) {
                specs.set(key, value);
            }
        }
        const builder = this.builder.command();
        const index = builder.cliIndex;
        const paths = (_a = commandClass.paths) !== null && _a !== void 0 ? _a : command.paths;
        if (typeof paths !== `undefined`)
            for (const path of paths)
                builder.addPath(path);
        this.registrations.set(commandClass, { specs, builder, index });
        for (const [key, { definition }] of specs.entries())
            definition(builder, key);
        builder.setContext({
            commandClass,
        });
    }
    process(input) {
        const { contexts, process } = this.builder.compile();
        const state = process(input);
        switch (state.selectedIndex) {
            case constants.HELP_COMMAND_INDEX:
                {
                    return HelpCommand.HelpCommand.from(state, contexts);
                }
            default:
                {
                    const { commandClass } = contexts[state.selectedIndex];
                    const record = this.registrations.get(commandClass);
                    if (typeof record === `undefined`)
                        throw new Error(`Assertion failed: Expected the command class to have been registered.`);
                    const command = new commandClass();
                    command.path = state.path;
                    try {
                        for (const [key, { transformer }] of record.specs.entries())
                            command[key] = transformer(record.builder, key, state);
                        return command;
                    }
                    catch (error) {
                        error[errorCommandSymbol] = command;
                        throw error;
                    }
                }
                break;
        }
    }
    async run(input, context) {
        let command;
        if (!Array.isArray(input)) {
            command = input;
        }
        else {
            try {
                command = this.process(input);
            }
            catch (error) {
                context.stdout.write(this.error(error));
                return 1;
            }
        }
        if (command.help) {
            context.stdout.write(this.usage(command, { detailed: true }));
            return 0;
        }
        command.context = context;
        command.cli = {
            binaryLabel: this.binaryLabel,
            binaryName: this.binaryName,
            binaryVersion: this.binaryVersion,
            enableColors: this.enableColors,
            definitions: () => this.definitions(),
            error: (error, opts) => this.error(error, opts),
            process: input => this.process(input),
            run: (input, subContext) => this.run(input, { ...context, ...subContext }),
            usage: (command, opts) => this.usage(command, opts),
        };
        let exitCode;
        try {
            exitCode = await command.validateAndExecute().catch(error => command.catch(error).then(() => 0));
        }
        catch (error) {
            context.stdout.write(this.error(error, { command }));
            return 1;
        }
        return exitCode;
    }
    /**
     * Runs a command and exits the current `process` with the exit code returned by the command.
     *
     * @param input An array containing the name of the command and its arguments.
     *
     * @example
     * cli.runExit(process.argv.slice(2), Cli.defaultContext)
     */
    async runExit(input, context) {
        process.exitCode = await this.run(input, context);
    }
    suggest(input, partial) {
        const { suggest } = this.builder.compile();
        return suggest(input, partial);
    }
    definitions({ colored = false } = {}) {
        const data = [];
        for (const [commandClass, { index }] of this.registrations) {
            if (typeof commandClass.usage === `undefined`)
                continue;
            const { usage: path } = this.getUsageByIndex(index, { detailed: false });
            const { usage, options } = this.getUsageByIndex(index, { detailed: true, inlineOptions: false });
            const category = typeof commandClass.usage.category !== `undefined`
                ? format.formatMarkdownish(commandClass.usage.category, { format: this.format(colored), paragraphs: false })
                : undefined;
            const description = typeof commandClass.usage.description !== `undefined`
                ? format.formatMarkdownish(commandClass.usage.description, { format: this.format(colored), paragraphs: false })
                : undefined;
            const details = typeof commandClass.usage.details !== `undefined`
                ? format.formatMarkdownish(commandClass.usage.details, { format: this.format(colored), paragraphs: true })
                : undefined;
            const examples = typeof commandClass.usage.examples !== `undefined`
                ? commandClass.usage.examples.map(([label, cli]) => [format.formatMarkdownish(label, { format: this.format(colored), paragraphs: false }), cli.replace(/\$0/g, this.binaryName)])
                : undefined;
            data.push({ path, usage, category, description, details, examples, options });
        }
        return data;
    }
    usage(command = null, { colored, detailed = false, prefix = `$ ` } = {}) {
        var _a;
        // In case the default command is the only one, we can just show the command help rather than the general one
        if (command === null) {
            for (const commandClass of this.registrations.keys()) {
                const paths = commandClass.paths;
                const isDocumented = typeof commandClass.usage !== `undefined`;
                const isExclusivelyDefault = !paths || paths.length === 0 || (paths.length === 1 && paths[0].length === 0);
                const isDefault = isExclusivelyDefault || ((_a = paths === null || paths === void 0 ? void 0 : paths.some(path => path.length === 0)) !== null && _a !== void 0 ? _a : false);
                if (isDefault) {
                    if (command) {
                        command = null;
                        break;
                    }
                    else {
                        command = commandClass;
                    }
                }
                else {
                    if (isDocumented) {
                        command = null;
                        continue;
                    }
                }
            }
            if (command) {
                detailed = true;
            }
        }
        // @ts-ignore
        const commandClass = command !== null && command instanceof Command.Command
            ? command.constructor
            : command;
        let result = ``;
        if (!commandClass) {
            const commandsByCategories = new Map();
            for (const [commandClass, { index }] of this.registrations.entries()) {
                if (typeof commandClass.usage === `undefined`)
                    continue;
                const category = typeof commandClass.usage.category !== `undefined`
                    ? format.formatMarkdownish(commandClass.usage.category, { format: this.format(colored), paragraphs: false })
                    : null;
                let categoryCommands = commandsByCategories.get(category);
                if (typeof categoryCommands === `undefined`)
                    commandsByCategories.set(category, categoryCommands = []);
                const { usage } = this.getUsageByIndex(index);
                categoryCommands.push({ commandClass, usage });
            }
            const categoryNames = Array.from(commandsByCategories.keys()).sort((a, b) => {
                if (a === null)
                    return -1;
                if (b === null)
                    return +1;
                return a.localeCompare(b, `en`, { usage: `sort`, caseFirst: `upper` });
            });
            const hasLabel = typeof this.binaryLabel !== `undefined`;
            const hasVersion = typeof this.binaryVersion !== `undefined`;
            if (hasLabel || hasVersion) {
                if (hasLabel && hasVersion)
                    result += `${this.format(colored).header(`${this.binaryLabel} - ${this.binaryVersion}`)}\n\n`;
                else if (hasLabel)
                    result += `${this.format(colored).header(`${this.binaryLabel}`)}\n`;
                else
                    result += `${this.format(colored).header(`${this.binaryVersion}`)}\n`;
                result += `  ${this.format(colored).bold(prefix)}${this.binaryName} <command>\n`;
            }
            else {
                result += `${this.format(colored).bold(prefix)}${this.binaryName} <command>\n`;
            }
            for (const categoryName of categoryNames) {
                const commands = commandsByCategories.get(categoryName).slice().sort((a, b) => {
                    return a.usage.localeCompare(b.usage, `en`, { usage: `sort`, caseFirst: `upper` });
                });
                const header = categoryName !== null
                    ? categoryName.trim()
                    : `General commands`;
                result += `\n`;
                result += `${this.format(colored).header(`${header}`)}\n`;
                for (const { commandClass, usage } of commands) {
                    const doc = commandClass.usage.description || `undocumented`;
                    result += `\n`;
                    result += `  ${this.format(colored).bold(usage)}\n`;
                    result += `    ${format.formatMarkdownish(doc, { format: this.format(colored), paragraphs: false })}`;
                }
            }
            result += `\n`;
            result += format.formatMarkdownish(`You can also print more details about any of these commands by calling them with the \`-h,--help\` flag right after the command name.`, { format: this.format(colored), paragraphs: true });
        }
        else {
            if (!detailed) {
                const { usage } = this.getUsageByRegistration(commandClass);
                result += `${this.format(colored).bold(prefix)}${usage}\n`;
            }
            else {
                const { description = ``, details = ``, examples = [], } = commandClass.usage || {};
                if (description !== ``) {
                    result += format.formatMarkdownish(description, { format: this.format(colored), paragraphs: false }).replace(/^./, $0 => $0.toUpperCase());
                    result += `\n`;
                }
                if (details !== `` || examples.length > 0) {
                    result += `${this.format(colored).header(`Usage`)}\n`;
                    result += `\n`;
                }
                const { usage, options } = this.getUsageByRegistration(commandClass, { inlineOptions: false });
                result += `${this.format(colored).bold(prefix)}${usage}\n`;
                if (options.length > 0) {
                    result += `\n`;
                    result += `${format.richFormat.header(`Options`)}\n`;
                    const maxDefinitionLength = options.reduce((length, option) => {
                        return Math.max(length, option.definition.length);
                    }, 0);
                    result += `\n`;
                    for (const { definition, description } of options) {
                        result += `  ${this.format(colored).bold(definition.padEnd(maxDefinitionLength))}    ${format.formatMarkdownish(description, { format: this.format(colored), paragraphs: false })}`;
                    }
                }
                if (details !== ``) {
                    result += `\n`;
                    result += `${this.format(colored).header(`Details`)}\n`;
                    result += `\n`;
                    result += format.formatMarkdownish(details, { format: this.format(colored), paragraphs: true });
                }
                if (examples.length > 0) {
                    result += `\n`;
                    result += `${this.format(colored).header(`Examples`)}\n`;
                    for (const [description, example] of examples) {
                        result += `\n`;
                        result += format.formatMarkdownish(description, { format: this.format(colored), paragraphs: false });
                        result += `${example
                            .replace(/^/m, `  ${this.format(colored).bold(prefix)}`)
                            .replace(/\$0/g, this.binaryName)}\n`;
                    }
                }
            }
        }
        return result;
    }
    error(error, _a) {
        var _b;
        var { colored, command = (_b = error[errorCommandSymbol]) !== null && _b !== void 0 ? _b : null } = _a === void 0 ? {} : _a;
        if (!(error instanceof Error))
            error = new Error(`Execution failed with a non-error rejection (rejected value: ${JSON.stringify(error)})`);
        let result = ``;
        let name = error.name.replace(/([a-z])([A-Z])/g, `$1 $2`);
        if (name === `Error`)
            name = `Internal Error`;
        result += `${this.format(colored).error(name)}: ${error.message}\n`;
        const meta = error.clipanion;
        if (typeof meta !== `undefined`) {
            if (meta.type === `usage`) {
                result += `\n`;
                result += this.usage(command);
            }
        }
        else {
            if (error.stack) {
                result += `${error.stack.replace(/^.*\n/, ``)}\n`;
            }
        }
        return result;
    }
    getUsageByRegistration(klass, opts) {
        const record = this.registrations.get(klass);
        if (typeof record === `undefined`)
            throw new Error(`Assertion failed: Unregistered command`);
        return this.getUsageByIndex(record.index, opts);
    }
    getUsageByIndex(n, opts) {
        return this.builder.getBuilderByIndex(n).usage(opts);
    }
    format(colored = this.enableColors) {
        return colored ? format.richFormat : format.textFormat;
    }
}
/**
 * The default context of the CLI.
 *
 * Contains the stdio of the current `process`.
 */
Cli.defaultContext = {
    stdin: process.stdin,
    stdout: process.stdout,
    stderr: process.stderr,
};

exports.Cli = Cli;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Command.js":
/*!*************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Command.js ***!
  \*************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var utils = __webpack_require__(/*! ./options/utils.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js");

function _interopNamespace(e) {
    if (e && e.__esModule) return e;
    var n = Object.create(null);
    if (e) {
        Object.keys(e).forEach(function (k) {
            if (k !== 'default') {
                var d = Object.getOwnPropertyDescriptor(e, k);
                Object.defineProperty(n, k, d.get ? d : {
                    enumerable: true,
                    get: function () {
                        return e[k];
                    }
                });
            }
        });
    }
    n['default'] = e;
    return Object.freeze(n);
}

class Command {
    constructor() {
        /**
         * Predefined that will be set to true if `-h,--help` has been used, in
         * which case `Command#execute` won't be called.
         */
        this.help = false;
    }
    /**
     * Defines the usage information for the given command.
     */
    static Usage(usage) {
        return usage;
    }
    /**
     * Standard error handler which will simply rethrow the error. Can be used
     * to add custom logic to handle errors from the command or simply return
     * the parent class error handling.
     */
    async catch(error) {
        throw error;
    }
    async validateAndExecute() {
        const commandClass = this.constructor;
        const cascade = commandClass.schema;
        if (typeof cascade !== `undefined`) {
            const { isDict, isUnknown, applyCascade } = await Promise.resolve().then(function () { return /*#__PURE__*/_interopNamespace(__webpack_require__(/*! typanion */ "./.yarn/cache/typanion-npm-3.3.1-ee9997c7cf-212d2beb55.zip/node_modules/typanion/lib/index.js")); });
            const schema = applyCascade(isDict(isUnknown()), cascade);
            const errors = [];
            const coercions = [];
            const check = schema(this, { errors, coercions });
            if (!check)
                throw utils.formatError(`Invalid option schema`, errors);
            for (const [, op] of coercions) {
                op();
            }
        }
        const exitCode = await this.execute();
        if (typeof exitCode !== `undefined`) {
            return exitCode;
        }
        else {
            return 0;
        }
    }
}
/**
 * Used to detect option definitions.
 */
Command.isOption = utils.isOptionSymbol;
/**
 * Just an helper to use along with the `paths` fields, to make it
 * clearer that a command is the default one.
 *
 * @example
 * class MyCommand extends Command {
 *   static paths = [Command.Default];
 * }
 */
Command.Default = [];

exports.Command = Command;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/HelpCommand.js":
/*!*****************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/HelpCommand.js ***!
  \*****************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var Command = __webpack_require__(/*! ./Command.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Command.js");

class HelpCommand extends Command.Command {
    constructor(contexts) {
        super();
        this.contexts = contexts;
        this.commands = [];
    }
    static from(state, contexts) {
        const command = new HelpCommand(contexts);
        command.path = state.path;
        for (const opt of state.options) {
            switch (opt.name) {
                case `-c`:
                    {
                        command.commands.push(Number(opt.value));
                    }
                    break;
                case `-i`:
                    {
                        command.index = Number(opt.value);
                    }
                    break;
            }
        }
        return command;
    }
    async execute() {
        let commands = this.commands;
        if (typeof this.index !== `undefined` && this.index >= 0 && this.index < commands.length)
            commands = [commands[this.index]];
        if (commands.length === 0) {
            this.context.stdout.write(this.cli.usage());
        }
        else if (commands.length === 1) {
            this.context.stdout.write(this.cli.usage(this.contexts[commands[0]].commandClass, { detailed: true }));
        }
        else if (commands.length > 1) {
            this.context.stdout.write(`Multiple commands match your selection:\n`);
            this.context.stdout.write(`\n`);
            let index = 0;
            for (const command of this.commands)
                this.context.stdout.write(this.cli.usage(this.contexts[command].commandClass, { prefix: `${index++}. `.padStart(5) }));
            this.context.stdout.write(`\n`);
            this.context.stdout.write(`Run again with -h=<index> to see the longer details of any of those commands.\n`);
        }
    }
}

exports.HelpCommand = HelpCommand;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/definitions.js":
/*!**************************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/definitions.js ***!
  \**************************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var Command = __webpack_require__(/*! ../Command.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Command.js");

/**
 * A command that prints the clipanion definitions.
 */
class DefinitionsCommand extends Command.Command {
    async execute() {
        this.context.stdout.write(`${JSON.stringify(this.cli.definitions(), null, 2)}\n`);
    }
}
DefinitionsCommand.paths = [[`--clipanion=definitions`]];

exports.DefinitionsCommand = DefinitionsCommand;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/help.js":
/*!*******************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/help.js ***!
  \*******************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var Command = __webpack_require__(/*! ../Command.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Command.js");

/**
 * A command that prints the usage of all commands.
 *
 * Paths: `-h`, `--help`
 */
class HelpCommand extends Command.Command {
    async execute() {
        this.context.stdout.write(this.cli.usage());
    }
}
HelpCommand.paths = [[`-h`], [`--help`]];

exports.HelpCommand = HelpCommand;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/index.js":
/*!********************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/index.js ***!
  \********************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var definitions = __webpack_require__(/*! ./definitions.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/definitions.js");
var help = __webpack_require__(/*! ./help.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/help.js");
var version = __webpack_require__(/*! ./version.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/version.js");



exports.DefinitionsCommand = definitions.DefinitionsCommand;
exports.HelpCommand = help.HelpCommand;
exports.VersionCommand = version.VersionCommand;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/version.js":
/*!**********************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/version.js ***!
  \**********************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var Command = __webpack_require__(/*! ../Command.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Command.js");

/**
 * A command that prints the version of the binary (`cli.binaryVersion`).
 *
 * Paths: `-v`, `--version`
 */
class VersionCommand extends Command.Command {
    async execute() {
        var _a;
        this.context.stdout.write(`${(_a = this.cli.binaryVersion) !== null && _a !== void 0 ? _a : `<unknown>`}\n`);
    }
}
VersionCommand.paths = [[`-v`], [`--version`]];

exports.VersionCommand = VersionCommand;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js":
/*!***********************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js ***!
  \***********************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var errors = __webpack_require__(/*! ../errors.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/errors.js");
var Command = __webpack_require__(/*! ./Command.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Command.js");
var Cli = __webpack_require__(/*! ./Cli.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/Cli.js");
var index = __webpack_require__(/*! ./builtins/index.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/builtins/index.js");
var index$1 = __webpack_require__(/*! ./options/index.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/index.js");



exports.UsageError = errors.UsageError;
exports.Command = Command.Command;
exports.Cli = Cli.Cli;
exports.Builtins = index;
exports.Option = index$1;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Array.js":
/*!*******************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Array.js ***!
  \*******************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var utils = __webpack_require__(/*! ./utils.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js");

function Array(descriptor, initialValueBase, optsBase) {
    const [initialValue, opts] = utils.rerouteArguments(initialValueBase, optsBase !== null && optsBase !== void 0 ? optsBase : {});
    const { arity = 1 } = opts;
    const optNames = descriptor.split(`,`);
    const nameSet = new Set(optNames);
    return utils.makeCommandOption({
        definition(builder) {
            builder.addOption({
                names: optNames,
                arity,
                hidden: opts === null || opts === void 0 ? void 0 : opts.hidden,
                description: opts === null || opts === void 0 ? void 0 : opts.description,
                required: opts.required,
            });
        },
        transformer(builder, key, state) {
            let currentValue = typeof initialValue !== `undefined`
                ? [...initialValue]
                : undefined;
            for (const { name, value } of state.options) {
                if (!nameSet.has(name))
                    continue;
                currentValue = currentValue !== null && currentValue !== void 0 ? currentValue : [];
                currentValue.push(value);
            }
            return currentValue;
        },
    });
}

exports.Array = Array;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Boolean.js":
/*!*********************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Boolean.js ***!
  \*********************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var utils = __webpack_require__(/*! ./utils.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js");

function Boolean(descriptor, initialValueBase, optsBase) {
    const [initialValue, opts] = utils.rerouteArguments(initialValueBase, optsBase !== null && optsBase !== void 0 ? optsBase : {});
    const optNames = descriptor.split(`,`);
    const nameSet = new Set(optNames);
    return utils.makeCommandOption({
        definition(builder) {
            builder.addOption({
                names: optNames,
                allowBinding: false,
                arity: 0,
                hidden: opts.hidden,
                description: opts.description,
                required: opts.required,
            });
        },
        transformer(builer, key, state) {
            let currentValue = initialValue;
            for (const { name, value } of state.options) {
                if (!nameSet.has(name))
                    continue;
                currentValue = value;
            }
            return currentValue;
        },
    });
}

exports.Boolean = Boolean;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Counter.js":
/*!*********************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Counter.js ***!
  \*********************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var utils = __webpack_require__(/*! ./utils.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js");

function Counter(descriptor, initialValueBase, optsBase) {
    const [initialValue, opts] = utils.rerouteArguments(initialValueBase, optsBase !== null && optsBase !== void 0 ? optsBase : {});
    const optNames = descriptor.split(`,`);
    const nameSet = new Set(optNames);
    return utils.makeCommandOption({
        definition(builder) {
            builder.addOption({
                names: optNames,
                allowBinding: false,
                arity: 0,
                hidden: opts.hidden,
                description: opts.description,
                required: opts.required,
            });
        },
        transformer(builder, key, state) {
            let currentValue = initialValue;
            for (const { name, value } of state.options) {
                if (!nameSet.has(name))
                    continue;
                currentValue !== null && currentValue !== void 0 ? currentValue : (currentValue = 0);
                // Negated options reset the counter
                if (!value) {
                    currentValue = 0;
                }
                else {
                    currentValue += 1;
                }
            }
            return currentValue;
        },
    });
}

exports.Counter = Counter;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Proxy.js":
/*!*******************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Proxy.js ***!
  \*******************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var utils = __webpack_require__(/*! ./utils.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js");

/**
 * Used to annotate that the command wants to retrieve all trailing
 * arguments that cannot be tied to a declared option.
 *
 * Be careful: this function is order-dependent! Make sure to define it
 * after any positional argument you want to declare.
 *
 * This function is mutually exclusive with Option.Rest.
 *
 * @example
 * yarn run foo hello --foo=bar world
 *     ► proxy = ["hello", "--foo=bar", "world"]
 */
function Proxy(opts = {}) {
    return utils.makeCommandOption({
        definition(builder, key) {
            var _a;
            builder.addProxy({
                name: (_a = opts.name) !== null && _a !== void 0 ? _a : key,
                required: opts.required,
            });
        },
        transformer(builder, key, state) {
            return state.positionals.map(({ value }) => value);
        },
    });
}

exports.Proxy = Proxy;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Rest.js":
/*!******************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Rest.js ***!
  \******************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var utils = __webpack_require__(/*! ./utils.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js");
var core = __webpack_require__(/*! ../../core.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/core.js");

/**
 * Used to annotate that the command supports any number of positional
 * arguments.
 *
 * Be careful: this function is order-dependent! Make sure to define it
 * after any positional argument you want to declare.
 *
 * This function is mutually exclusive with Option.Proxy.
 *
 * @example
 * yarn add hello world
 *     ► rest = ["hello", "world"]
 */
function Rest(opts = {}) {
    return utils.makeCommandOption({
        definition(builder, key) {
            var _a;
            builder.addRest({
                name: (_a = opts.name) !== null && _a !== void 0 ? _a : key,
                required: opts.required,
            });
        },
        transformer(builder, key, state) {
            // The builder's arity.extra will always be NoLimits,
            // because it is set when we call registerDefinition
            const isRestPositional = (index) => {
                const positional = state.positionals[index];
                // A NoLimits extra (i.e. an optional rest argument)
                if (positional.extra === core.NoLimits)
                    return true;
                // A leading positional (i.e. a required rest argument)
                if (positional.extra === false && index < builder.arity.leading.length)
                    return true;
                return false;
            };
            let count = 0;
            while (count < state.positionals.length && isRestPositional(count))
                count += 1;
            return state.positionals.splice(0, count).map(({ value }) => value);
        },
    });
}

exports.Rest = Rest;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/String.js":
/*!********************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/String.js ***!
  \********************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var utils = __webpack_require__(/*! ./utils.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js");
var core = __webpack_require__(/*! ../../core.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/core.js");

function StringOption(descriptor, initialValueBase, optsBase) {
    const [initialValue, opts] = utils.rerouteArguments(initialValueBase, optsBase !== null && optsBase !== void 0 ? optsBase : {});
    const { arity = 1 } = opts;
    const optNames = descriptor.split(`,`);
    const nameSet = new Set(optNames);
    return utils.makeCommandOption({
        definition(builder) {
            builder.addOption({
                names: optNames,
                arity: opts.tolerateBoolean ? 0 : arity,
                hidden: opts.hidden,
                description: opts.description,
                required: opts.required,
            });
        },
        transformer(builder, key, state) {
            let usedName;
            let currentValue = initialValue;
            for (const { name, value } of state.options) {
                if (!nameSet.has(name))
                    continue;
                usedName = name;
                currentValue = value;
            }
            if (typeof currentValue === `string`) {
                return utils.applyValidator(usedName !== null && usedName !== void 0 ? usedName : key, currentValue, opts.validator);
            }
            else {
                return currentValue;
            }
        },
    });
}
function StringPositional(opts = {}) {
    const { required = true } = opts;
    return utils.makeCommandOption({
        definition(builder, key) {
            var _a;
            builder.addPositional({
                name: (_a = opts.name) !== null && _a !== void 0 ? _a : key,
                required: opts.required,
            });
        },
        transformer(builder, key, state) {
            var _a;
            for (let i = 0; i < state.positionals.length; ++i) {
                // We skip NoLimits extras. We only care about
                // required and optional finite positionals.
                if (state.positionals[i].extra === core.NoLimits)
                    continue;
                // We skip optional positionals when we only
                // care about required positionals.
                if (required && state.positionals[i].extra === true)
                    continue;
                // We skip required positionals when we only
                // care about optional positionals.
                if (!required && state.positionals[i].extra === false)
                    continue;
                // We remove the positional from the list
                const [positional] = state.positionals.splice(i, 1);
                return utils.applyValidator((_a = opts.name) !== null && _a !== void 0 ? _a : key, positional.value, opts.validator);
            }
            return undefined;
        },
    });
}
// This function is badly typed, but it doesn't matter because the overloads provide the true public typings
function String(descriptor, ...args) {
    if (typeof descriptor === `string`) {
        return StringOption(descriptor, ...args);
    }
    else {
        return StringPositional(descriptor);
    }
}

exports.String = String;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/index.js":
/*!*******************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/index.js ***!
  \*******************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var utils = __webpack_require__(/*! ./utils.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js");
var _Array = __webpack_require__(/*! ./Array.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Array.js");
var _Boolean = __webpack_require__(/*! ./Boolean.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Boolean.js");
var Counter = __webpack_require__(/*! ./Counter.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Counter.js");
var _Proxy = __webpack_require__(/*! ./Proxy.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Proxy.js");
var Rest = __webpack_require__(/*! ./Rest.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/Rest.js");
var _String = __webpack_require__(/*! ./String.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/String.js");



exports.applyValidator = utils.applyValidator;
exports.cleanValidationError = utils.cleanValidationError;
exports.formatError = utils.formatError;
exports.isOptionSymbol = utils.isOptionSymbol;
exports.makeCommandOption = utils.makeCommandOption;
exports.rerouteArguments = utils.rerouteArguments;
exports.Array = _Array.Array;
exports.Boolean = _Boolean.Boolean;
exports.Counter = Counter.Counter;
exports.Proxy = _Proxy.Proxy;
exports.Rest = Rest.Rest;
exports.String = _String.String;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js":
/*!*******************************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/options/utils.js ***!
  \*******************************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var errors = __webpack_require__(/*! ../../errors.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/errors.js");

const isOptionSymbol = Symbol(`clipanion/isOption`);
function makeCommandOption(spec) {
    // We lie! But it's for the good cause: the cli engine will turn the specs into proper values after instantiation.
    return { ...spec, [isOptionSymbol]: true };
}
function rerouteArguments(a, b) {
    if (typeof a === `undefined`)
        return [a, b];
    if (typeof a === `object` && a !== null && !Array.isArray(a)) {
        return [undefined, a];
    }
    else {
        return [a, b];
    }
}
function cleanValidationError(message, lowerCase = false) {
    let cleaned = message.replace(/^\.: /, ``);
    if (lowerCase)
        cleaned = cleaned[0].toLowerCase() + cleaned.slice(1);
    return cleaned;
}
function formatError(message, errors$1) {
    if (errors$1.length === 1) {
        return new errors.UsageError(`${message}: ${cleanValidationError(errors$1[0], true)}`);
    }
    else {
        return new errors.UsageError(`${message}:\n${errors$1.map(error => `\n- ${cleanValidationError(error)}`).join(``)}`);
    }
}
function applyValidator(name, value, validator) {
    if (typeof validator === `undefined`)
        return value;
    const errors = [];
    const coercions = [];
    const coercion = (v) => {
        const orig = value;
        value = v;
        return coercion.bind(null, orig);
    };
    const check = validator(value, { errors, coercions, coercion });
    if (!check)
        throw formatError(`Invalid value for ${name}`, errors);
    for (const [, op] of coercions)
        op();
    return value;
}

exports.applyValidator = applyValidator;
exports.cleanValidationError = cleanValidationError;
exports.formatError = formatError;
exports.isOptionSymbol = isOptionSymbol;
exports.makeCommandOption = makeCommandOption;
exports.rerouteArguments = rerouteArguments;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/constants.js":
/*!******************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/constants.js ***!
  \******************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

const NODE_INITIAL = 0;
const NODE_SUCCESS = 1;
const NODE_ERRORED = 2;
const START_OF_INPUT = `\u0001`;
const END_OF_INPUT = `\u0000`;
const HELP_COMMAND_INDEX = -1;
const HELP_REGEX = /^(-h|--help)(?:=([0-9]+))?$/;
const OPTION_REGEX = /^(--[a-z]+(?:-[a-z]+)*|-[a-zA-Z]+)$/;
const BATCH_REGEX = /^-[a-zA-Z]{2,}$/;
const BINDING_REGEX = /^([^=]+)=([\s\S]*)$/;
const DEBUG = process.env.DEBUG_CLI === `1`;

exports.BATCH_REGEX = BATCH_REGEX;
exports.BINDING_REGEX = BINDING_REGEX;
exports.DEBUG = DEBUG;
exports.END_OF_INPUT = END_OF_INPUT;
exports.HELP_COMMAND_INDEX = HELP_COMMAND_INDEX;
exports.HELP_REGEX = HELP_REGEX;
exports.NODE_ERRORED = NODE_ERRORED;
exports.NODE_INITIAL = NODE_INITIAL;
exports.NODE_SUCCESS = NODE_SUCCESS;
exports.OPTION_REGEX = OPTION_REGEX;
exports.START_OF_INPUT = START_OF_INPUT;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/core.js":
/*!*************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/core.js ***!
  \*************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var constants = __webpack_require__(/*! ./constants.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/constants.js");
var errors = __webpack_require__(/*! ./errors.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/errors.js");

// ------------------------------------------------------------------------
function debug(str) {
    if (constants.DEBUG) {
        console.log(str);
    }
}
const basicHelpState = {
    candidateUsage: null,
    requiredOptions: [],
    errorMessage: null,
    ignoreOptions: false,
    path: [],
    positionals: [],
    options: [],
    remainder: null,
    selectedIndex: constants.HELP_COMMAND_INDEX,
};
function makeStateMachine() {
    return {
        nodes: [makeNode(), makeNode(), makeNode()],
    };
}
function makeAnyOfMachine(inputs) {
    const output = makeStateMachine();
    const heads = [];
    let offset = output.nodes.length;
    for (const input of inputs) {
        heads.push(offset);
        for (let t = 0; t < input.nodes.length; ++t)
            if (!isTerminalNode(t))
                output.nodes.push(cloneNode(input.nodes[t], offset));
        offset += input.nodes.length - 2;
    }
    for (const head of heads)
        registerShortcut(output, constants.NODE_INITIAL, head);
    return output;
}
function injectNode(machine, node) {
    machine.nodes.push(node);
    return machine.nodes.length - 1;
}
function simplifyMachine(input) {
    const visited = new Set();
    const process = (node) => {
        if (visited.has(node))
            return;
        visited.add(node);
        const nodeDef = input.nodes[node];
        for (const transitions of Object.values(nodeDef.statics))
            for (const { to } of transitions)
                process(to);
        for (const [, { to }] of nodeDef.dynamics)
            process(to);
        for (const { to } of nodeDef.shortcuts)
            process(to);
        const shortcuts = new Set(nodeDef.shortcuts.map(({ to }) => to));
        while (nodeDef.shortcuts.length > 0) {
            const { to } = nodeDef.shortcuts.shift();
            const toDef = input.nodes[to];
            for (const [segment, transitions] of Object.entries(toDef.statics)) {
                const store = !Object.prototype.hasOwnProperty.call(nodeDef.statics, segment)
                    ? nodeDef.statics[segment] = []
                    : nodeDef.statics[segment];
                for (const transition of transitions) {
                    if (!store.some(({ to }) => transition.to === to)) {
                        store.push(transition);
                    }
                }
            }
            for (const [test, transition] of toDef.dynamics)
                if (!nodeDef.dynamics.some(([otherTest, { to }]) => test === otherTest && transition.to === to))
                    nodeDef.dynamics.push([test, transition]);
            for (const transition of toDef.shortcuts) {
                if (!shortcuts.has(transition.to)) {
                    nodeDef.shortcuts.push(transition);
                    shortcuts.add(transition.to);
                }
            }
        }
    };
    process(constants.NODE_INITIAL);
}
function debugMachine(machine, { prefix = `` } = {}) {
    // Don't iterate unless it's needed
    if (constants.DEBUG) {
        debug(`${prefix}Nodes are:`);
        for (let t = 0; t < machine.nodes.length; ++t) {
            debug(`${prefix}  ${t}: ${JSON.stringify(machine.nodes[t])}`);
        }
    }
}
function runMachineInternal(machine, input, partial = false) {
    debug(`Running a vm on ${JSON.stringify(input)}`);
    let branches = [{ node: constants.NODE_INITIAL, state: {
                candidateUsage: null,
                requiredOptions: [],
                errorMessage: null,
                ignoreOptions: false,
                options: [],
                path: [],
                positionals: [],
                remainder: null,
                selectedIndex: null,
            } }];
    debugMachine(machine, { prefix: `  ` });
    const tokens = [constants.START_OF_INPUT, ...input];
    for (let t = 0; t < tokens.length; ++t) {
        const segment = tokens[t];
        debug(`  Processing ${JSON.stringify(segment)}`);
        const nextBranches = [];
        for (const { node, state } of branches) {
            debug(`    Current node is ${node}`);
            const nodeDef = machine.nodes[node];
            if (node === constants.NODE_ERRORED) {
                nextBranches.push({ node, state });
                continue;
            }
            console.assert(nodeDef.shortcuts.length === 0, `Shortcuts should have been eliminated by now`);
            const hasExactMatch = Object.prototype.hasOwnProperty.call(nodeDef.statics, segment);
            if (!partial || t < tokens.length - 1 || hasExactMatch) {
                if (hasExactMatch) {
                    const transitions = nodeDef.statics[segment];
                    for (const { to, reducer } of transitions) {
                        nextBranches.push({ node: to, state: typeof reducer !== `undefined` ? execute(reducers, reducer, state, segment) : state });
                        debug(`      Static transition to ${to} found`);
                    }
                }
                else {
                    debug(`      No static transition found`);
                }
            }
            else {
                let hasMatches = false;
                for (const candidate of Object.keys(nodeDef.statics)) {
                    if (!candidate.startsWith(segment))
                        continue;
                    if (segment === candidate) {
                        for (const { to, reducer } of nodeDef.statics[candidate]) {
                            nextBranches.push({ node: to, state: typeof reducer !== `undefined` ? execute(reducers, reducer, state, segment) : state });
                            debug(`      Static transition to ${to} found`);
                        }
                    }
                    else {
                        for (const { to } of nodeDef.statics[candidate]) {
                            nextBranches.push({ node: to, state: { ...state, remainder: candidate.slice(segment.length) } });
                            debug(`      Static transition to ${to} found (partial match)`);
                        }
                    }
                    hasMatches = true;
                }
                if (!hasMatches) {
                    debug(`      No partial static transition found`);
                }
            }
            if (segment !== constants.END_OF_INPUT) {
                for (const [test, { to, reducer }] of nodeDef.dynamics) {
                    if (execute(tests, test, state, segment)) {
                        nextBranches.push({ node: to, state: typeof reducer !== `undefined` ? execute(reducers, reducer, state, segment) : state });
                        debug(`      Dynamic transition to ${to} found (via ${test})`);
                    }
                }
            }
        }
        if (nextBranches.length === 0 && segment === constants.END_OF_INPUT && input.length === 1) {
            return [{
                    node: constants.NODE_INITIAL,
                    state: basicHelpState,
                }];
        }
        if (nextBranches.length === 0) {
            throw new errors.UnknownSyntaxError(input, branches.filter(({ node }) => {
                return node !== constants.NODE_ERRORED;
            }).map(({ state }) => {
                return { usage: state.candidateUsage, reason: null };
            }));
        }
        if (nextBranches.every(({ node }) => node === constants.NODE_ERRORED)) {
            throw new errors.UnknownSyntaxError(input, nextBranches.map(({ state }) => {
                return { usage: state.candidateUsage, reason: state.errorMessage };
            }));
        }
        branches = trimSmallerBranches(nextBranches);
    }
    if (branches.length > 0) {
        debug(`  Results:`);
        for (const branch of branches) {
            debug(`    - ${branch.node} -> ${JSON.stringify(branch.state)}`);
        }
    }
    else {
        debug(`  No results`);
    }
    return branches;
}
function checkIfNodeIsFinished(node, state) {
    if (state.selectedIndex !== null)
        return true;
    if (Object.prototype.hasOwnProperty.call(node.statics, constants.END_OF_INPUT))
        for (const { to } of node.statics[constants.END_OF_INPUT])
            if (to === constants.NODE_SUCCESS)
                return true;
    return false;
}
function suggestMachine(machine, input, partial) {
    // If we're accepting partial matches, then exact matches need to be
    // prefixed with an extra space.
    const prefix = partial && input.length > 0 ? [``] : [];
    const branches = runMachineInternal(machine, input, partial);
    const suggestions = [];
    const suggestionsJson = new Set();
    const traverseSuggestion = (suggestion, node, skipFirst = true) => {
        let nextNodes = [node];
        while (nextNodes.length > 0) {
            const currentNodes = nextNodes;
            nextNodes = [];
            for (const node of currentNodes) {
                const nodeDef = machine.nodes[node];
                const keys = Object.keys(nodeDef.statics);
                // The fact that `key` is unused is likely a bug, but no one has investigated it yet.
                // TODO: Investigate it.
                // eslint-disable-next-line @typescript-eslint/no-unused-vars
                for (const key of Object.keys(nodeDef.statics)) {
                    const segment = keys[0];
                    for (const { to, reducer } of nodeDef.statics[segment]) {
                        if (reducer !== `pushPath`)
                            continue;
                        if (!skipFirst)
                            suggestion.push(segment);
                        nextNodes.push(to);
                    }
                }
            }
            skipFirst = false;
        }
        const json = JSON.stringify(suggestion);
        if (suggestionsJson.has(json))
            return;
        suggestions.push(suggestion);
        suggestionsJson.add(json);
    };
    for (const { node, state } of branches) {
        if (state.remainder !== null) {
            traverseSuggestion([state.remainder], node);
            continue;
        }
        const nodeDef = machine.nodes[node];
        const isFinished = checkIfNodeIsFinished(nodeDef, state);
        for (const [candidate, transitions] of Object.entries(nodeDef.statics))
            if ((isFinished && candidate !== constants.END_OF_INPUT) || (!candidate.startsWith(`-`) && transitions.some(({ reducer }) => reducer === `pushPath`)))
                traverseSuggestion([...prefix, candidate], node);
        if (!isFinished)
            continue;
        for (const [test, { to }] of nodeDef.dynamics) {
            if (to === constants.NODE_ERRORED)
                continue;
            const tokens = suggest(test, state);
            if (tokens === null)
                continue;
            for (const token of tokens) {
                traverseSuggestion([...prefix, token], node);
            }
        }
    }
    return [...suggestions].sort();
}
function runMachine(machine, input) {
    const branches = runMachineInternal(machine, [...input, constants.END_OF_INPUT]);
    return selectBestState(input, branches.map(({ state }) => {
        return state;
    }));
}
function trimSmallerBranches(branches) {
    let maxPathSize = 0;
    for (const { state } of branches)
        if (state.path.length > maxPathSize)
            maxPathSize = state.path.length;
    return branches.filter(({ state }) => {
        return state.path.length === maxPathSize;
    });
}
function selectBestState(input, states) {
    const terminalStates = states.filter(state => {
        return state.selectedIndex !== null;
    });
    if (terminalStates.length === 0)
        throw new Error();
    const requiredOptionsSetStates = terminalStates.filter(state => state.requiredOptions.every(names => names.some(name => state.options.find(opt => opt.name === name))));
    if (requiredOptionsSetStates.length === 0) {
        throw new errors.UnknownSyntaxError(input, terminalStates.map(state => ({
            usage: state.candidateUsage,
            reason: null,
        })));
    }
    let maxPathSize = 0;
    for (const state of requiredOptionsSetStates)
        if (state.path.length > maxPathSize)
            maxPathSize = state.path.length;
    const bestPathBranches = requiredOptionsSetStates.filter(state => {
        return state.path.length === maxPathSize;
    });
    const getPositionalCount = (state) => state.positionals.filter(({ extra }) => {
        return !extra;
    }).length + state.options.length;
    const statesWithPositionalCount = bestPathBranches.map(state => {
        return { state, positionalCount: getPositionalCount(state) };
    });
    let maxPositionalCount = 0;
    for (const { positionalCount } of statesWithPositionalCount)
        if (positionalCount > maxPositionalCount)
            maxPositionalCount = positionalCount;
    const bestPositionalStates = statesWithPositionalCount.filter(({ positionalCount }) => {
        return positionalCount === maxPositionalCount;
    }).map(({ state }) => {
        return state;
    });
    const fixedStates = aggregateHelpStates(bestPositionalStates);
    if (fixedStates.length > 1)
        throw new errors.AmbiguousSyntaxError(input, fixedStates.map(state => state.candidateUsage));
    return fixedStates[0];
}
function aggregateHelpStates(states) {
    const notHelps = [];
    const helps = [];
    for (const state of states) {
        if (state.selectedIndex === constants.HELP_COMMAND_INDEX) {
            helps.push(state);
        }
        else {
            notHelps.push(state);
        }
    }
    if (helps.length > 0) {
        notHelps.push({
            ...basicHelpState,
            path: findCommonPrefix(...helps.map(state => state.path)),
            options: helps.reduce((options, state) => options.concat(state.options), []),
        });
    }
    return notHelps;
}
function findCommonPrefix(firstPath, secondPath, ...rest) {
    if (secondPath === undefined)
        return Array.from(firstPath);
    return findCommonPrefix(firstPath.filter((segment, i) => segment === secondPath[i]), ...rest);
}
function makeNode() {
    return {
        dynamics: [],
        shortcuts: [],
        statics: {},
    };
}
function isTerminalNode(node) {
    return node === constants.NODE_SUCCESS || node === constants.NODE_ERRORED;
}
function cloneTransition(input, offset = 0) {
    return {
        to: !isTerminalNode(input.to) ? input.to > 2 ? input.to + offset - 2 : input.to + offset : input.to,
        reducer: input.reducer,
    };
}
function cloneNode(input, offset = 0) {
    const output = makeNode();
    for (const [test, transition] of input.dynamics)
        output.dynamics.push([test, cloneTransition(transition, offset)]);
    for (const transition of input.shortcuts)
        output.shortcuts.push(cloneTransition(transition, offset));
    for (const [segment, transitions] of Object.entries(input.statics))
        output.statics[segment] = transitions.map(transition => cloneTransition(transition, offset));
    return output;
}
function registerDynamic(machine, from, test, to, reducer) {
    machine.nodes[from].dynamics.push([
        test,
        { to, reducer: reducer },
    ]);
}
function registerShortcut(machine, from, to, reducer) {
    machine.nodes[from].shortcuts.push({ to, reducer: reducer });
}
function registerStatic(machine, from, test, to, reducer) {
    const store = !Object.prototype.hasOwnProperty.call(machine.nodes[from].statics, test)
        ? machine.nodes[from].statics[test] = []
        : machine.nodes[from].statics[test];
    store.push({ to, reducer: reducer });
}
function execute(store, callback, state, segment) {
    // TypeScript's control flow can't properly narrow
    // generic conditionals for some mysterious reason
    if (Array.isArray(callback)) {
        const [name, ...args] = callback;
        return store[name](state, segment, ...args);
    }
    else {
        return store[callback](state, segment);
    }
}
function suggest(callback, state) {
    const fn = Array.isArray(callback)
        ? tests[callback[0]]
        : tests[callback];
    // @ts-ignore
    if (typeof fn.suggest === `undefined`)
        return null;
    const args = Array.isArray(callback)
        ? callback.slice(1)
        : [];
    // @ts-ignore
    return fn.suggest(state, ...args);
}
const tests = {
    always: () => {
        return true;
    },
    isOptionLike: (state, segment) => {
        return !state.ignoreOptions && (segment !== `-` && segment.startsWith(`-`));
    },
    isNotOptionLike: (state, segment) => {
        return state.ignoreOptions || segment === `-` || !segment.startsWith(`-`);
    },
    isOption: (state, segment, name, hidden) => {
        return !state.ignoreOptions && segment === name;
    },
    isBatchOption: (state, segment, names) => {
        return !state.ignoreOptions && constants.BATCH_REGEX.test(segment) && [...segment.slice(1)].every(name => names.includes(`-${name}`));
    },
    isBoundOption: (state, segment, names, options) => {
        const optionParsing = segment.match(constants.BINDING_REGEX);
        return !state.ignoreOptions && !!optionParsing && constants.OPTION_REGEX.test(optionParsing[1]) && names.includes(optionParsing[1])
            // Disallow bound options with no arguments (i.e. booleans)
            && options.filter(opt => opt.names.includes(optionParsing[1])).every(opt => opt.allowBinding);
    },
    isNegatedOption: (state, segment, name) => {
        return !state.ignoreOptions && segment === `--no-${name.slice(2)}`;
    },
    isHelp: (state, segment) => {
        return !state.ignoreOptions && constants.HELP_REGEX.test(segment);
    },
    isUnsupportedOption: (state, segment, names) => {
        return !state.ignoreOptions && segment.startsWith(`-`) && constants.OPTION_REGEX.test(segment) && !names.includes(segment);
    },
    isInvalidOption: (state, segment) => {
        return !state.ignoreOptions && segment.startsWith(`-`) && !constants.OPTION_REGEX.test(segment);
    },
};
// @ts-ignore
tests.isOption.suggest = (state, name, hidden = true) => {
    return !hidden ? [name] : null;
};
const reducers = {
    setCandidateState: (state, segment, candidateState) => {
        return { ...state, ...candidateState };
    },
    setSelectedIndex: (state, segment, index) => {
        return { ...state, selectedIndex: index };
    },
    pushBatch: (state, segment) => {
        return { ...state, options: state.options.concat([...segment.slice(1)].map(name => ({ name: `-${name}`, value: true }))) };
    },
    pushBound: (state, segment) => {
        const [, name, value] = segment.match(constants.BINDING_REGEX);
        return { ...state, options: state.options.concat({ name, value }) };
    },
    pushPath: (state, segment) => {
        return { ...state, path: state.path.concat(segment) };
    },
    pushPositional: (state, segment) => {
        return { ...state, positionals: state.positionals.concat({ value: segment, extra: false }) };
    },
    pushExtra: (state, segment) => {
        return { ...state, positionals: state.positionals.concat({ value: segment, extra: true }) };
    },
    pushExtraNoLimits: (state, segment) => {
        return { ...state, positionals: state.positionals.concat({ value: segment, extra: NoLimits }) };
    },
    pushTrue: (state, segment, name = segment) => {
        return { ...state, options: state.options.concat({ name: segment, value: true }) };
    },
    pushFalse: (state, segment, name = segment) => {
        return { ...state, options: state.options.concat({ name, value: false }) };
    },
    pushUndefined: (state, segment) => {
        return { ...state, options: state.options.concat({ name: segment, value: undefined }) };
    },
    pushStringValue: (state, segment) => {
        var _a;
        const copy = { ...state, options: [...state.options] };
        const lastOption = state.options[state.options.length - 1];
        lastOption.value = ((_a = lastOption.value) !== null && _a !== void 0 ? _a : []).concat([segment]);
        return copy;
    },
    setStringValue: (state, segment) => {
        const copy = { ...state, options: [...state.options] };
        const lastOption = state.options[state.options.length - 1];
        lastOption.value = segment;
        return copy;
    },
    inhibateOptions: (state) => {
        return { ...state, ignoreOptions: true };
    },
    useHelp: (state, segment, command) => {
        const [, /* name */ , index] = segment.match(constants.HELP_REGEX);
        if (typeof index !== `undefined`) {
            return { ...state, options: [{ name: `-c`, value: String(command) }, { name: `-i`, value: index }] };
        }
        else {
            return { ...state, options: [{ name: `-c`, value: String(command) }] };
        }
    },
    setError: (state, segment, errorMessage) => {
        if (segment === constants.END_OF_INPUT) {
            return { ...state, errorMessage: `${errorMessage}.` };
        }
        else {
            return { ...state, errorMessage: `${errorMessage} ("${segment}").` };
        }
    },
    setOptionArityError: (state, segment) => {
        const lastOption = state.options[state.options.length - 1];
        return { ...state, errorMessage: `Not enough arguments to option ${lastOption.name}.` };
    },
};
// ------------------------------------------------------------------------
const NoLimits = Symbol();
class CommandBuilder {
    constructor(cliIndex, cliOpts) {
        this.allOptionNames = [];
        this.arity = { leading: [], trailing: [], extra: [], proxy: false };
        this.options = [];
        this.paths = [];
        this.cliIndex = cliIndex;
        this.cliOpts = cliOpts;
    }
    addPath(path) {
        this.paths.push(path);
    }
    setArity({ leading = this.arity.leading, trailing = this.arity.trailing, extra = this.arity.extra, proxy = this.arity.proxy }) {
        Object.assign(this.arity, { leading, trailing, extra, proxy });
    }
    addPositional({ name = `arg`, required = true } = {}) {
        if (!required && this.arity.extra === NoLimits)
            throw new Error(`Optional parameters cannot be declared when using .rest() or .proxy()`);
        if (!required && this.arity.trailing.length > 0)
            throw new Error(`Optional parameters cannot be declared after the required trailing positional arguments`);
        if (!required && this.arity.extra !== NoLimits) {
            this.arity.extra.push(name);
        }
        else if (this.arity.extra !== NoLimits && this.arity.extra.length === 0) {
            this.arity.leading.push(name);
        }
        else {
            this.arity.trailing.push(name);
        }
    }
    addRest({ name = `arg`, required = 0 } = {}) {
        if (this.arity.extra === NoLimits)
            throw new Error(`Infinite lists cannot be declared multiple times in the same command`);
        if (this.arity.trailing.length > 0)
            throw new Error(`Infinite lists cannot be declared after the required trailing positional arguments`);
        for (let t = 0; t < required; ++t)
            this.addPositional({ name });
        this.arity.extra = NoLimits;
    }
    addProxy({ required = 0 } = {}) {
        this.addRest({ required });
        this.arity.proxy = true;
    }
    addOption({ names, description, arity = 0, hidden = false, required = false, allowBinding = true }) {
        if (!allowBinding && arity > 1)
            throw new Error(`The arity cannot be higher than 1 when the option only supports the --arg=value syntax`);
        if (!Number.isInteger(arity))
            throw new Error(`The arity must be an integer, got ${arity}`);
        if (arity < 0)
            throw new Error(`The arity must be positive, got ${arity}`);
        this.allOptionNames.push(...names);
        this.options.push({ names, description, arity, hidden, required, allowBinding });
    }
    setContext(context) {
        this.context = context;
    }
    usage({ detailed = true, inlineOptions = true } = {}) {
        const segments = [this.cliOpts.binaryName];
        const detailedOptionList = [];
        if (this.paths.length > 0)
            segments.push(...this.paths[0]);
        if (detailed) {
            for (const { names, arity, hidden, description, required } of this.options) {
                if (hidden)
                    continue;
                const args = [];
                for (let t = 0; t < arity; ++t)
                    args.push(` #${t}`);
                const definition = `${names.join(`,`)}${args.join(``)}`;
                if (!inlineOptions && description) {
                    detailedOptionList.push({ definition, description, required });
                }
                else {
                    segments.push(required ? `<${definition}>` : `[${definition}]`);
                }
            }
            segments.push(...this.arity.leading.map(name => `<${name}>`));
            if (this.arity.extra === NoLimits)
                segments.push(`...`);
            else
                segments.push(...this.arity.extra.map(name => `[${name}]`));
            segments.push(...this.arity.trailing.map(name => `<${name}>`));
        }
        const usage = segments.join(` `);
        return { usage, options: detailedOptionList };
    }
    compile() {
        if (typeof this.context === `undefined`)
            throw new Error(`Assertion failed: No context attached`);
        const machine = makeStateMachine();
        let firstNode = constants.NODE_INITIAL;
        const candidateUsage = this.usage().usage;
        const requiredOptions = this.options
            .filter(opt => opt.required)
            .map(opt => opt.names);
        firstNode = injectNode(machine, makeNode());
        registerStatic(machine, constants.NODE_INITIAL, constants.START_OF_INPUT, firstNode, [`setCandidateState`, { candidateUsage, requiredOptions }]);
        const positionalArgument = this.arity.proxy
            ? `always`
            : `isNotOptionLike`;
        const paths = this.paths.length > 0
            ? this.paths
            : [[]];
        for (const path of paths) {
            let lastPathNode = firstNode;
            // We allow options to be specified before the path. Note that we
            // only do this when there is a path, otherwise there would be
            // some redundancy with the options attached later.
            if (path.length > 0) {
                const optionPathNode = injectNode(machine, makeNode());
                registerShortcut(machine, lastPathNode, optionPathNode);
                this.registerOptions(machine, optionPathNode);
                lastPathNode = optionPathNode;
            }
            for (let t = 0; t < path.length; ++t) {
                const nextPathNode = injectNode(machine, makeNode());
                registerStatic(machine, lastPathNode, path[t], nextPathNode, `pushPath`);
                lastPathNode = nextPathNode;
            }
            if (this.arity.leading.length > 0 || !this.arity.proxy) {
                const helpNode = injectNode(machine, makeNode());
                registerDynamic(machine, lastPathNode, `isHelp`, helpNode, [`useHelp`, this.cliIndex]);
                registerStatic(machine, helpNode, constants.END_OF_INPUT, constants.NODE_SUCCESS, [`setSelectedIndex`, constants.HELP_COMMAND_INDEX]);
                this.registerOptions(machine, lastPathNode);
            }
            if (this.arity.leading.length > 0)
                registerStatic(machine, lastPathNode, constants.END_OF_INPUT, constants.NODE_ERRORED, [`setError`, `Not enough positional arguments`]);
            let lastLeadingNode = lastPathNode;
            for (let t = 0; t < this.arity.leading.length; ++t) {
                const nextLeadingNode = injectNode(machine, makeNode());
                if (!this.arity.proxy)
                    this.registerOptions(machine, nextLeadingNode);
                if (this.arity.trailing.length > 0 || t + 1 !== this.arity.leading.length)
                    registerStatic(machine, nextLeadingNode, constants.END_OF_INPUT, constants.NODE_ERRORED, [`setError`, `Not enough positional arguments`]);
                registerDynamic(machine, lastLeadingNode, `isNotOptionLike`, nextLeadingNode, `pushPositional`);
                lastLeadingNode = nextLeadingNode;
            }
            let lastExtraNode = lastLeadingNode;
            if (this.arity.extra === NoLimits || this.arity.extra.length > 0) {
                const extraShortcutNode = injectNode(machine, makeNode());
                registerShortcut(machine, lastLeadingNode, extraShortcutNode);
                if (this.arity.extra === NoLimits) {
                    const extraNode = injectNode(machine, makeNode());
                    if (!this.arity.proxy)
                        this.registerOptions(machine, extraNode);
                    registerDynamic(machine, lastLeadingNode, positionalArgument, extraNode, `pushExtraNoLimits`);
                    registerDynamic(machine, extraNode, positionalArgument, extraNode, `pushExtraNoLimits`);
                    registerShortcut(machine, extraNode, extraShortcutNode);
                }
                else {
                    for (let t = 0; t < this.arity.extra.length; ++t) {
                        const nextExtraNode = injectNode(machine, makeNode());
                        if (!this.arity.proxy)
                            this.registerOptions(machine, nextExtraNode);
                        registerDynamic(machine, lastExtraNode, positionalArgument, nextExtraNode, `pushExtra`);
                        registerShortcut(machine, nextExtraNode, extraShortcutNode);
                        lastExtraNode = nextExtraNode;
                    }
                }
                lastExtraNode = extraShortcutNode;
            }
            if (this.arity.trailing.length > 0)
                registerStatic(machine, lastExtraNode, constants.END_OF_INPUT, constants.NODE_ERRORED, [`setError`, `Not enough positional arguments`]);
            let lastTrailingNode = lastExtraNode;
            for (let t = 0; t < this.arity.trailing.length; ++t) {
                const nextTrailingNode = injectNode(machine, makeNode());
                if (!this.arity.proxy)
                    this.registerOptions(machine, nextTrailingNode);
                if (t + 1 < this.arity.trailing.length)
                    registerStatic(machine, nextTrailingNode, constants.END_OF_INPUT, constants.NODE_ERRORED, [`setError`, `Not enough positional arguments`]);
                registerDynamic(machine, lastTrailingNode, `isNotOptionLike`, nextTrailingNode, `pushPositional`);
                lastTrailingNode = nextTrailingNode;
            }
            registerDynamic(machine, lastTrailingNode, positionalArgument, constants.NODE_ERRORED, [`setError`, `Extraneous positional argument`]);
            registerStatic(machine, lastTrailingNode, constants.END_OF_INPUT, constants.NODE_SUCCESS, [`setSelectedIndex`, this.cliIndex]);
        }
        return {
            machine,
            context: this.context,
        };
    }
    registerOptions(machine, node) {
        registerDynamic(machine, node, [`isOption`, `--`], node, `inhibateOptions`);
        registerDynamic(machine, node, [`isBatchOption`, this.allOptionNames], node, `pushBatch`);
        registerDynamic(machine, node, [`isBoundOption`, this.allOptionNames, this.options], node, `pushBound`);
        registerDynamic(machine, node, [`isUnsupportedOption`, this.allOptionNames], constants.NODE_ERRORED, [`setError`, `Unsupported option name`]);
        registerDynamic(machine, node, [`isInvalidOption`], constants.NODE_ERRORED, [`setError`, `Invalid option name`]);
        for (const option of this.options) {
            const longestName = option.names.reduce((longestName, name) => {
                return name.length > longestName.length ? name : longestName;
            }, ``);
            if (option.arity === 0) {
                for (const name of option.names) {
                    registerDynamic(machine, node, [`isOption`, name, option.hidden || name !== longestName], node, `pushTrue`);
                    if (name.startsWith(`--`) && !name.startsWith(`--no-`)) {
                        registerDynamic(machine, node, [`isNegatedOption`, name], node, [`pushFalse`, name]);
                    }
                }
            }
            else {
                // We inject a new node at the end of the state machine
                let lastNode = injectNode(machine, makeNode());
                // We register transitions from the starting node to this new node
                for (const name of option.names)
                    registerDynamic(machine, node, [`isOption`, name, option.hidden || name !== longestName], lastNode, `pushUndefined`);
                // For each argument, we inject a new node at the end and we
                // register a transition from the current node to this new node
                for (let t = 0; t < option.arity; ++t) {
                    const nextNode = injectNode(machine, makeNode());
                    // We can provide better errors when another option or END_OF_INPUT is encountered
                    registerStatic(machine, lastNode, constants.END_OF_INPUT, constants.NODE_ERRORED, `setOptionArityError`);
                    registerDynamic(machine, lastNode, `isOptionLike`, constants.NODE_ERRORED, `setOptionArityError`);
                    // If the option has a single argument, no need to store it in an array
                    const action = option.arity === 1
                        ? `setStringValue`
                        : `pushStringValue`;
                    registerDynamic(machine, lastNode, `isNotOptionLike`, nextNode, action);
                    lastNode = nextNode;
                }
                // In the end, we register a shortcut from
                // the last node back to the starting node
                registerShortcut(machine, lastNode, node);
            }
        }
    }
}
class CliBuilder {
    constructor({ binaryName = `...` } = {}) {
        this.builders = [];
        this.opts = { binaryName };
    }
    static build(cbs, opts = {}) {
        return new CliBuilder(opts).commands(cbs).compile();
    }
    getBuilderByIndex(n) {
        if (!(n >= 0 && n < this.builders.length))
            throw new Error(`Assertion failed: Out-of-bound command index (${n})`);
        return this.builders[n];
    }
    commands(cbs) {
        for (const cb of cbs)
            cb(this.command());
        return this;
    }
    command() {
        const builder = new CommandBuilder(this.builders.length, this.opts);
        this.builders.push(builder);
        return builder;
    }
    compile() {
        const machines = [];
        const contexts = [];
        for (const builder of this.builders) {
            const { machine, context } = builder.compile();
            machines.push(machine);
            contexts.push(context);
        }
        const machine = makeAnyOfMachine(machines);
        simplifyMachine(machine);
        return {
            machine,
            contexts,
            process: (input) => {
                return runMachine(machine, input);
            },
            suggest: (input, partial) => {
                return suggestMachine(machine, input, partial);
            },
        };
    }
}

exports.CliBuilder = CliBuilder;
exports.CommandBuilder = CommandBuilder;
exports.NoLimits = NoLimits;
exports.aggregateHelpStates = aggregateHelpStates;
exports.cloneNode = cloneNode;
exports.cloneTransition = cloneTransition;
exports.debug = debug;
exports.debugMachine = debugMachine;
exports.execute = execute;
exports.injectNode = injectNode;
exports.isTerminalNode = isTerminalNode;
exports.makeAnyOfMachine = makeAnyOfMachine;
exports.makeNode = makeNode;
exports.makeStateMachine = makeStateMachine;
exports.reducers = reducers;
exports.registerDynamic = registerDynamic;
exports.registerShortcut = registerShortcut;
exports.registerStatic = registerStatic;
exports.runMachineInternal = runMachineInternal;
exports.selectBestState = selectBestState;
exports.simplifyMachine = simplifyMachine;
exports.suggest = suggest;
exports.tests = tests;
exports.trimSmallerBranches = trimSmallerBranches;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/errors.js":
/*!***************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/errors.js ***!
  \***************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

var constants = __webpack_require__(/*! ./constants.js */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/constants.js");

/**
 * A generic usage error with the name `UsageError`.
 *
 * It should be used over `Error` only when it's the user's fault.
 */
class UsageError extends Error {
    constructor(message) {
        super(message);
        this.clipanion = { type: `usage` };
        this.name = `UsageError`;
    }
}
class UnknownSyntaxError extends Error {
    constructor(input, candidates) {
        super();
        this.input = input;
        this.candidates = candidates;
        this.clipanion = { type: `none` };
        this.name = `UnknownSyntaxError`;
        if (this.candidates.length === 0) {
            this.message = `Command not found, but we're not sure what's the alternative.`;
        }
        else if (this.candidates.every(candidate => candidate.reason !== null && candidate.reason === candidates[0].reason)) {
            const [{ reason }] = this.candidates;
            this.message = `${reason}\n\n${this.candidates.map(({ usage }) => `$ ${usage}`).join(`\n`)}`;
        }
        else if (this.candidates.length === 1) {
            const [{ usage }] = this.candidates;
            this.message = `Command not found; did you mean:\n\n$ ${usage}\n${whileRunning(input)}`;
        }
        else {
            this.message = `Command not found; did you mean one of:\n\n${this.candidates.map(({ usage }, index) => {
                return `${`${index}.`.padStart(4)} ${usage}`;
            }).join(`\n`)}\n\n${whileRunning(input)}`;
        }
    }
}
class AmbiguousSyntaxError extends Error {
    constructor(input, usages) {
        super();
        this.input = input;
        this.usages = usages;
        this.clipanion = { type: `none` };
        this.name = `AmbiguousSyntaxError`;
        this.message = `Cannot find which to pick amongst the following alternatives:\n\n${this.usages.map((usage, index) => {
            return `${`${index}.`.padStart(4)} ${usage}`;
        }).join(`\n`)}\n\n${whileRunning(input)}`;
    }
}
const whileRunning = (input) => `While running ${input.filter(token => {
    return token !== constants.END_OF_INPUT;
}).map(token => {
    const json = JSON.stringify(token);
    if (token.match(/\s/) || token.length === 0 || json !== `"${token}"`) {
        return json;
    }
    else {
        return token;
    }
}).join(` `)}`;

exports.AmbiguousSyntaxError = AmbiguousSyntaxError;
exports.UnknownSyntaxError = UnknownSyntaxError;
exports.UsageError = UsageError;


/***/ }),

/***/ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/format.js":
/*!***************************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/format.js ***!
  \***************************************************************************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

const MAX_LINE_LENGTH = 80;
const richLine = Array(MAX_LINE_LENGTH).fill(`━`);
for (let t = 0; t <= 24; ++t)
    richLine[richLine.length - t] = `\x1b[38;5;${232 + t}m━`;
const richFormat = {
    header: str => `\x1b[1m━━━ ${str}${str.length < MAX_LINE_LENGTH - 5 ? ` ${richLine.slice(str.length + 5).join(``)}` : `:`}\x1b[0m`,
    bold: str => `\x1b[1m${str}\x1b[22m`,
    error: str => `\x1b[31m\x1b[1m${str}\x1b[22m\x1b[39m`,
    code: str => `\x1b[36m${str}\x1b[39m`,
};
const textFormat = {
    header: str => str,
    bold: str => str,
    error: str => str,
    code: str => str,
};
function dedent(text) {
    const lines = text.split(`\n`);
    const nonEmptyLines = lines.filter(line => line.match(/\S/));
    const indent = nonEmptyLines.length > 0 ? nonEmptyLines.reduce((minLength, line) => Math.min(minLength, line.length - line.trimStart().length), Number.MAX_VALUE) : 0;
    return lines
        .map(line => line.slice(indent).trimRight())
        .join(`\n`);
}
function formatMarkdownish(text, { format, paragraphs }) {
    // Enforce \n as newline character
    text = text.replace(/\r\n?/g, `\n`);
    // Remove the indentation, since it got messed up with the JS indentation
    text = dedent(text);
    // Remove surrounding newlines, since they got added for JS formatting
    text = text.replace(/^\n+|\n+$/g, ``);
    // List items always end with at least two newlines (in order to not be collapsed)
    text = text.replace(/^(\s*)-([^\n]*?)\n+/gm, `$1-$2\n\n`);
    // Single newlines are removed; larger than that are collapsed into one
    text = text.replace(/\n(\n)?\n*/g, `$1`);
    if (paragraphs) {
        text = text.split(/\n/).map(paragraph => {
            // Does the paragraph starts with a list?
            const bulletMatch = paragraph.match(/^\s*[*-][\t ]+(.*)/);
            if (!bulletMatch)
                // No, cut the paragraphs into segments of 80 characters
                return paragraph.match(/(.{1,80})(?: |$)/g).join(`\n`);
            const indent = paragraph.length - paragraph.trimStart().length;
            // Yes, cut the paragraphs into segments of (78 - indent) characters (to account for the prefix)
            return bulletMatch[1].match(new RegExp(`(.{1,${78 - indent}})(?: |$)`, `g`)).map((line, index) => {
                return ` `.repeat(indent) + (index === 0 ? `- ` : `  `) + line;
            }).join(`\n`);
        }).join(`\n\n`);
    }
    // Highlight the code segments
    text = text.replace(/(`+)((?:.|[\n])*?)\1/g, ($0, $1, $2) => {
        return format.code($1 + $2 + $1);
    });
    // Highlight the code segments
    text = text.replace(/(\*\*)((?:.|[\n])*?)\1/g, ($0, $1, $2) => {
        return format.bold($1 + $2 + $1);
    });
    return text ? `${text}\n` : ``;
}

exports.formatMarkdownish = formatMarkdownish;
exports.richFormat = richFormat;
exports.textFormat = textFormat;


/***/ }),

/***/ "./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/browser.js":
/*!****************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/browser.js ***!
  \****************************************************************************************************************************************/
/***/ ((module, exports, __webpack_require__) => {

/* eslint-env browser */

/**
 * This is the web browser implementation of `debug()`.
 */

exports.log = log;
exports.formatArgs = formatArgs;
exports.save = save;
exports.load = load;
exports.useColors = useColors;
exports.storage = localstorage();

/**
 * Colors.
 */

exports.colors = [
	'#0000CC',
	'#0000FF',
	'#0033CC',
	'#0033FF',
	'#0066CC',
	'#0066FF',
	'#0099CC',
	'#0099FF',
	'#00CC00',
	'#00CC33',
	'#00CC66',
	'#00CC99',
	'#00CCCC',
	'#00CCFF',
	'#3300CC',
	'#3300FF',
	'#3333CC',
	'#3333FF',
	'#3366CC',
	'#3366FF',
	'#3399CC',
	'#3399FF',
	'#33CC00',
	'#33CC33',
	'#33CC66',
	'#33CC99',
	'#33CCCC',
	'#33CCFF',
	'#6600CC',
	'#6600FF',
	'#6633CC',
	'#6633FF',
	'#66CC00',
	'#66CC33',
	'#9900CC',
	'#9900FF',
	'#9933CC',
	'#9933FF',
	'#99CC00',
	'#99CC33',
	'#CC0000',
	'#CC0033',
	'#CC0066',
	'#CC0099',
	'#CC00CC',
	'#CC00FF',
	'#CC3300',
	'#CC3333',
	'#CC3366',
	'#CC3399',
	'#CC33CC',
	'#CC33FF',
	'#CC6600',
	'#CC6633',
	'#CC9900',
	'#CC9933',
	'#CCCC00',
	'#CCCC33',
	'#FF0000',
	'#FF0033',
	'#FF0066',
	'#FF0099',
	'#FF00CC',
	'#FF00FF',
	'#FF3300',
	'#FF3333',
	'#FF3366',
	'#FF3399',
	'#FF33CC',
	'#FF33FF',
	'#FF6600',
	'#FF6633',
	'#FF9900',
	'#FF9933',
	'#FFCC00',
	'#FFCC33'
];

/**
 * Currently only WebKit-based Web Inspectors, Firefox >= v31,
 * and the Firebug extension (any Firefox version) are known
 * to support "%c" CSS customizations.
 *
 * TODO: add a `localStorage` variable to explicitly enable/disable colors
 */

// eslint-disable-next-line complexity
function useColors() {
	// NB: In an Electron preload script, document will be defined but not fully
	// initialized. Since we know we're in Chrome, we'll just detect this case
	// explicitly
	if (typeof window !== 'undefined' && window.process && (window.process.type === 'renderer' || window.process.__nwjs)) {
		return true;
	}

	// Internet Explorer and Edge do not support colors.
	if (typeof navigator !== 'undefined' && navigator.userAgent && navigator.userAgent.toLowerCase().match(/(edge|trident)\/(\d+)/)) {
		return false;
	}

	// Is webkit? http://stackoverflow.com/a/16459606/376773
	// document is undefined in react-native: https://github.com/facebook/react-native/pull/1632
	return (typeof document !== 'undefined' && document.documentElement && document.documentElement.style && document.documentElement.style.WebkitAppearance) ||
		// Is firebug? http://stackoverflow.com/a/398120/376773
		(typeof window !== 'undefined' && window.console && (window.console.firebug || (window.console.exception && window.console.table))) ||
		// Is firefox >= v31?
		// https://developer.mozilla.org/en-US/docs/Tools/Web_Console#Styling_messages
		(typeof navigator !== 'undefined' && navigator.userAgent && navigator.userAgent.toLowerCase().match(/firefox\/(\d+)/) && parseInt(RegExp.$1, 10) >= 31) ||
		// Double check webkit in userAgent just in case we are in a worker
		(typeof navigator !== 'undefined' && navigator.userAgent && navigator.userAgent.toLowerCase().match(/applewebkit\/(\d+)/));
}

/**
 * Colorize log arguments if enabled.
 *
 * @api public
 */

function formatArgs(args) {
	args[0] = (this.useColors ? '%c' : '') +
		this.namespace +
		(this.useColors ? ' %c' : ' ') +
		args[0] +
		(this.useColors ? '%c ' : ' ') +
		'+' + module.exports.humanize(this.diff);

	if (!this.useColors) {
		return;
	}

	const c = 'color: ' + this.color;
	args.splice(1, 0, c, 'color: inherit');

	// The final "%c" is somewhat tricky, because there could be other
	// arguments passed either before or after the %c, so we need to
	// figure out the correct index to insert the CSS into
	let index = 0;
	let lastC = 0;
	args[0].replace(/%[a-zA-Z%]/g, match => {
		if (match === '%%') {
			return;
		}
		index++;
		if (match === '%c') {
			// We only are interested in the *last* %c
			// (the user may have provided their own)
			lastC = index;
		}
	});

	args.splice(lastC, 0, c);
}

/**
 * Invokes `console.log()` when available.
 * No-op when `console.log` is not a "function".
 *
 * @api public
 */
function log(...args) {
	// This hackery is required for IE8/9, where
	// the `console.log` function doesn't have 'apply'
	return typeof console === 'object' &&
		console.log &&
		console.log(...args);
}

/**
 * Save `namespaces`.
 *
 * @param {String} namespaces
 * @api private
 */
function save(namespaces) {
	try {
		if (namespaces) {
			exports.storage.setItem('debug', namespaces);
		} else {
			exports.storage.removeItem('debug');
		}
	} catch (error) {
		// Swallow
		// XXX (@Qix-) should we be logging these?
	}
}

/**
 * Load `namespaces`.
 *
 * @return {String} returns the previously persisted debug modes
 * @api private
 */
function load() {
	let r;
	try {
		r = exports.storage.getItem('debug');
	} catch (error) {
		// Swallow
		// XXX (@Qix-) should we be logging these?
	}

	// If debug isn't set in LS, and we're in Electron, try to load $DEBUG
	if (!r && typeof process !== 'undefined' && 'env' in process) {
		r = process.env.DEBUG;
	}

	return r;
}

/**
 * Localstorage attempts to return the localstorage.
 *
 * This is necessary because safari throws
 * when a user disables cookies/localstorage
 * and you attempt to access it.
 *
 * @return {LocalStorage}
 * @api private
 */

function localstorage() {
	try {
		// TVMLKit (Apple TV JS Runtime) does not have a window object, just localStorage in the global context
		// The Browser also has localStorage in the global context.
		return localStorage;
	} catch (error) {
		// Swallow
		// XXX (@Qix-) should we be logging these?
	}
}

module.exports = __webpack_require__(/*! ./common */ "./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/common.js")(exports);

const {formatters} = module.exports;

/**
 * Map %j to `JSON.stringify()`, since no Web Inspectors do that by default.
 */

formatters.j = function (v) {
	try {
		return JSON.stringify(v);
	} catch (error) {
		return '[UnexpectedJSONParseError]: ' + error.message;
	}
};


/***/ }),

/***/ "./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/common.js":
/*!***************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/common.js ***!
  \***************************************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {


/**
 * This is the common logic for both the Node.js and web browser
 * implementations of `debug()`.
 */

function setup(env) {
	createDebug.debug = createDebug;
	createDebug.default = createDebug;
	createDebug.coerce = coerce;
	createDebug.disable = disable;
	createDebug.enable = enable;
	createDebug.enabled = enabled;
	createDebug.humanize = __webpack_require__(/*! ms */ "./.yarn/cache/ms-npm-2.1.2-ec0c1512ff-673cdb2c31.zip/node_modules/ms/index.js");

	Object.keys(env).forEach(key => {
		createDebug[key] = env[key];
	});

	/**
	* Active `debug` instances.
	*/
	createDebug.instances = [];

	/**
	* The currently active debug mode names, and names to skip.
	*/

	createDebug.names = [];
	createDebug.skips = [];

	/**
	* Map of special "%n" handling functions, for the debug "format" argument.
	*
	* Valid key names are a single, lower or upper-case letter, i.e. "n" and "N".
	*/
	createDebug.formatters = {};

	/**
	* Selects a color for a debug namespace
	* @param {String} namespace The namespace string for the for the debug instance to be colored
	* @return {Number|String} An ANSI color code for the given namespace
	* @api private
	*/
	function selectColor(namespace) {
		let hash = 0;

		for (let i = 0; i < namespace.length; i++) {
			hash = ((hash << 5) - hash) + namespace.charCodeAt(i);
			hash |= 0; // Convert to 32bit integer
		}

		return createDebug.colors[Math.abs(hash) % createDebug.colors.length];
	}
	createDebug.selectColor = selectColor;

	/**
	* Create a debugger with the given `namespace`.
	*
	* @param {String} namespace
	* @return {Function}
	* @api public
	*/
	function createDebug(namespace) {
		let prevTime;

		function debug(...args) {
			// Disabled?
			if (!debug.enabled) {
				return;
			}

			const self = debug;

			// Set `diff` timestamp
			const curr = Number(new Date());
			const ms = curr - (prevTime || curr);
			self.diff = ms;
			self.prev = prevTime;
			self.curr = curr;
			prevTime = curr;

			args[0] = createDebug.coerce(args[0]);

			if (typeof args[0] !== 'string') {
				// Anything else let's inspect with %O
				args.unshift('%O');
			}

			// Apply any `formatters` transformations
			let index = 0;
			args[0] = args[0].replace(/%([a-zA-Z%])/g, (match, format) => {
				// If we encounter an escaped % then don't increase the array index
				if (match === '%%') {
					return match;
				}
				index++;
				const formatter = createDebug.formatters[format];
				if (typeof formatter === 'function') {
					const val = args[index];
					match = formatter.call(self, val);

					// Now we need to remove `args[index]` since it's inlined in the `format`
					args.splice(index, 1);
					index--;
				}
				return match;
			});

			// Apply env-specific formatting (colors, etc.)
			createDebug.formatArgs.call(self, args);

			const logFn = self.log || createDebug.log;
			logFn.apply(self, args);
		}

		debug.namespace = namespace;
		debug.enabled = createDebug.enabled(namespace);
		debug.useColors = createDebug.useColors();
		debug.color = selectColor(namespace);
		debug.destroy = destroy;
		debug.extend = extend;
		// Debug.formatArgs = formatArgs;
		// debug.rawLog = rawLog;

		// env-specific initialization logic for debug instances
		if (typeof createDebug.init === 'function') {
			createDebug.init(debug);
		}

		createDebug.instances.push(debug);

		return debug;
	}

	function destroy() {
		const index = createDebug.instances.indexOf(this);
		if (index !== -1) {
			createDebug.instances.splice(index, 1);
			return true;
		}
		return false;
	}

	function extend(namespace, delimiter) {
		const newDebug = createDebug(this.namespace + (typeof delimiter === 'undefined' ? ':' : delimiter) + namespace);
		newDebug.log = this.log;
		return newDebug;
	}

	/**
	* Enables a debug mode by namespaces. This can include modes
	* separated by a colon and wildcards.
	*
	* @param {String} namespaces
	* @api public
	*/
	function enable(namespaces) {
		createDebug.save(namespaces);

		createDebug.names = [];
		createDebug.skips = [];

		let i;
		const split = (typeof namespaces === 'string' ? namespaces : '').split(/[\s,]+/);
		const len = split.length;

		for (i = 0; i < len; i++) {
			if (!split[i]) {
				// ignore empty strings
				continue;
			}

			namespaces = split[i].replace(/\*/g, '.*?');

			if (namespaces[0] === '-') {
				createDebug.skips.push(new RegExp('^' + namespaces.substr(1) + '$'));
			} else {
				createDebug.names.push(new RegExp('^' + namespaces + '$'));
			}
		}

		for (i = 0; i < createDebug.instances.length; i++) {
			const instance = createDebug.instances[i];
			instance.enabled = createDebug.enabled(instance.namespace);
		}
	}

	/**
	* Disable debug output.
	*
	* @return {String} namespaces
	* @api public
	*/
	function disable() {
		const namespaces = [
			...createDebug.names.map(toNamespace),
			...createDebug.skips.map(toNamespace).map(namespace => '-' + namespace)
		].join(',');
		createDebug.enable('');
		return namespaces;
	}

	/**
	* Returns true if the given mode name is enabled, false otherwise.
	*
	* @param {String} name
	* @return {Boolean}
	* @api public
	*/
	function enabled(name) {
		if (name[name.length - 1] === '*') {
			return true;
		}

		let i;
		let len;

		for (i = 0, len = createDebug.skips.length; i < len; i++) {
			if (createDebug.skips[i].test(name)) {
				return false;
			}
		}

		for (i = 0, len = createDebug.names.length; i < len; i++) {
			if (createDebug.names[i].test(name)) {
				return true;
			}
		}

		return false;
	}

	/**
	* Convert regexp to namespace
	*
	* @param {RegExp} regxep
	* @return {String} namespace
	* @api private
	*/
	function toNamespace(regexp) {
		return regexp.toString()
			.substring(2, regexp.toString().length - 2)
			.replace(/\.\*\?$/, '*');
	}

	/**
	* Coerce `val`.
	*
	* @param {Mixed} val
	* @return {Mixed}
	* @api private
	*/
	function coerce(val) {
		if (val instanceof Error) {
			return val.stack || val.message;
		}
		return val;
	}

	createDebug.enable(createDebug.load());

	return createDebug;
}

module.exports = setup;


/***/ }),

/***/ "./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/index.js":
/*!**************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/index.js ***!
  \**************************************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

/**
 * Detect Electron renderer / nwjs process, which is node, but we should
 * treat as a browser.
 */

if (typeof process === 'undefined' || process.type === 'renderer' || process.browser === true || process.__nwjs) {
	module.exports = __webpack_require__(/*! ./browser.js */ "./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/browser.js");
} else {
	module.exports = __webpack_require__(/*! ./node.js */ "./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/node.js");
}


/***/ }),

/***/ "./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/node.js":
/*!*************************************************************************************************************************************!*\
  !*** ./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/node.js ***!
  \*************************************************************************************************************************************/
/***/ ((module, exports, __webpack_require__) => {

/**
 * Module dependencies.
 */

const tty = __webpack_require__(/*! tty */ "tty");
const util = __webpack_require__(/*! util */ "util");

/**
 * This is the Node.js implementation of `debug()`.
 */

exports.init = init;
exports.log = log;
exports.formatArgs = formatArgs;
exports.save = save;
exports.load = load;
exports.useColors = useColors;

/**
 * Colors.
 */

exports.colors = [6, 2, 3, 4, 5, 1];

try {
	// Optional dependency (as in, doesn't need to be installed, NOT like optionalDependencies in package.json)
	// eslint-disable-next-line import/no-extraneous-dependencies
	const supportsColor = __webpack_require__(/*! supports-color */ "./.yarn/cache/supports-color-npm-7.1.0-df2ba1e338-899480ac85.zip/node_modules/supports-color/index.js");

	if (supportsColor && (supportsColor.stderr || supportsColor).level >= 2) {
		exports.colors = [
			20,
			21,
			26,
			27,
			32,
			33,
			38,
			39,
			40,
			41,
			42,
			43,
			44,
			45,
			56,
			57,
			62,
			63,
			68,
			69,
			74,
			75,
			76,
			77,
			78,
			79,
			80,
			81,
			92,
			93,
			98,
			99,
			112,
			113,
			128,
			129,
			134,
			135,
			148,
			149,
			160,
			161,
			162,
			163,
			164,
			165,
			166,
			167,
			168,
			169,
			170,
			171,
			172,
			173,
			178,
			179,
			184,
			185,
			196,
			197,
			198,
			199,
			200,
			201,
			202,
			203,
			204,
			205,
			206,
			207,
			208,
			209,
			214,
			215,
			220,
			221
		];
	}
} catch (error) {
	// Swallow - we only care if `supports-color` is available; it doesn't have to be.
}

/**
 * Build up the default `inspectOpts` object from the environment variables.
 *
 *   $ DEBUG_COLORS=no DEBUG_DEPTH=10 DEBUG_SHOW_HIDDEN=enabled node script.js
 */

exports.inspectOpts = Object.keys(process.env).filter(key => {
	return /^debug_/i.test(key);
}).reduce((obj, key) => {
	// Camel-case
	const prop = key
		.substring(6)
		.toLowerCase()
		.replace(/_([a-z])/g, (_, k) => {
			return k.toUpperCase();
		});

	// Coerce string value into JS value
	let val = process.env[key];
	if (/^(yes|on|true|enabled)$/i.test(val)) {
		val = true;
	} else if (/^(no|off|false|disabled)$/i.test(val)) {
		val = false;
	} else if (val === 'null') {
		val = null;
	} else {
		val = Number(val);
	}

	obj[prop] = val;
	return obj;
}, {});

/**
 * Is stdout a TTY? Colored output is enabled when `true`.
 */

function useColors() {
	return 'colors' in exports.inspectOpts ?
		Boolean(exports.inspectOpts.colors) :
		tty.isatty(process.stderr.fd);
}

/**
 * Adds ANSI color escape codes if enabled.
 *
 * @api public
 */

function formatArgs(args) {
	const {namespace: name, useColors} = this;

	if (useColors) {
		const c = this.color;
		const colorCode = '\u001B[3' + (c < 8 ? c : '8;5;' + c);
		const prefix = `  ${colorCode};1m${name} \u001B[0m`;

		args[0] = prefix + args[0].split('\n').join('\n' + prefix);
		args.push(colorCode + 'm+' + module.exports.humanize(this.diff) + '\u001B[0m');
	} else {
		args[0] = getDate() + name + ' ' + args[0];
	}
}

function getDate() {
	if (exports.inspectOpts.hideDate) {
		return '';
	}
	return new Date().toISOString() + ' ';
}

/**
 * Invokes `util.format()` with the specified arguments and writes to stderr.
 */

function log(...args) {
	return process.stderr.write(util.format(...args) + '\n');
}

/**
 * Save `namespaces`.
 *
 * @param {String} namespaces
 * @api private
 */
function save(namespaces) {
	if (namespaces) {
		process.env.DEBUG = namespaces;
	} else {
		// If you set a process.env field to null or undefined, it gets cast to the
		// string 'null' or 'undefined'. Just delete instead.
		delete process.env.DEBUG;
	}
}

/**
 * Load `namespaces`.
 *
 * @return {String} returns the previously persisted debug modes
 * @api private
 */

function load() {
	return process.env.DEBUG;
}

/**
 * Init logic for `debug` instances.
 *
 * Create a new `inspectOpts` object in case `useColors` is set
 * differently for a particular `debug` instance.
 */

function init(debug) {
	debug.inspectOpts = {};

	const keys = Object.keys(exports.inspectOpts);
	for (let i = 0; i < keys.length; i++) {
		debug.inspectOpts[keys[i]] = exports.inspectOpts[keys[i]];
	}
}

module.exports = __webpack_require__(/*! ./common */ "./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/common.js")(exports);

const {formatters} = module.exports;

/**
 * Map %o to `util.inspect()`, all on a single line.
 */

formatters.o = function (v) {
	this.inspectOpts.colors = this.useColors;
	return util.inspect(v, this.inspectOpts)
		.replace(/\s*\n\s*/g, ' ');
};

/**
 * Map %O to `util.inspect()`, allowing multiple lines if needed.
 */

formatters.O = function (v) {
	this.inspectOpts.colors = this.useColors;
	return util.inspect(v, this.inspectOpts);
};


/***/ }),

/***/ "./sources/Engine.ts":
/*!***************************!*\
  !*** ./sources/Engine.ts ***!
  \***************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "Engine": () => (/* binding */ Engine)
/* harmony export */ });
/* harmony import */ var clipanion__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(/*! clipanion */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! fs */ "fs");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(fs__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! path */ "path");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(path__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var semver__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/index.js");
/* harmony import */ var semver__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(semver__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var _config_json__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../config.json */ "./config.json");
/* harmony import */ var _folderUtils__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ./folderUtils */ "./sources/folderUtils.ts");
/* harmony import */ var _corepackUtils__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ./corepackUtils */ "./sources/corepackUtils.ts");
/* harmony import */ var _semverUtils__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(/*! ./semverUtils */ "./sources/semverUtils.ts");
/* harmony import */ var _types__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(/*! ./types */ "./sources/types.ts");









class Engine {
    constructor(config = _config_json__WEBPACK_IMPORTED_MODULE_3__) {
        this.config = config;
    }
    getPackageManagerFor(binaryName) {
        for (const packageManager of _types__WEBPACK_IMPORTED_MODULE_7__.SupportedPackageManagerSet) {
            for (const rangeDefinition of Object.values(this.config.definitions[packageManager].ranges)) {
                const bins = Array.isArray(rangeDefinition.bin)
                    ? rangeDefinition.bin
                    : Object.keys(rangeDefinition.bin);
                if (bins.includes(binaryName)) {
                    return packageManager;
                }
            }
        }
        return null;
    }
    getBinariesFor(name) {
        const binNames = new Set();
        for (const rangeDefinition of Object.values(this.config.definitions[name].ranges)) {
            const bins = Array.isArray(rangeDefinition.bin)
                ? rangeDefinition.bin
                : Object.keys(rangeDefinition.bin);
            for (const name of bins) {
                binNames.add(name);
            }
        }
        return binNames;
    }
    async getDefaultDescriptors() {
        const locators = [];
        for (const name of _types__WEBPACK_IMPORTED_MODULE_7__.SupportedPackageManagerSet)
            locators.push({ name, range: await this.getDefaultVersion(name) });
        return locators;
    }
    async getDefaultVersion(packageManager) {
        const definition = this.config.definitions[packageManager];
        if (typeof definition === `undefined`)
            throw new clipanion__WEBPACK_IMPORTED_MODULE_8__.UsageError(`This package manager (${packageManager}) isn't supported by this corepack build`);
        let lastKnownGood;
        try {
            lastKnownGood = JSON.parse(await fs__WEBPACK_IMPORTED_MODULE_0___default().promises.readFile(this.getLastKnownGoodFile(), `utf8`));
        }
        catch (_a) {
            // Ignore errors; too bad
        }
        if (typeof lastKnownGood !== `object` || lastKnownGood === null)
            return definition.default;
        if (!Object.prototype.hasOwnProperty.call(lastKnownGood, packageManager))
            return definition.default;
        const override = lastKnownGood[packageManager];
        if (typeof override !== `string`)
            return definition.default;
        return override;
    }
    async activatePackageManager(locator) {
        const lastKnownGoodFile = this.getLastKnownGoodFile();
        let lastKnownGood;
        try {
            lastKnownGood = JSON.parse(await fs__WEBPACK_IMPORTED_MODULE_0___default().promises.readFile(lastKnownGoodFile, `utf8`));
        }
        catch (_a) {
            // Ignore errors; too bad
        }
        if (typeof lastKnownGood !== `object` || lastKnownGood === null)
            lastKnownGood = {};
        lastKnownGood[locator.name] = locator.reference;
        await fs__WEBPACK_IMPORTED_MODULE_0___default().promises.mkdir(path__WEBPACK_IMPORTED_MODULE_1___default().dirname(lastKnownGoodFile), { recursive: true });
        await fs__WEBPACK_IMPORTED_MODULE_0___default().promises.writeFile(lastKnownGoodFile, `${JSON.stringify(lastKnownGood, null, 2)}\n`);
    }
    async ensurePackageManager(locator) {
        const definition = this.config.definitions[locator.name];
        if (typeof definition === `undefined`)
            throw new clipanion__WEBPACK_IMPORTED_MODULE_8__.UsageError(`This package manager (${locator.name}) isn't supported by this corepack build`);
        const ranges = Object.keys(definition.ranges).reverse();
        const range = ranges.find(range => _semverUtils__WEBPACK_IMPORTED_MODULE_6__.satisfiesWithPrereleases(locator.reference, range));
        if (typeof range === `undefined`)
            throw new Error(`Assertion failed: Specified resolution (${locator.reference}) isn't supported by any of ${ranges.join(`, `)}`);
        const installedLocation = await _corepackUtils__WEBPACK_IMPORTED_MODULE_5__.installVersion(_folderUtils__WEBPACK_IMPORTED_MODULE_4__.getInstallFolder(), locator, {
            spec: definition.ranges[range],
        });
        return {
            location: installedLocation,
            spec: definition.ranges[range],
        };
    }
    async resolveDescriptor(descriptor, { allowTags = false, useCache = true } = {}) {
        const definition = this.config.definitions[descriptor.name];
        if (typeof definition === `undefined`)
            throw new clipanion__WEBPACK_IMPORTED_MODULE_8__.UsageError(`This package manager (${descriptor.name}) isn't supported by this corepack build`);
        let finalDescriptor = descriptor;
        if (descriptor.range.match(/^[a-z-]+$/)) {
            if (!allowTags)
                throw new clipanion__WEBPACK_IMPORTED_MODULE_8__.UsageError(`Packages managers can't be referended via tags in this context`);
            // We only resolve tags from the latest registry entry
            const ranges = Object.keys(definition.ranges);
            const tagRange = ranges[ranges.length - 1];
            const tags = await _corepackUtils__WEBPACK_IMPORTED_MODULE_5__.fetchAvailableTags(definition.ranges[tagRange].registry);
            if (!Object.prototype.hasOwnProperty.call(tags, descriptor.range))
                throw new clipanion__WEBPACK_IMPORTED_MODULE_8__.UsageError(`Tag not found (${descriptor.range})`);
            finalDescriptor = {
                name: descriptor.name,
                range: tags[descriptor.range],
            };
        }
        // If a compatible version is already installed, no need to query one
        // from the remote listings
        const cachedVersion = await _corepackUtils__WEBPACK_IMPORTED_MODULE_5__.findInstalledVersion(_folderUtils__WEBPACK_IMPORTED_MODULE_4__.getInstallFolder(), finalDescriptor);
        if (cachedVersion !== null && useCache)
            return { name: finalDescriptor.name, reference: cachedVersion };
        const candidateRangeDefinitions = Object.keys(definition.ranges).filter(range => {
            return _semverUtils__WEBPACK_IMPORTED_MODULE_6__.satisfiesWithPrereleases(finalDescriptor.range, range);
        });
        const tagResolutions = await Promise.all(candidateRangeDefinitions.map(async (range) => {
            return [range, await _corepackUtils__WEBPACK_IMPORTED_MODULE_5__.fetchAvailableVersions(definition.ranges[range].registry)];
        }));
        // If a version is available under multiple strategies (for example if
        // Yarn is published to both the v1 package and git), we only care
        // about the latest one
        const resolutionMap = new Map();
        for (const [range, resolutions] of tagResolutions)
            for (const entry of resolutions)
                resolutionMap.set(entry, range);
        const candidates = [...resolutionMap.keys()];
        const maxSatisfying = semver__WEBPACK_IMPORTED_MODULE_2___default().maxSatisfying(candidates, finalDescriptor.range);
        if (maxSatisfying === null)
            return null;
        return { name: finalDescriptor.name, reference: maxSatisfying };
    }
    getLastKnownGoodFile() {
        return path__WEBPACK_IMPORTED_MODULE_1___default().join(_folderUtils__WEBPACK_IMPORTED_MODULE_4__.getInstallFolder(), `lastKnownGood.json`);
    }
}


/***/ }),

/***/ "./sources/commands/Disable.ts":
/*!*************************************!*\
  !*** ./sources/commands/Disable.ts ***!
  \*************************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "DisableCommand": () => (/* binding */ DisableCommand)
/* harmony export */ });
/* harmony import */ var clipanion__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! clipanion */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! fs */ "fs");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(fs__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! path */ "path");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(path__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var which__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! which */ "./.yarn/cache/which-npm-2.0.2-320ddf72f7-1a5c563d3c.zip/node_modules/which/which.js");
/* harmony import */ var which__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(which__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var _types__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../types */ "./sources/types.ts");





class DisableCommand extends clipanion__WEBPACK_IMPORTED_MODULE_4__.Command {
    constructor() {
        super(...arguments);
        this.installDirectory = clipanion__WEBPACK_IMPORTED_MODULE_4__.Option.String(`--install-directory`, {
            description: `Where the shims are located`,
        });
        this.names = clipanion__WEBPACK_IMPORTED_MODULE_4__.Option.Rest();
    }
    async execute() {
        let installDirectory = this.installDirectory;
        // Node always call realpath on the module it executes, so we already
        // lost track of how the binary got called. To find it back, we need to
        // iterate over the PATH variable.
        if (typeof installDirectory === `undefined`)
            installDirectory = path__WEBPACK_IMPORTED_MODULE_1___default().dirname(await which__WEBPACK_IMPORTED_MODULE_2___default()(`corepack`));
        const names = this.names.length === 0
            ? _types__WEBPACK_IMPORTED_MODULE_3__.SupportedPackageManagerSetWithoutNpm
            : this.names;
        for (const name of new Set(names)) {
            if (!(0,_types__WEBPACK_IMPORTED_MODULE_3__.isSupportedPackageManager)(name))
                throw new clipanion__WEBPACK_IMPORTED_MODULE_4__.UsageError(`Invalid package manager name '${name}'`);
            for (const binName of this.context.engine.getBinariesFor(name)) {
                if (process.platform === `win32`) {
                    await this.removeWin32Link(installDirectory, binName);
                }
                else {
                    await this.removePosixLink(installDirectory, binName);
                }
            }
        }
    }
    async removePosixLink(installDirectory, binName) {
        const file = path__WEBPACK_IMPORTED_MODULE_1___default().join(installDirectory, binName);
        try {
            await fs__WEBPACK_IMPORTED_MODULE_0___default().promises.unlink(file);
        }
        catch (err) {
            if (err.code !== `ENOENT`) {
                throw err;
            }
        }
    }
    async removeWin32Link(installDirectory, binName) {
        for (const ext of [``, `.ps1`, `.cmd`]) {
            const file = path__WEBPACK_IMPORTED_MODULE_1___default().join(installDirectory, `${binName}${ext}`);
            try {
                await fs__WEBPACK_IMPORTED_MODULE_0___default().promises.unlink(file);
            }
            catch (err) {
                if (err.code !== `ENOENT`) {
                    throw err;
                }
            }
        }
    }
}
DisableCommand.paths = [
    [`disable`],
];
DisableCommand.usage = clipanion__WEBPACK_IMPORTED_MODULE_4__.Command.Usage({
    description: `Remove the Corepack shims from the install directory`,
    details: `
      When run, this command will remove the shims for the specified package managers from the install directory, or all shims if no parameters are passed.

      By default it will locate the install directory by running the equivalent of \`which corepack\`, but this can be tweaked by explicitly passing the install directory via the \`--bin-folder\` flag.
    `,
    examples: [[
            `Disable all shims, removing them if they're next to the \`coreshim\` binary`,
            `$0 disable`,
        ], [
            `Disable all shims, removing them from the specified directory`,
            `$0 disable --install-directory /path/to/bin`,
        ], [
            `Disable the Yarn shim only`,
            `$0 disable yarn`,
        ]],
});


/***/ }),

/***/ "./sources/commands/Enable.ts":
/*!************************************!*\
  !*** ./sources/commands/Enable.ts ***!
  \************************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "EnableCommand": () => (/* binding */ EnableCommand)
/* harmony export */ });
/* harmony import */ var _zkochan_cmd_shim__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! @zkochan/cmd-shim */ "./.yarn/cache/@zkochan-cmd-shim-npm-5.0.0-9e90407ba0-ace99c7c36.zip/node_modules/@zkochan/cmd-shim/index.js");
/* harmony import */ var _zkochan_cmd_shim__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(_zkochan_cmd_shim__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var clipanion__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! clipanion */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! fs */ "fs");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(fs__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! path */ "path");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(path__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var which__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! which */ "./.yarn/cache/which-npm-2.0.2-320ddf72f7-1a5c563d3c.zip/node_modules/which/which.js");
/* harmony import */ var which__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(which__WEBPACK_IMPORTED_MODULE_3__);
/* harmony import */ var _types__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ../types */ "./sources/types.ts");






class EnableCommand extends clipanion__WEBPACK_IMPORTED_MODULE_5__.Command {
    constructor() {
        super(...arguments);
        this.installDirectory = clipanion__WEBPACK_IMPORTED_MODULE_5__.Option.String(`--install-directory`, {
            description: `Where the shims are to be installed`,
        });
        this.names = clipanion__WEBPACK_IMPORTED_MODULE_5__.Option.Rest();
    }
    async execute() {
        let installDirectory = this.installDirectory;
        // Node always call realpath on the module it executes, so we already
        // lost track of how the binary got called. To find it back, we need to
        // iterate over the PATH variable.
        if (typeof installDirectory === `undefined`)
            installDirectory = path__WEBPACK_IMPORTED_MODULE_2___default().dirname(await which__WEBPACK_IMPORTED_MODULE_3___default()(`corepack`));
        // Otherwise the relative symlink we'll compute will be incorrect, if the
        // install directory is within a symlink
        installDirectory = fs__WEBPACK_IMPORTED_MODULE_1___default().realpathSync(installDirectory);
        // We use `eval` so that Webpack doesn't statically transform it.
        const manifestPath = eval(`require`).resolve(`corepack/package.json`);
        const distFolder = path__WEBPACK_IMPORTED_MODULE_2___default().join(path__WEBPACK_IMPORTED_MODULE_2___default().dirname(manifestPath), `dist`);
        if (!fs__WEBPACK_IMPORTED_MODULE_1___default().existsSync(distFolder))
            throw new Error(`Assertion failed: The stub folder doesn't exist`);
        const names = this.names.length === 0
            ? _types__WEBPACK_IMPORTED_MODULE_4__.SupportedPackageManagerSetWithoutNpm
            : this.names;
        for (const name of new Set(names)) {
            if (!(0,_types__WEBPACK_IMPORTED_MODULE_4__.isSupportedPackageManager)(name))
                throw new clipanion__WEBPACK_IMPORTED_MODULE_5__.UsageError(`Invalid package manager name '${name}'`);
            for (const binName of this.context.engine.getBinariesFor(name)) {
                if (process.platform === `win32`) {
                    await this.generateWin32Link(installDirectory, distFolder, binName);
                }
                else {
                    await this.generatePosixLink(installDirectory, distFolder, binName);
                }
            }
        }
    }
    async generatePosixLink(installDirectory, distFolder, binName) {
        const file = path__WEBPACK_IMPORTED_MODULE_2___default().join(installDirectory, binName);
        const symlink = path__WEBPACK_IMPORTED_MODULE_2___default().relative(installDirectory, path__WEBPACK_IMPORTED_MODULE_2___default().join(distFolder, `${binName}.js`));
        if (fs__WEBPACK_IMPORTED_MODULE_1___default().existsSync(file)) {
            const currentSymlink = await fs__WEBPACK_IMPORTED_MODULE_1___default().promises.readlink(file);
            if (currentSymlink !== symlink) {
                await fs__WEBPACK_IMPORTED_MODULE_1___default().promises.unlink(file);
            }
            else {
                return;
            }
        }
        await fs__WEBPACK_IMPORTED_MODULE_1___default().promises.symlink(symlink, file);
    }
    async generateWin32Link(installDirectory, distFolder, binName) {
        const file = path__WEBPACK_IMPORTED_MODULE_2___default().join(installDirectory, binName);
        await _zkochan_cmd_shim__WEBPACK_IMPORTED_MODULE_0___default()(path__WEBPACK_IMPORTED_MODULE_2___default().join(distFolder, `${binName}.js`), file, {
            createCmdFile: true,
        });
    }
}
EnableCommand.paths = [
    [`enable`],
];
EnableCommand.usage = clipanion__WEBPACK_IMPORTED_MODULE_5__.Command.Usage({
    description: `Add the Corepack shims to the install directories`,
    details: `
      When run, this commmand will check whether the shims for the specified package managers can be found with the correct values inside the install directory. If not, or if they don't exist, they will be created.

      By default it will locate the install directory by running the equivalent of \`which corepack\`, but this can be tweaked by explicitly passing the install directory via the \`--bin-folder\` flag.
    `,
    examples: [[
            `Enable all shims, putting them next to the \`corepath\` binary`,
            `$0 enable`,
        ], [
            `Enable all shims, putting them in the specified directory`,
            `$0 enable --install-directory /path/to/folder`,
        ], [
            `Enable the Yarn shim only`,
            `$0 enable yarn`,
        ]],
});


/***/ }),

/***/ "./sources/commands/Hydrate.ts":
/*!*************************************!*\
  !*** ./sources/commands/Hydrate.ts ***!
  \*************************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "HydrateCommand": () => (/* binding */ HydrateCommand)
/* harmony export */ });
/* harmony import */ var clipanion__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! clipanion */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! path */ "path");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(path__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var _folderUtils__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../folderUtils */ "./sources/folderUtils.ts");
/* harmony import */ var _types__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../types */ "./sources/types.ts");




class HydrateCommand extends clipanion__WEBPACK_IMPORTED_MODULE_3__.Command {
    constructor() {
        super(...arguments);
        this.activate = clipanion__WEBPACK_IMPORTED_MODULE_3__.Option.Boolean(`--activate`, false, {
            description: `If true, this release will become the default one for this package manager`,
        });
        this.fileName = clipanion__WEBPACK_IMPORTED_MODULE_3__.Option.String();
    }
    async execute() {
        const installFolder = _folderUtils__WEBPACK_IMPORTED_MODULE_1__.getInstallFolder();
        const fileName = path__WEBPACK_IMPORTED_MODULE_0___default().resolve(this.context.cwd, this.fileName);
        const archiveEntries = new Map();
        let hasShortEntries = false;
        const { default: tar } = await Promise.resolve(/*! import() eager */).then(__webpack_require__.t.bind(__webpack_require__, /*! tar */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/index.js", 19));
        await tar.t({ file: fileName, onentry: entry => {
                const segments = entry.header.path.split(/\//g);
                if (segments.length < 3) {
                    hasShortEntries = true;
                }
                else {
                    let references = archiveEntries.get(segments[0]);
                    if (typeof references === `undefined`)
                        archiveEntries.set(segments[0], references = new Set());
                    references.add(segments[1]);
                }
            } });
        if (hasShortEntries || archiveEntries.size < 1)
            throw new clipanion__WEBPACK_IMPORTED_MODULE_3__.UsageError(`Invalid archive format; did it get generated by 'corepack prepare'?`);
        for (const [name, references] of archiveEntries) {
            for (const reference of references) {
                if (!(0,_types__WEBPACK_IMPORTED_MODULE_2__.isSupportedPackageManager)(name))
                    throw new clipanion__WEBPACK_IMPORTED_MODULE_3__.UsageError(`Unsupported package manager '${name}'`);
                if (this.activate)
                    this.context.stdout.write(`Hydrating ${name}@${reference} for immediate activation...\n`);
                else
                    this.context.stdout.write(`Hydrating ${name}@${reference}...\n`);
                await tar.x({ file: fileName, cwd: installFolder }, [`${name}/${reference}`]);
                if (this.activate) {
                    await this.context.engine.activatePackageManager({ name, reference });
                }
            }
        }
        this.context.stdout.write(`All done!\n`);
    }
}
HydrateCommand.paths = [
    [`hydrate`],
];
HydrateCommand.usage = clipanion__WEBPACK_IMPORTED_MODULE_3__.Command.Usage({
    description: `Import a package manager into the cache`,
    details: `
      This command unpacks a package manager archive into the cache. The archive must have been generated by the \`corepack prepare\` command - no other will work.
    `,
    examples: [[
            `Import a package manager in the cache`,
            `$0 hydrate corepack.tgz`,
        ]],
});


/***/ }),

/***/ "./sources/commands/Prepare.ts":
/*!*************************************!*\
  !*** ./sources/commands/Prepare.ts ***!
  \*************************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "PrepareCommand": () => (/* binding */ PrepareCommand)
/* harmony export */ });
/* harmony import */ var clipanion__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! clipanion */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! path */ "path");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(path__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var _folderUtils__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../folderUtils */ "./sources/folderUtils.ts");
/* harmony import */ var _specUtils__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../specUtils */ "./sources/specUtils.ts");




class PrepareCommand extends clipanion__WEBPACK_IMPORTED_MODULE_3__.Command {
    constructor() {
        super(...arguments);
        this.activate = clipanion__WEBPACK_IMPORTED_MODULE_3__.Option.Boolean(`--activate`, false, {
            description: `If true, this release will become the default one for this package manager`,
        });
        this.all = clipanion__WEBPACK_IMPORTED_MODULE_3__.Option.Boolean(`--all`, false, {
            description: `If true, all available default package managers will be installed`,
        });
        this.json = clipanion__WEBPACK_IMPORTED_MODULE_3__.Option.Boolean(`--json`, false, {
            description: `If true, the output will be the path of the generated tarball`,
        });
        this.output = clipanion__WEBPACK_IMPORTED_MODULE_3__.Option.String(`-o,--output`, {
            description: `If true, the installed package managers will also be stored in a tarball`,
            tolerateBoolean: true,
        });
        this.specs = clipanion__WEBPACK_IMPORTED_MODULE_3__.Option.Rest();
    }
    async execute() {
        if (this.all && this.specs.length > 0)
            throw new clipanion__WEBPACK_IMPORTED_MODULE_3__.UsageError(`The --all option cannot be used along with an explicit package manager specification`);
        const specs = this.all
            ? await this.context.engine.getDefaultDescriptors()
            : this.specs;
        const installLocations = [];
        for (const request of specs) {
            let spec;
            if (typeof request === `undefined`) {
                const lookup = await _specUtils__WEBPACK_IMPORTED_MODULE_2__.loadSpec(this.context.cwd);
                switch (lookup.type) {
                    case `NoProject`:
                        throw new clipanion__WEBPACK_IMPORTED_MODULE_3__.UsageError(`Couldn't find a project in the local directory - please explicit the package manager to pack, or run this command from a valid project`);
                    case `NoSpec`:
                        throw new clipanion__WEBPACK_IMPORTED_MODULE_3__.UsageError(`The local project doesn't feature a 'packageManager' field - please explicit the package manager to pack, or update the manifest to reference it`);
                    default: {
                        spec = lookup.spec;
                    }
                }
            }
            else {
                spec = typeof request === `string`
                    ? _specUtils__WEBPACK_IMPORTED_MODULE_2__.parseSpec(request, `CLI arguments`)
                    : request;
            }
            const resolved = await this.context.engine.resolveDescriptor(spec);
            if (resolved === null)
                throw new clipanion__WEBPACK_IMPORTED_MODULE_3__.UsageError(`Failed to successfully resolve '${spec.range}' to a valid ${spec.name} release`);
            if (!this.json) {
                if (this.activate) {
                    this.context.stdout.write(`Preparing ${spec.name}@${spec.range} for immediate activation...\n`);
                }
                else {
                    this.context.stdout.write(`Preparing ${spec.name}@${spec.range}...\n`);
                }
            }
            const installSpec = await this.context.engine.ensurePackageManager(resolved);
            installLocations.push(installSpec.location);
            if (this.activate) {
                await this.context.engine.activatePackageManager(resolved);
            }
        }
        if (this.output) {
            const outputName = typeof this.output === `string`
                ? this.output
                : `corepack.tgz`;
            const baseInstallFolder = _folderUtils__WEBPACK_IMPORTED_MODULE_1__.getInstallFolder();
            const outputPath = path__WEBPACK_IMPORTED_MODULE_0___default().resolve(this.context.cwd, outputName);
            if (!this.json)
                this.context.stdout.write(`Packing the selected tools in ${path__WEBPACK_IMPORTED_MODULE_0___default().basename(outputPath)}...\n`);
            const { default: tar } = await Promise.resolve(/*! import() eager */).then(__webpack_require__.t.bind(__webpack_require__, /*! tar */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/index.js", 19));
            await tar.c({ gzip: true, cwd: baseInstallFolder, file: path__WEBPACK_IMPORTED_MODULE_0___default().resolve(outputPath) }, installLocations.map(location => {
                return path__WEBPACK_IMPORTED_MODULE_0___default().relative(baseInstallFolder, location);
            }));
            if (this.json) {
                this.context.stdout.write(`${JSON.stringify(outputPath)}\n`);
            }
            else {
                this.context.stdout.write(`All done!\n`);
            }
        }
    }
}
PrepareCommand.paths = [
    [`prepare`],
];
PrepareCommand.usage = clipanion__WEBPACK_IMPORTED_MODULE_3__.Command.Usage({
    description: `Generate a package manager archive`,
    details: `
      This command makes sure that the specified package managers are installed in the local cache. Calling this command explicitly unless you operate in an environment without network access (in which case you'd have to call \`prepare\` while building your image, to make sure all tools are available for later use).

      When the \`-o,--output\` flag is set, Corepack will also compress the resulting package manager into a format suitable for \`corepack hydrate\`, and will store it at the specified location on the disk.
    `,
    examples: [[
            `Prepare the package manager from the active project`,
            `$0 prepare`,
        ], [
            `Prepare a specific Yarn version`,
            `$0 prepare yarn@2.2.2`,
        ], [
            `Generate an archive for a specific Yarn version`,
            `$0 prepare yarn@2.2.2 -o`,
        ], [
            `Generate a named archive`,
            `$0 prepare yarn@2.2.2 --output=yarn.tgz`,
        ]],
});


/***/ }),

/***/ "./sources/corepackUtils.ts":
/*!**********************************!*\
  !*** ./sources/corepackUtils.ts ***!
  \**********************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "fetchAvailableTags": () => (/* binding */ fetchAvailableTags),
/* harmony export */   "fetchAvailableVersions": () => (/* binding */ fetchAvailableVersions),
/* harmony export */   "findInstalledVersion": () => (/* binding */ findInstalledVersion),
/* harmony export */   "installVersion": () => (/* binding */ installVersion),
/* harmony export */   "runVersion": () => (/* binding */ runVersion)
/* harmony export */ });
/* harmony import */ var child_process__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! child_process */ "child_process");
/* harmony import */ var child_process__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(child_process__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! fs */ "fs");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(fs__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! path */ "path");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(path__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var semver__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/index.js");
/* harmony import */ var semver__WEBPACK_IMPORTED_MODULE_3___default = /*#__PURE__*/__webpack_require__.n(semver__WEBPACK_IMPORTED_MODULE_3__);
/* harmony import */ var _debugUtils__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ./debugUtils */ "./sources/debugUtils.ts");
/* harmony import */ var _folderUtils__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ./folderUtils */ "./sources/folderUtils.ts");
/* harmony import */ var _fsUtils__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(/*! ./fsUtils */ "./sources/fsUtils.ts");
/* harmony import */ var _httpUtils__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(/*! ./httpUtils */ "./sources/httpUtils.ts");








async function fetchAvailableTags(spec) {
    switch (spec.type) {
        case `npm`: {
            const data = await _httpUtils__WEBPACK_IMPORTED_MODULE_7__.fetchAsJson(`https://registry.npmjs.org/${spec.package}`, { headers: { [`Accept`]: `application/vnd.npm.install-v1+json` } });
            return data[`dist-tags`];
        }
        case `url`: {
            const data = await _httpUtils__WEBPACK_IMPORTED_MODULE_7__.fetchAsJson(spec.url);
            return data[spec.fields.tags];
        }
        default: {
            throw new Error(`Unsupported specification ${JSON.stringify(spec)}`);
        }
    }
}
async function fetchAvailableVersions(spec) {
    switch (spec.type) {
        case `npm`: {
            const data = await _httpUtils__WEBPACK_IMPORTED_MODULE_7__.fetchAsJson(`https://registry.npmjs.org/${spec.package}`, { headers: { [`Accept`]: `application/vnd.npm.install-v1+json` } });
            return Object.keys(data.versions);
        }
        case `url`: {
            const data = await _httpUtils__WEBPACK_IMPORTED_MODULE_7__.fetchAsJson(spec.url);
            const field = data[spec.fields.versions];
            return Array.isArray(field) ? field : Object.keys(field);
        }
        default: {
            throw new Error(`Unsupported specification ${JSON.stringify(spec)}`);
        }
    }
}
async function findInstalledVersion(installTarget, descriptor) {
    const installFolder = path__WEBPACK_IMPORTED_MODULE_2___default().join(installTarget, descriptor.name);
    let folderContent;
    try {
        folderContent = await fs__WEBPACK_IMPORTED_MODULE_1___default().promises.readdir(installFolder);
    }
    catch (error) {
        if (error.code === `ENOENT`) {
            folderContent = [];
        }
        else {
            throw error;
        }
    }
    const candidateVersions = [];
    for (const entry of folderContent) {
        // Some dot-folders tend to pop inside directories, especially on OSX
        if (entry.startsWith(`.`))
            continue;
        candidateVersions.push(entry);
    }
    const bestMatch = semver__WEBPACK_IMPORTED_MODULE_3___default().maxSatisfying(candidateVersions, descriptor.range);
    if (bestMatch === null)
        return null;
    return bestMatch;
}
async function installVersion(installTarget, locator, { spec }) {
    const { default: tar } = await Promise.resolve(/*! import() eager */).then(__webpack_require__.t.bind(__webpack_require__, /*! tar */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/index.js", 19));
    const installFolder = path__WEBPACK_IMPORTED_MODULE_2___default().join(installTarget, locator.name, locator.reference);
    if (fs__WEBPACK_IMPORTED_MODULE_1___default().existsSync(installFolder)) {
        _debugUtils__WEBPACK_IMPORTED_MODULE_4__.log(`Reusing ${locator.name}@${locator.reference}`);
        return installFolder;
    }
    const url = spec.url.replace(`{}`, locator.reference);
    _debugUtils__WEBPACK_IMPORTED_MODULE_4__.log(`Installing ${locator.name}@${locator.reference} from ${url}`);
    return await _fsUtils__WEBPACK_IMPORTED_MODULE_6__.mutex(installFolder, async () => {
        // Creating a temporary folder inside the install folder means that we
        // are sure it'll be in the same drive as the destination, so we can
        // just move it there atomically once we are done
        const tmpFolder = _folderUtils__WEBPACK_IMPORTED_MODULE_5__.getTemporaryFolder(installTarget);
        const stream = await _httpUtils__WEBPACK_IMPORTED_MODULE_7__.fetchUrlStream(url);
        const parsedUrl = new URL(url);
        const ext = path__WEBPACK_IMPORTED_MODULE_2___default().posix.extname(parsedUrl.pathname);
        let outputFile = null;
        let sendTo;
        if (ext === `.tgz`) {
            sendTo = tar.x({ strip: 1, cwd: tmpFolder });
        }
        else if (ext === `.js`) {
            outputFile = path__WEBPACK_IMPORTED_MODULE_2___default().join(tmpFolder, path__WEBPACK_IMPORTED_MODULE_2___default().posix.basename(parsedUrl.pathname));
            sendTo = fs__WEBPACK_IMPORTED_MODULE_1___default().createWriteStream(outputFile);
        }
        stream.pipe(sendTo);
        await new Promise(resolve => {
            sendTo.on(`finish`, resolve);
        });
        await fs__WEBPACK_IMPORTED_MODULE_1___default().promises.mkdir(path__WEBPACK_IMPORTED_MODULE_2___default().dirname(installFolder), { recursive: true });
        await fs__WEBPACK_IMPORTED_MODULE_1___default().promises.rename(tmpFolder, installFolder);
        _debugUtils__WEBPACK_IMPORTED_MODULE_4__.log(`Install finished`);
        return installFolder;
    });
}
async function runVersion(installSpec, locator, binName, args, context) {
    let binPath = null;
    if (Array.isArray(installSpec.spec.bin)) {
        if (installSpec.spec.bin.some(bin => bin === binName)) {
            const parsedUrl = new URL(installSpec.spec.url);
            const ext = path__WEBPACK_IMPORTED_MODULE_2___default().posix.extname(parsedUrl.pathname);
            if (ext === `.js`) {
                binPath = path__WEBPACK_IMPORTED_MODULE_2___default().join(installSpec.location, path__WEBPACK_IMPORTED_MODULE_2___default().posix.basename(parsedUrl.pathname));
            }
        }
    }
    else {
        for (const [name, dest] of Object.entries(installSpec.spec.bin)) {
            if (name === binName) {
                binPath = path__WEBPACK_IMPORTED_MODULE_2___default().join(installSpec.location, dest);
                break;
            }
        }
    }
    if (!binPath)
        throw new Error(`Assertion failed: Unable to locate path for bin '${binName}'`);
    return new Promise((resolve, reject) => {
        process.on(`SIGINT`, () => {
            // We don't want to exit the process before the child, so we just
            // ignore SIGINT and wait for the regular exit to happen (the child
            // will receive SIGINT too since it's part of the same process grp)
        });
        const stdio = [`pipe`, `pipe`, `pipe`];
        if (context.stdin === process.stdin)
            stdio[0] = `inherit`;
        if (context.stdout === process.stdout)
            stdio[1] = `inherit`;
        if (context.stderr === process.stderr)
            stdio[2] = `inherit`;
        const v8CompileCache = typeof require !== `undefined`
            ? eval(`require`).resolve(`./vcc.js`)
            : eval(`require`).resolve(`corepack/dist/vcc.js`);
        const child = (0,child_process__WEBPACK_IMPORTED_MODULE_0__.spawn)(process.execPath, [`--require`, v8CompileCache, binPath, ...args], {
            cwd: context.cwd,
            stdio,
            env: Object.assign(Object.assign({}, process.env), { COREPACK_ROOT: path__WEBPACK_IMPORTED_MODULE_2___default().dirname(eval(`__dirname`)) }),
        });
        activeChildren.add(child);
        if (activeChildren.size === 1) {
            process.on(`SIGINT`, sigintHandler);
            process.on(`SIGTERM`, sigtermHandler);
        }
        if (context.stdin !== process.stdin)
            context.stdin.pipe(child.stdin);
        if (context.stdout !== process.stdout)
            child.stdout.pipe(context.stdout);
        if (context.stderr !== process.stderr)
            child.stderr.pipe(context.stderr);
        child.on(`error`, error => {
            activeChildren.delete(child);
            if (activeChildren.size === 0) {
                process.off(`SIGINT`, sigintHandler);
                process.off(`SIGTERM`, sigtermHandler);
            }
            reject(error);
        });
        child.on(`exit`, exitCode => {
            activeChildren.delete(child);
            if (activeChildren.size === 0) {
                process.off(`SIGINT`, sigintHandler);
                process.off(`SIGTERM`, sigtermHandler);
            }
            resolve(exitCode !== null ? exitCode : 1);
        });
    });
}
const activeChildren = new Set();
function sigintHandler() {
    // We don't want SIGINT to kill our process; we want it to kill the
    // innermost process, whose end will cause our own to exit.
}
function sigtermHandler() {
    for (const child of activeChildren) {
        child.kill();
    }
}


/***/ }),

/***/ "./sources/debugUtils.ts":
/*!*******************************!*\
  !*** ./sources/debugUtils.ts ***!
  \*******************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "log": () => (/* binding */ log)
/* harmony export */ });
/* harmony import */ var debug__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! debug */ "./.yarn/__virtual__/debug-virtual-d208043b83/0/cache/debug-npm-4.1.1-540248b3aa-1e681f5cce.zip/node_modules/debug/src/index.js");
/* harmony import */ var debug__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(debug__WEBPACK_IMPORTED_MODULE_0__);

const log = debug__WEBPACK_IMPORTED_MODULE_0___default()(`corepack`);


/***/ }),

/***/ "./sources/folderUtils.ts":
/*!********************************!*\
  !*** ./sources/folderUtils.ts ***!
  \********************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "getInstallFolder": () => (/* binding */ getInstallFolder),
/* harmony export */   "getTemporaryFolder": () => (/* binding */ getTemporaryFolder)
/* harmony export */ });
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! fs */ "fs");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(fs__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var os__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! os */ "os");
/* harmony import */ var os__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(os__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! path */ "path");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(path__WEBPACK_IMPORTED_MODULE_2__);



function getInstallFolder() {
    var _a;
    return (_a = process.env.COREPACK_HOME) !== null && _a !== void 0 ? _a : (0,path__WEBPACK_IMPORTED_MODULE_2__.join)((0,os__WEBPACK_IMPORTED_MODULE_1__.homedir)(), `.node/corepack`);
}
function getTemporaryFolder(target = (0,os__WEBPACK_IMPORTED_MODULE_1__.tmpdir)()) {
    (0,fs__WEBPACK_IMPORTED_MODULE_0__.mkdirSync)(target, { recursive: true });
    while (true) {
        const rnd = Math.random() * 0x100000000;
        const hex = rnd.toString(16).padStart(8, `0`);
        const path = (0,path__WEBPACK_IMPORTED_MODULE_2__.join)(target, `corepack-${process.pid}-${hex}`);
        try {
            (0,fs__WEBPACK_IMPORTED_MODULE_0__.mkdirSync)(path);
            return path;
        }
        catch (error) {
            if (error.code === `EEXIST`) {
                continue;
            }
            else {
                throw error;
            }
        }
    }
}


/***/ }),

/***/ "./sources/fsUtils.ts":
/*!****************************!*\
  !*** ./sources/fsUtils.ts ***!
  \****************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "mutex": () => (/* binding */ mutex)
/* harmony export */ });
async function mutex(p, cb) {
    return await cb();
}


/***/ }),

/***/ "./sources/httpUtils.ts":
/*!******************************!*\
  !*** ./sources/httpUtils.ts ***!
  \******************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "fetchUrlStream": () => (/* binding */ fetchUrlStream),
/* harmony export */   "fetchAsBuffer": () => (/* binding */ fetchAsBuffer),
/* harmony export */   "fetchAsJson": () => (/* binding */ fetchAsJson)
/* harmony export */ });
/* harmony import */ var clipanion__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! clipanion */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js");

async function fetchUrlStream(url, options = {}) {
    if (process.env.COREPACK_ENABLE_NETWORK === `0`)
        throw new clipanion__WEBPACK_IMPORTED_MODULE_0__.UsageError(`Network access disabled by the environment; can't reach ${url}`);
    const { default: https } = await Promise.resolve(/*! import() */).then(__webpack_require__.t.bind(__webpack_require__, /*! https */ "https", 23));
    return new Promise((resolve, reject) => {
        const request = https.get(url, options, response => {
            var _a;
            const statusCode = (_a = response.statusCode) !== null && _a !== void 0 ? _a : 500;
            if (!(statusCode >= 200 && statusCode < 300))
                return reject(new Error(`Server answered with HTTP ${statusCode}`));
            return resolve(response);
        });
        request.on(`error`, err => {
            reject(new Error(`Error when performing the request`));
        });
    });
}
async function fetchAsBuffer(url, options) {
    const response = await fetchUrlStream(url, options);
    return new Promise((resolve, reject) => {
        const chunks = [];
        response.on(`data`, chunk => {
            chunks.push(chunk);
        });
        response.on(`error`, error => {
            reject(error);
        });
        response.on(`end`, () => {
            resolve(Buffer.concat(chunks));
        });
    });
}
async function fetchAsJson(url, options) {
    const buffer = await fetchAsBuffer(url, options);
    const asText = buffer.toString();
    try {
        return JSON.parse(asText);
    }
    catch (error) {
        const truncated = asText.length > 30
            ? `${asText.slice(0, 30)}...`
            : asText;
        throw new Error(`Couldn't parse JSON data: ${JSON.stringify(truncated)}`);
    }
}


/***/ }),

/***/ "./sources/miscUtils.ts":
/*!******************************!*\
  !*** ./sources/miscUtils.ts ***!
  \******************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "Cancellation": () => (/* binding */ Cancellation)
/* harmony export */ });
class Cancellation extends Error {
    constructor() {
        super(`Cancelled operation`);
    }
}


/***/ }),

/***/ "./sources/semverUtils.ts":
/*!********************************!*\
  !*** ./sources/semverUtils.ts ***!
  \********************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "satisfiesWithPrereleases": () => (/* binding */ satisfiesWithPrereleases)
/* harmony export */ });
/* harmony import */ var semver__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/index.js");
/* harmony import */ var semver__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(semver__WEBPACK_IMPORTED_MODULE_0__);

/**
 * Returns whether the given semver version satisfies the given range. Notably
 * this supports prerelease versions so that "2.0.0-rc.0" satisfies the range
 * ">=1.0.0", for example.
 *
 * This function exists because the semver.satisfies method does not include
 * pre releases. This means ranges such as * would not satisfy 1.0.0-rc. The
 * includePrerelease flag has a weird behavior and cannot be used (if you want
 * to try it out, just run the `semverUtils` testsuite using this flag instead
 * of our own implementation, and you'll see the failing cases).
 *
 * See https://github.com/yarnpkg/berry/issues/575 for more context.
 */
function satisfiesWithPrereleases(version, range, loose = false) {
    let semverRange;
    try {
        semverRange = new (semver__WEBPACK_IMPORTED_MODULE_0___default().Range)(range, loose);
    }
    catch (err) {
        return false;
    }
    if (!version)
        return false;
    let semverVersion;
    try {
        semverVersion = new (semver__WEBPACK_IMPORTED_MODULE_0___default().SemVer)(version, semverRange.loose);
        if (semverVersion.prerelease) {
            semverVersion.prerelease = [];
        }
    }
    catch (err) {
        return false;
    }
    // A range has multiple sets of comparators. A version must satisfy all
    // comparators in a set and at least one set to satisfy the range.
    return semverRange.set.some(comparatorSet => {
        for (const comparator of comparatorSet)
            if (comparator.semver.prerelease)
                comparator.semver.prerelease = [];
        return comparatorSet.every(comparator => {
            return comparator.test(semverVersion);
        });
    });
}


/***/ }),

/***/ "./sources/specUtils.ts":
/*!******************************!*\
  !*** ./sources/specUtils.ts ***!
  \******************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "parseSpec": () => (/* binding */ parseSpec),
/* harmony export */   "findProjectSpec": () => (/* binding */ findProjectSpec),
/* harmony export */   "loadSpec": () => (/* binding */ loadSpec)
/* harmony export */ });
/* harmony import */ var clipanion__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! clipanion */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! fs */ "fs");
/* harmony import */ var fs__WEBPACK_IMPORTED_MODULE_0___default = /*#__PURE__*/__webpack_require__.n(fs__WEBPACK_IMPORTED_MODULE_0__);
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! path */ "path");
/* harmony import */ var path__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(path__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var semver__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/index.js");
/* harmony import */ var semver__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(semver__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var _types__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./types */ "./sources/types.ts");





const nodeModulesRegExp = /[\\/]node_modules[\\/](@[^\\/]*[\\/])?([^@\\/][^\\/]*)$/;
function parseSpec(raw, source) {
    if (typeof raw !== `string`)
        throw new clipanion__WEBPACK_IMPORTED_MODULE_4__.UsageError(`Invalid package manager specification in ${source}; expected a string`);
    const match = raw.match(/^(?!_)(.+)@(.+)$/);
    if (match === null || !semver__WEBPACK_IMPORTED_MODULE_2___default().valid(match[2]))
        throw new clipanion__WEBPACK_IMPORTED_MODULE_4__.UsageError(`Invalid package manager specification in ${source}; expected a semver version`);
    if (!(0,_types__WEBPACK_IMPORTED_MODULE_3__.isSupportedPackageManager)(match[1]))
        throw new clipanion__WEBPACK_IMPORTED_MODULE_4__.UsageError(`Unsupported package manager specification (${match})`);
    return {
        name: match[1],
        range: match[2],
    };
}
/**
 * Locates the active project's package manager specification.
 *
 * If the specification exists but doesn't match the active package manager,
 * an error is thrown to prevent users from using the wrong package manager,
 * which would lead to inconsistent project layouts.
 *
 * If the project doesn't include a specification file, we just assume that
 * whatever the user uses is exactly what they want to use. Since the version
 * isn't explicited, we fallback on known good versions.
 *
 * Finally, if the project doesn't exist at all, we ask the user whether they
 * want to create one in the current project. If they do, we initialize a new
 * project using the default package managers, and configure it so that we
 * don't need to ask again in the future.
 */
async function findProjectSpec(initialCwd, locator, { transparent = false } = {}) {
    // A locator is a valid descriptor (but not the other way around)
    const fallbackLocator = { name: locator.name, range: locator.reference };
    while (true) {
        const result = await loadSpec(initialCwd);
        switch (result.type) {
            case `NoProject`:
            case `NoSpec`:
                {
                    return fallbackLocator;
                }
                break;
            case `Found`:
                {
                    if (result.spec.name !== locator.name) {
                        if (transparent) {
                            return fallbackLocator;
                        }
                        else {
                            throw new clipanion__WEBPACK_IMPORTED_MODULE_4__.UsageError(`This project is configured to use ${result.spec.name}`);
                        }
                    }
                    else {
                        return result.spec;
                    }
                }
                break;
        }
    }
}
async function loadSpec(initialCwd) {
    let nextCwd = initialCwd;
    let currCwd = ``;
    let selection = null;
    while (nextCwd !== currCwd && (!selection || !selection.data.packageManager)) {
        currCwd = nextCwd;
        nextCwd = path__WEBPACK_IMPORTED_MODULE_1___default().dirname(currCwd);
        if (nodeModulesRegExp.test(currCwd))
            continue;
        const manifestPath = path__WEBPACK_IMPORTED_MODULE_1___default().join(currCwd, `package.json`);
        if (!fs__WEBPACK_IMPORTED_MODULE_0___default().existsSync(manifestPath))
            continue;
        const content = await fs__WEBPACK_IMPORTED_MODULE_0___default().promises.readFile(manifestPath, `utf8`);
        let data;
        try {
            data = JSON.parse(content);
        }
        catch (_a) { }
        if (typeof data !== `object` || data === null)
            throw new clipanion__WEBPACK_IMPORTED_MODULE_4__.UsageError(`Invalid package.json in ${path__WEBPACK_IMPORTED_MODULE_1___default().relative(initialCwd, manifestPath)}`);
        selection = { data, manifestPath };
    }
    if (selection === null)
        return { type: `NoProject`, target: path__WEBPACK_IMPORTED_MODULE_1___default().join(initialCwd, `package.json`) };
    const rawPmSpec = selection.data.packageManager;
    if (typeof rawPmSpec === `undefined`)
        return { type: `NoSpec`, target: selection.manifestPath };
    return {
        type: `Found`,
        spec: parseSpec(rawPmSpec, path__WEBPACK_IMPORTED_MODULE_1___default().relative(initialCwd, selection.manifestPath)),
    };
}


/***/ }),

/***/ "./sources/types.ts":
/*!**************************!*\
  !*** ./sources/types.ts ***!
  \**************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "SupportedPackageManagers": () => (/* binding */ SupportedPackageManagers),
/* harmony export */   "SupportedPackageManagerSet": () => (/* binding */ SupportedPackageManagerSet),
/* harmony export */   "SupportedPackageManagerSetWithoutNpm": () => (/* binding */ SupportedPackageManagerSetWithoutNpm),
/* harmony export */   "isSupportedPackageManager": () => (/* binding */ isSupportedPackageManager)
/* harmony export */ });
var SupportedPackageManagers;
(function (SupportedPackageManagers) {
    SupportedPackageManagers["Npm"] = "npm";
    SupportedPackageManagers["Pnpm"] = "pnpm";
    SupportedPackageManagers["Yarn"] = "yarn";
})(SupportedPackageManagers || (SupportedPackageManagers = {}));
const SupportedPackageManagerSet = new Set(Object.values(SupportedPackageManagers));
const SupportedPackageManagerSetWithoutNpm = new Set(Object.values(SupportedPackageManagers));
// npm is distributed with Node as a builtin; we don't want Corepack to override it unless the npm team is on board
SupportedPackageManagerSetWithoutNpm.delete(SupportedPackageManagers.Npm);
function isSupportedPackageManager(value) {
    return SupportedPackageManagerSet.has(value);
}


/***/ }),

/***/ "./.yarn/cache/@zkochan-cmd-shim-npm-5.0.0-9e90407ba0-ace99c7c36.zip/node_modules/@zkochan/cmd-shim/index.js":
/*!*******************************************************************************************************************!*\
  !*** ./.yarn/cache/@zkochan-cmd-shim-npm-5.0.0-9e90407ba0-ace99c7c36.zip/node_modules/@zkochan/cmd-shim/index.js ***!
  \*******************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

cmdShim.ifExists = cmdShimIfExists;
const util_1 = __webpack_require__(/*! util */ "util");
const path = __webpack_require__(/*! path */ "path");
const isWindows = __webpack_require__(/*! is-windows */ "./.yarn/cache/is-windows-npm-1.0.2-898cd6f3d7-438b7e5265.zip/node_modules/is-windows/index.js");
const shebangExpr = /^#!\s*(?:\/usr\/bin\/env)?\s*([^ \t]+)(.*)$/;
const DEFAULT_OPTIONS = {
    // Create PowerShell file by default if the option hasn't been specified
    createPwshFile: true,
    createCmdFile: isWindows(),
    fs: __webpack_require__(/*! fs */ "fs")
};
/**
 * Map from extensions of files that this module is frequently used for to their runtime.
 * @type {Map<string, string>}
 */
const extensionToProgramMap = new Map([
    ['.js', 'node'],
    ['.cmd', 'cmd'],
    ['.bat', 'cmd'],
    ['.ps1', 'pwsh'],
    ['.sh', 'sh']
]);
function ingestOptions(opts) {
    const opts_ = { ...DEFAULT_OPTIONS, ...opts };
    const fs = opts_.fs;
    opts_.fs_ = {
        chmod: fs.chmod ? util_1.promisify(fs.chmod) : (async () => { }),
        mkdir: util_1.promisify(fs.mkdir),
        readFile: util_1.promisify(fs.readFile),
        stat: util_1.promisify(fs.stat),
        unlink: util_1.promisify(fs.unlink),
        writeFile: util_1.promisify(fs.writeFile)
    };
    return opts_;
}
/**
 * Try to create shims.
 *
 * @param src Path to program (executable or script).
 * @param to Path to shims.
 * Don't add an extension if you will create multiple types of shims.
 * @param opts Options.
 * @throws If `src` is missing.
 */
async function cmdShim(src, to, opts) {
    const opts_ = ingestOptions(opts);
    await opts_.fs_.stat(src);
    await cmdShim_(src, to, opts_);
}
/**
 * Try to create shims.
 *
 * Does nothing if `src` doesn't exist.
 *
 * @param src Path to program (executable or script).
 * @param to Path to shims.
 * Don't add an extension if you will create multiple types of shims.
 * @param opts Options.
 */
function cmdShimIfExists(src, to, opts) {
    return cmdShim(src, to, opts).catch(() => { });
}
/**
 * Try to unlink, but ignore errors.
 * Any problems will surface later.
 *
 * @param path File to be removed.
 */
function rm(path, opts) {
    return opts.fs_.unlink(path).catch(() => { });
}
/**
 * Try to create shims **even if `src` is missing**.
 *
 * @param src Path to program (executable or script).
 * @param to Path to shims.
 * Don't add an extension if you will create multiple types of shims.
 * @param opts Options.
 */
async function cmdShim_(src, to, opts) {
    const srcRuntimeInfo = await searchScriptRuntime(src, opts);
    // Always tries to create all types of shims by calling `writeAllShims` as of now.
    // Append your code here to change the behavior in response to `srcRuntimeInfo`.
    // Create 3 shims for (Ba)sh in Cygwin / MSYS, no extension) & CMD (.cmd) & PowerShell (.ps1)
    await writeShimsPreCommon(to, opts);
    return writeAllShims(src, to, srcRuntimeInfo, opts);
}
/**
 * Do processes before **all** shims are created.
 * This must be called **only once** for one call of `cmdShim(IfExists)`.
 *
 * @param target Path of shims that are going to be created.
 */
function writeShimsPreCommon(target, opts) {
    return opts.fs_.mkdir(path.dirname(target), { recursive: true });
}
/**
 * Write all types (sh & cmd & pwsh) of shims to files.
 * Extensions (`.cmd` and `.ps1`) are appended to cmd and pwsh shims.
 *
 *
 * @param src Path to program (executable or script).
 * @param to Path to shims **without extensions**.
 * Extensions are added for CMD and PowerShell shims.
 * @param srcRuntimeInfo Return value of `await searchScriptRuntime(src)`.
 * @param opts Options.
 */
function writeAllShims(src, to, srcRuntimeInfo, opts) {
    const opts_ = ingestOptions(opts);
    const generatorAndExts = [{ generator: generateShShim, extension: '' }];
    if (opts_.createCmdFile) {
        generatorAndExts.push({ generator: generateCmdShim, extension: '.cmd' });
    }
    if (opts_.createPwshFile) {
        generatorAndExts.push({ generator: generatePwshShim, extension: '.ps1' });
    }
    return Promise.all(generatorAndExts.map((generatorAndExt) => writeShim(src, to + generatorAndExt.extension, srcRuntimeInfo, generatorAndExt.generator, opts_)));
}
/**
 * Do processes before writing shim.
 *
 * @param target Path to shim that is going to be created.
 */
function writeShimPre(target, opts) {
    return rm(target, opts);
}
/**
 * Do processes after writing the shim.
 *
 * @param target Path to just created shim.
 */
function writeShimPost(target, opts) {
    // Only chmoding shims as of now.
    // Some other processes may be appended.
    return chmodShim(target, opts);
}
/**
 * Look into runtime (e.g. `node` & `sh` & `pwsh`) and its arguments
 * of the target program (script or executable).
 *
 * @param target Path to the executable or script.
 * @return Promise of infomation of runtime of `target`.
 */
async function searchScriptRuntime(target, opts) {
    const data = await opts.fs_.readFile(target, 'utf8');
    // First, check if the bin is a #! of some sort.
    const firstLine = data.trim().split(/\r*\n/)[0];
    const shebang = firstLine.match(shebangExpr);
    if (!shebang) {
        // If not, infer script type from its extension.
        // If the inference fails, it's something that'll be compiled, or some other
        // sort of script, and just call it directly.
        const targetExtension = path.extname(target).toLowerCase();
        return {
            // undefined if extension is unknown but it's converted to null.
            program: extensionToProgramMap.get(targetExtension) || null,
            additionalArgs: ''
        };
    }
    return {
        program: shebang[1],
        additionalArgs: shebang[2]
    };
}
/**
 * Write shim to the file system while executing the pre- and post-processes
 * defined in `WriteShimPre` and `WriteShimPost`.
 *
 * @param src Path to the executable or script.
 * @param to Path to the (sh) shim(s) that is going to be created.
 * @param srcRuntimeInfo Result of `await searchScriptRuntime(src)`.
 * @param generateShimScript Generator of shim script.
 * @param opts Other options.
 */
async function writeShim(src, to, srcRuntimeInfo, generateShimScript, opts) {
    const defaultArgs = opts.preserveSymlinks ? '--preserve-symlinks' : '';
    // `Array.prototype.filter` removes ''.
    // ['--foo', '--bar'].join(' ') and [].join(' ') returns '--foo --bar' and '' respectively.
    const args = [srcRuntimeInfo.additionalArgs, defaultArgs].filter(arg => arg).join(' ');
    opts = Object.assign({}, opts, {
        prog: srcRuntimeInfo.program,
        args: args
    });
    await writeShimPre(to, opts);
    await opts.fs_.writeFile(to, generateShimScript(src, to, opts), 'utf8');
    return writeShimPost(to, opts);
}
/**
 * Generate the content of a shim for CMD.
 *
 * @param src Path to the executable or script.
 * @param to Path to the shim to be created.
 * It is highly recommended to end with `.cmd` (or `.bat`).
 * @param opts Options.
 * @return The content of shim.
 */
function generateCmdShim(src, to, opts) {
    // `shTarget` is not used to generate the content.
    const shTarget = path.relative(path.dirname(to), src);
    let target = shTarget.split('/').join('\\');
    const quotedPathToTarget = path.isAbsolute(target) ? `"${target}"` : `"%~dp0\\${target}"`;
    let longProg;
    let prog = opts.prog;
    let args = opts.args || '';
    const nodePath = normalizePathEnvVar(opts.nodePath).win32;
    if (!prog) {
        prog = quotedPathToTarget;
        args = '';
        target = '';
    }
    else {
        longProg = `"%~dp0\\${prog}.exe"`;
        target = quotedPathToTarget;
    }
    let progArgs = opts.progArgs ? `${opts.progArgs.join(` `)} ` : '';
    // @IF EXIST "%~dp0\node.exe" (
    //   "%~dp0\node.exe" "%~dp0\.\node_modules\npm\bin\npm-cli.js" %*
    // ) ELSE (
    //   SETLOCAL
    //   SET PATHEXT=%PATHEXT:;.JS;=;%
    //   node "%~dp0\.\node_modules\npm\bin\npm-cli.js" %*
    // )
    let cmd = nodePath ? `@SET NODE_PATH=${nodePath}\r\n` : '';
    if (longProg) {
        cmd += `@IF EXIST ${longProg} (\r\n` +
            `  ${longProg} ${args} ${target} ${progArgs}%*\r\n` +
            ') ELSE (\r\n' +
            '  @SETLOCAL\r\n' +
            '  @SET PATHEXT=%PATHEXT:;.JS;=;%\r\n' +
            `  ${prog} ${args} ${target} ${progArgs}%*\r\n` +
            ')';
    }
    else {
        cmd += `@${prog} ${args} ${target} ${progArgs}%*\r\n`;
    }
    return cmd;
}
/**
 * Generate the content of a shim for (Ba)sh in, for example, Cygwin and MSYS(2).
 *
 * @param src Path to the executable or script.
 * @param to Path to the shim to be created.
 * It is highly recommended to end with `.sh` or to contain no extension.
 * @param opts Options.
 * @return The content of shim.
 */
function generateShShim(src, to, opts) {
    let shTarget = path.relative(path.dirname(to), src);
    let shProg = opts.prog && opts.prog.split('\\').join('/');
    let shLongProg;
    shTarget = shTarget.split('\\').join('/');
    const quotedPathToTarget = path.isAbsolute(shTarget) ? `"${shTarget}"` : `"$basedir/${shTarget}"`;
    let args = opts.args || '';
    const shNodePath = normalizePathEnvVar(opts.nodePath).posix;
    if (!shProg) {
        shProg = quotedPathToTarget;
        args = '';
        shTarget = '';
    }
    else {
        shLongProg = `"$basedir/${opts.prog}"`;
        shTarget = quotedPathToTarget;
    }
    let progArgs = opts.progArgs ? `${opts.progArgs.join(` `)} ` : '';
    // #!/bin/sh
    // basedir=`dirname "$0"`
    //
    // case `uname` in
    //     *CYGWIN*) basedir=`cygpath -w "$basedir"`;;
    // esac
    //
    // export NODE_PATH="<nodepath>"
    //
    // if [ -x "$basedir/node.exe" ]; then
    //   exec "$basedir/node.exe" "$basedir/node_modules/npm/bin/npm-cli.js" "$@"
    // else
    //   exec node "$basedir/node_modules/npm/bin/npm-cli.js" "$@"
    // fi
    let sh = '#!/bin/sh\n';
    sh = sh +
        "basedir=$(dirname \"$(echo \"$0\" | sed -e 's,\\\\,/,g')\")\n" +
        '\n' +
        'case `uname` in\n' +
        '    *CYGWIN*) basedir=`cygpath -w "$basedir"`;;\n' +
        'esac\n' +
        '\n';
    const env = opts.nodePath ? `export NODE_PATH="${shNodePath}"\n` : '';
    if (shLongProg) {
        sh += env +
            `if [ -x ${shLongProg} ]; then\n` +
            `  exec ${shLongProg} ${args} ${shTarget} ${progArgs}"$@"\n` +
            'else \n' +
            `  exec ${shProg} ${args} ${shTarget} ${progArgs}"$@"\n` +
            'fi\n';
    }
    else {
        sh += `${env}${shProg} ${args} ${shTarget} ${progArgs}"$@"\n` +
            'exit $?\n';
    }
    return sh;
}
/**
 * Generate the content of a shim for PowerShell.
 *
 * @param src Path to the executable or script.
 * @param to Path to the shim to be created.
 * It is highly recommended to end with `.ps1`.
 * @param opts Options.
 * @return The content of shim.
 */
function generatePwshShim(src, to, opts) {
    let shTarget = path.relative(path.dirname(to), src);
    const shProg = opts.prog && opts.prog.split('\\').join('/');
    let pwshProg = shProg && `"${shProg}$exe"`;
    let pwshLongProg;
    shTarget = shTarget.split('\\').join('/');
    const quotedPathToTarget = path.isAbsolute(shTarget) ? `"${shTarget}"` : `"$basedir/${shTarget}"`;
    let args = opts.args || '';
    let normalizedPathEnvVar = normalizePathEnvVar(opts.nodePath);
    const nodePath = normalizedPathEnvVar.win32;
    const shNodePath = normalizedPathEnvVar.posix;
    if (!pwshProg) {
        pwshProg = quotedPathToTarget;
        args = '';
        shTarget = '';
    }
    else {
        pwshLongProg = `"$basedir/${opts.prog}$exe"`;
        shTarget = quotedPathToTarget;
    }
    let progArgs = opts.progArgs ? `${opts.progArgs.join(` `)} ` : '';
    // #!/usr/bin/env pwsh
    // $basedir=Split-Path $MyInvocation.MyCommand.Definition -Parent
    //
    // $ret=0
    // $exe = ""
    // if ($PSVersionTable.PSVersion -lt "6.0" -or $IsWindows) {
    //   # Fix case when both the Windows and Linux builds of Node
    //   # are installed in the same directory
    //   $exe = ".exe"
    // }
    // if (Test-Path "$basedir/node") {
    //   # Support pipeline input
    //   if ($MyInvocation.ExpectingInput) {
    //     $input | & "$basedir/node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
    //   } else {
    //     & "$basedir/node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
    //   }
    //   $ret=$LASTEXITCODE
    // } else {
    //   # Support pipeline input
    //   if ($MyInvocation.ExpectingInput) {
    //     $input | & "node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
    //   } else {
    //     & "node$exe" "$basedir/node_modules/npm/bin/npm-cli.js" $args
    //   }
    //   $ret=$LASTEXITCODE
    // }
    // exit $ret
    let pwsh = '#!/usr/bin/env pwsh\n' +
        '$basedir=Split-Path $MyInvocation.MyCommand.Definition -Parent\n' +
        '\n' +
        '$exe=""\n' +
        (opts.nodePath ? '$env_node_path=$env:NODE_PATH\n' +
            `$env:NODE_PATH="${nodePath}"\n` : '') +
        'if ($PSVersionTable.PSVersion -lt "6.0" -or $IsWindows) {\n' +
        '  # Fix case when both the Windows and Linux builds of Node\n' +
        '  # are installed in the same directory\n' +
        '  $exe=".exe"\n' +
        '}';
    if (opts.nodePath) {
        pwsh = pwsh +
            ' else {\n' +
            `  $env:NODE_PATH="${shNodePath}"\n` +
            '}';
    }
    pwsh += '\n';
    if (pwshLongProg) {
        pwsh = pwsh +
            '$ret=0\n' +
            `if (Test-Path ${pwshLongProg}) {\n` +
            '  # Support pipeline input\n' +
            '  if ($MyInvocation.ExpectingInput) {\n' +
            `    $input | & ${pwshLongProg} ${args} ${shTarget} ${progArgs}$args\n` +
            '  } else {\n' +
            `    & ${pwshLongProg} ${args} ${shTarget} ${progArgs}$args\n` +
            '  }\n' +
            '  $ret=$LASTEXITCODE\n' +
            '} else {\n' +
            '  # Support pipeline input\n' +
            '  if ($MyInvocation.ExpectingInput) {\n' +
            `    $input | & ${pwshProg} ${args} ${shTarget} ${progArgs}$args\n` +
            '  } else {\n' +
            `    & ${pwshProg} ${args} ${shTarget} ${progArgs}$args\n` +
            '  }\n' +
            '  $ret=$LASTEXITCODE\n' +
            '}\n' +
            (opts.nodePath ? '$env:NODE_PATH=$env_node_path\n' : '') +
            'exit $ret\n';
    }
    else {
        pwsh = pwsh +
            '# Support pipeline input\n' +
            'if ($MyInvocation.ExpectingInput) {\n' +
            `  $input | & ${pwshProg} ${args} ${shTarget} ${progArgs}$args\n` +
            '} else {\n' +
            `  & ${pwshProg} ${args} ${shTarget} ${progArgs}$args\n` +
            '}\n' +
            (opts.nodePath ? '$env:NODE_PATH=$env_node_path\n' : '') +
            'exit $LASTEXITCODE\n';
    }
    return pwsh;
}
/**
 * Chmod just created shim and make it executable
 *
 * @param to Path to shim.
 */
function chmodShim(to, opts) {
    return opts.fs_.chmod(to, 0o755);
}
function normalizePathEnvVar(nodePath) {
    if (!nodePath) {
        return {
            win32: '',
            posix: ''
        };
    }
    let split = (typeof nodePath === 'string' ? nodePath.split(path.delimiter) : Array.from(nodePath));
    let result = {};
    for (let i = 0; i < split.length; i++) {
        const win32 = split[i].split('/').join('\\');
        const posix = isWindows() ? split[i].split('\\').join('/').replace(/^([^:\\/]*):/, (_, $1) => `/mnt/${$1.toLowerCase()}`) : split[i];
        result.win32 = result.win32 ? `${result.win32};${win32}` : win32;
        result.posix = result.posix ? `${result.posix}:${posix}` : posix;
        result[i] = { win32, posix };
    }
    return result;
}
module.exports = cmdShim;
//# sourceMappingURL=index.js.map

/***/ }),

/***/ "./.yarn/cache/chownr-npm-1.1.4-5bd400ab08-115648f8eb.zip/node_modules/chownr/chownr.js":
/*!**********************************************************************************************!*\
  !*** ./.yarn/cache/chownr-npm-1.1.4-5bd400ab08-115648f8eb.zip/node_modules/chownr/chownr.js ***!
  \**********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

const fs = __webpack_require__(/*! fs */ "fs")
const path = __webpack_require__(/*! path */ "path")

/* istanbul ignore next */
const LCHOWN = fs.lchown ? 'lchown' : 'chown'
/* istanbul ignore next */
const LCHOWNSYNC = fs.lchownSync ? 'lchownSync' : 'chownSync'

/* istanbul ignore next */
const needEISDIRHandled = fs.lchown &&
  !process.version.match(/v1[1-9]+\./) &&
  !process.version.match(/v10\.[6-9]/)

const lchownSync = (path, uid, gid) => {
  try {
    return fs[LCHOWNSYNC](path, uid, gid)
  } catch (er) {
    if (er.code !== 'ENOENT')
      throw er
  }
}

/* istanbul ignore next */
const chownSync = (path, uid, gid) => {
  try {
    return fs.chownSync(path, uid, gid)
  } catch (er) {
    if (er.code !== 'ENOENT')
      throw er
  }
}

/* istanbul ignore next */
const handleEISDIR =
  needEISDIRHandled ? (path, uid, gid, cb) => er => {
    // Node prior to v10 had a very questionable implementation of
    // fs.lchown, which would always try to call fs.open on a directory
    // Fall back to fs.chown in those cases.
    if (!er || er.code !== 'EISDIR')
      cb(er)
    else
      fs.chown(path, uid, gid, cb)
  }
  : (_, __, ___, cb) => cb

/* istanbul ignore next */
const handleEISDirSync =
  needEISDIRHandled ? (path, uid, gid) => {
    try {
      return lchownSync(path, uid, gid)
    } catch (er) {
      if (er.code !== 'EISDIR')
        throw er
      chownSync(path, uid, gid)
    }
  }
  : (path, uid, gid) => lchownSync(path, uid, gid)

// fs.readdir could only accept an options object as of node v6
const nodeVersion = process.version
let readdir = (path, options, cb) => fs.readdir(path, options, cb)
let readdirSync = (path, options) => fs.readdirSync(path, options)
/* istanbul ignore next */
if (/^v4\./.test(nodeVersion))
  readdir = (path, options, cb) => fs.readdir(path, cb)

const chown = (cpath, uid, gid, cb) => {
  fs[LCHOWN](cpath, uid, gid, handleEISDIR(cpath, uid, gid, er => {
    // Skip ENOENT error
    cb(er && er.code !== 'ENOENT' ? er : null)
  }))
}

const chownrKid = (p, child, uid, gid, cb) => {
  if (typeof child === 'string')
    return fs.lstat(path.resolve(p, child), (er, stats) => {
      // Skip ENOENT error
      if (er)
        return cb(er.code !== 'ENOENT' ? er : null)
      stats.name = child
      chownrKid(p, stats, uid, gid, cb)
    })

  if (child.isDirectory()) {
    chownr(path.resolve(p, child.name), uid, gid, er => {
      if (er)
        return cb(er)
      const cpath = path.resolve(p, child.name)
      chown(cpath, uid, gid, cb)
    })
  } else {
    const cpath = path.resolve(p, child.name)
    chown(cpath, uid, gid, cb)
  }
}


const chownr = (p, uid, gid, cb) => {
  readdir(p, { withFileTypes: true }, (er, children) => {
    // any error other than ENOTDIR or ENOTSUP means it's not readable,
    // or doesn't exist.  give up.
    if (er) {
      if (er.code === 'ENOENT')
        return cb()
      else if (er.code !== 'ENOTDIR' && er.code !== 'ENOTSUP')
        return cb(er)
    }
    if (er || !children.length)
      return chown(p, uid, gid, cb)

    let len = children.length
    let errState = null
    const then = er => {
      if (errState)
        return
      if (er)
        return cb(errState = er)
      if (-- len === 0)
        return chown(p, uid, gid, cb)
    }

    children.forEach(child => chownrKid(p, child, uid, gid, then))
  })
}

const chownrKidSync = (p, child, uid, gid) => {
  if (typeof child === 'string') {
    try {
      const stats = fs.lstatSync(path.resolve(p, child))
      stats.name = child
      child = stats
    } catch (er) {
      if (er.code === 'ENOENT')
        return
      else
        throw er
    }
  }

  if (child.isDirectory())
    chownrSync(path.resolve(p, child.name), uid, gid)

  handleEISDirSync(path.resolve(p, child.name), uid, gid)
}

const chownrSync = (p, uid, gid) => {
  let children
  try {
    children = readdirSync(p, { withFileTypes: true })
  } catch (er) {
    if (er.code === 'ENOENT')
      return
    else if (er.code === 'ENOTDIR' || er.code === 'ENOTSUP')
      return handleEISDirSync(p, uid, gid)
    else
      throw er
  }

  if (children && children.length)
    children.forEach(child => chownrKidSync(p, child, uid, gid))

  return handleEISDirSync(p, uid, gid)
}

module.exports = chownr
chownr.sync = chownrSync


/***/ }),

/***/ "./.yarn/cache/fs-minipass-npm-2.1.0-501ef87306-1b8d128dae.zip/node_modules/fs-minipass/index.js":
/*!*******************************************************************************************************!*\
  !*** ./.yarn/cache/fs-minipass-npm-2.1.0-501ef87306-1b8d128dae.zip/node_modules/fs-minipass/index.js ***!
  \*******************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";

const MiniPass = __webpack_require__(/*! minipass */ "./.yarn/cache/minipass-npm-3.1.1-b51d7e264d-cfcfd86adc.zip/node_modules/minipass/index.js")
const EE = __webpack_require__(/*! events */ "events").EventEmitter
const fs = __webpack_require__(/*! fs */ "fs")

let writev = fs.writev
/* istanbul ignore next */
if (!writev) {
  // This entire block can be removed if support for earlier than Node.js
  // 12.9.0 is not needed.
  const binding = process.binding('fs')
  const FSReqWrap = binding.FSReqWrap || binding.FSReqCallback

  writev = (fd, iovec, pos, cb) => {
    const done = (er, bw) => cb(er, bw, iovec)
    const req = new FSReqWrap()
    req.oncomplete = done
    binding.writeBuffers(fd, iovec, pos, req)
  }
}

const _autoClose = Symbol('_autoClose')
const _close = Symbol('_close')
const _ended = Symbol('_ended')
const _fd = Symbol('_fd')
const _finished = Symbol('_finished')
const _flags = Symbol('_flags')
const _flush = Symbol('_flush')
const _handleChunk = Symbol('_handleChunk')
const _makeBuf = Symbol('_makeBuf')
const _mode = Symbol('_mode')
const _needDrain = Symbol('_needDrain')
const _onerror = Symbol('_onerror')
const _onopen = Symbol('_onopen')
const _onread = Symbol('_onread')
const _onwrite = Symbol('_onwrite')
const _open = Symbol('_open')
const _path = Symbol('_path')
const _pos = Symbol('_pos')
const _queue = Symbol('_queue')
const _read = Symbol('_read')
const _readSize = Symbol('_readSize')
const _reading = Symbol('_reading')
const _remain = Symbol('_remain')
const _size = Symbol('_size')
const _write = Symbol('_write')
const _writing = Symbol('_writing')
const _defaultFlag = Symbol('_defaultFlag')
const _errored = Symbol('_errored')

class ReadStream extends MiniPass {
  constructor (path, opt) {
    opt = opt || {}
    super(opt)

    this.readable = true
    this.writable = false

    if (typeof path !== 'string')
      throw new TypeError('path must be a string')

    this[_errored] = false
    this[_fd] = typeof opt.fd === 'number' ? opt.fd : null
    this[_path] = path
    this[_readSize] = opt.readSize || 16*1024*1024
    this[_reading] = false
    this[_size] = typeof opt.size === 'number' ? opt.size : Infinity
    this[_remain] = this[_size]
    this[_autoClose] = typeof opt.autoClose === 'boolean' ?
      opt.autoClose : true

    if (typeof this[_fd] === 'number')
      this[_read]()
    else
      this[_open]()
  }

  get fd () { return this[_fd] }
  get path () { return this[_path] }

  write () {
    throw new TypeError('this is a readable stream')
  }

  end () {
    throw new TypeError('this is a readable stream')
  }

  [_open] () {
    fs.open(this[_path], 'r', (er, fd) => this[_onopen](er, fd))
  }

  [_onopen] (er, fd) {
    if (er)
      this[_onerror](er)
    else {
      this[_fd] = fd
      this.emit('open', fd)
      this[_read]()
    }
  }

  [_makeBuf] () {
    return Buffer.allocUnsafe(Math.min(this[_readSize], this[_remain]))
  }

  [_read] () {
    if (!this[_reading]) {
      this[_reading] = true
      const buf = this[_makeBuf]()
      /* istanbul ignore if */
      if (buf.length === 0)
        return process.nextTick(() => this[_onread](null, 0, buf))
      fs.read(this[_fd], buf, 0, buf.length, null, (er, br, buf) =>
        this[_onread](er, br, buf))
    }
  }

  [_onread] (er, br, buf) {
    this[_reading] = false
    if (er)
      this[_onerror](er)
    else if (this[_handleChunk](br, buf))
      this[_read]()
  }

  [_close] () {
    if (this[_autoClose] && typeof this[_fd] === 'number') {
      const fd = this[_fd]
      this[_fd] = null
      fs.close(fd, er => er ? this.emit('error', er) : this.emit('close'))
    }
  }

  [_onerror] (er) {
    this[_reading] = true
    this[_close]()
    this.emit('error', er)
  }

  [_handleChunk] (br, buf) {
    let ret = false
    // no effect if infinite
    this[_remain] -= br
    if (br > 0)
      ret = super.write(br < buf.length ? buf.slice(0, br) : buf)

    if (br === 0 || this[_remain] <= 0) {
      ret = false
      this[_close]()
      super.end()
    }

    return ret
  }

  emit (ev, data) {
    switch (ev) {
      case 'prefinish':
      case 'finish':
        break

      case 'drain':
        if (typeof this[_fd] === 'number')
          this[_read]()
        break

      case 'error':
        if (this[_errored])
          return
        this[_errored] = true
        return super.emit(ev, data)

      default:
        return super.emit(ev, data)
    }
  }
}

class ReadStreamSync extends ReadStream {
  [_open] () {
    let threw = true
    try {
      this[_onopen](null, fs.openSync(this[_path], 'r'))
      threw = false
    } finally {
      if (threw)
        this[_close]()
    }
  }

  [_read] () {
    let threw = true
    try {
      if (!this[_reading]) {
        this[_reading] = true
        do {
          const buf = this[_makeBuf]()
          /* istanbul ignore next */
          const br = buf.length === 0 ? 0
            : fs.readSync(this[_fd], buf, 0, buf.length, null)
          if (!this[_handleChunk](br, buf))
            break
        } while (true)
        this[_reading] = false
      }
      threw = false
    } finally {
      if (threw)
        this[_close]()
    }
  }

  [_close] () {
    if (this[_autoClose] && typeof this[_fd] === 'number') {
      const fd = this[_fd]
      this[_fd] = null
      fs.closeSync(fd)
      this.emit('close')
    }
  }
}

class WriteStream extends EE {
  constructor (path, opt) {
    opt = opt || {}
    super(opt)
    this.readable = false
    this.writable = true
    this[_errored] = false
    this[_writing] = false
    this[_ended] = false
    this[_needDrain] = false
    this[_queue] = []
    this[_path] = path
    this[_fd] = typeof opt.fd === 'number' ? opt.fd : null
    this[_mode] = opt.mode === undefined ? 0o666 : opt.mode
    this[_pos] = typeof opt.start === 'number' ? opt.start : null
    this[_autoClose] = typeof opt.autoClose === 'boolean' ?
      opt.autoClose : true

    // truncating makes no sense when writing into the middle
    const defaultFlag = this[_pos] !== null ? 'r+' : 'w'
    this[_defaultFlag] = opt.flags === undefined
    this[_flags] = this[_defaultFlag] ? defaultFlag : opt.flags

    if (this[_fd] === null)
      this[_open]()
  }

  emit (ev, data) {
    if (ev === 'error') {
      if (this[_errored])
        return
      this[_errored] = true
    }
    return super.emit(ev, data)
  }


  get fd () { return this[_fd] }
  get path () { return this[_path] }

  [_onerror] (er) {
    this[_close]()
    this[_writing] = true
    this.emit('error', er)
  }

  [_open] () {
    fs.open(this[_path], this[_flags], this[_mode],
      (er, fd) => this[_onopen](er, fd))
  }

  [_onopen] (er, fd) {
    if (this[_defaultFlag] &&
        this[_flags] === 'r+' &&
        er && er.code === 'ENOENT') {
      this[_flags] = 'w'
      this[_open]()
    } else if (er)
      this[_onerror](er)
    else {
      this[_fd] = fd
      this.emit('open', fd)
      this[_flush]()
    }
  }

  end (buf, enc) {
    if (buf)
      this.write(buf, enc)

    this[_ended] = true

    // synthetic after-write logic, where drain/finish live
    if (!this[_writing] && !this[_queue].length &&
        typeof this[_fd] === 'number')
      this[_onwrite](null, 0)
    return this
  }

  write (buf, enc) {
    if (typeof buf === 'string')
      buf = Buffer.from(buf, enc)

    if (this[_ended]) {
      this.emit('error', new Error('write() after end()'))
      return false
    }

    if (this[_fd] === null || this[_writing] || this[_queue].length) {
      this[_queue].push(buf)
      this[_needDrain] = true
      return false
    }

    this[_writing] = true
    this[_write](buf)
    return true
  }

  [_write] (buf) {
    fs.write(this[_fd], buf, 0, buf.length, this[_pos], (er, bw) =>
      this[_onwrite](er, bw))
  }

  [_onwrite] (er, bw) {
    if (er)
      this[_onerror](er)
    else {
      if (this[_pos] !== null)
        this[_pos] += bw
      if (this[_queue].length)
        this[_flush]()
      else {
        this[_writing] = false

        if (this[_ended] && !this[_finished]) {
          this[_finished] = true
          this[_close]()
          this.emit('finish')
        } else if (this[_needDrain]) {
          this[_needDrain] = false
          this.emit('drain')
        }
      }
    }
  }

  [_flush] () {
    if (this[_queue].length === 0) {
      if (this[_ended])
        this[_onwrite](null, 0)
    } else if (this[_queue].length === 1)
      this[_write](this[_queue].pop())
    else {
      const iovec = this[_queue]
      this[_queue] = []
      writev(this[_fd], iovec, this[_pos],
        (er, bw) => this[_onwrite](er, bw))
    }
  }

  [_close] () {
    if (this[_autoClose] && typeof this[_fd] === 'number') {
      const fd = this[_fd]
      this[_fd] = null
      fs.close(fd, er => er ? this.emit('error', er) : this.emit('close'))
    }
  }
}

class WriteStreamSync extends WriteStream {
  [_open] () {
    let fd
    // only wrap in a try{} block if we know we'll retry, to avoid
    // the rethrow obscuring the error's source frame in most cases.
    if (this[_defaultFlag] && this[_flags] === 'r+') {
      try {
        fd = fs.openSync(this[_path], this[_flags], this[_mode])
      } catch (er) {
        if (er.code === 'ENOENT') {
          this[_flags] = 'w'
          return this[_open]()
        } else
          throw er
      }
    } else
      fd = fs.openSync(this[_path], this[_flags], this[_mode])

    this[_onopen](null, fd)
  }

  [_close] () {
    if (this[_autoClose] && typeof this[_fd] === 'number') {
      const fd = this[_fd]
      this[_fd] = null
      fs.closeSync(fd)
      this.emit('close')
    }
  }

  [_write] (buf) {
    // throw the original, but try to close if it fails
    let threw = true
    try {
      this[_onwrite](null,
        fs.writeSync(this[_fd], buf, 0, buf.length, this[_pos]))
      threw = false
    } finally {
      if (threw)
        try { this[_close]() } catch (_) {}
    }
  }
}

exports.ReadStream = ReadStream
exports.ReadStreamSync = ReadStreamSync

exports.WriteStream = WriteStream
exports.WriteStreamSync = WriteStreamSync


/***/ }),

/***/ "./.yarn/cache/has-flag-npm-4.0.0-32af9f0536-261a135703.zip/node_modules/has-flag/index.js":
/*!*************************************************************************************************!*\
  !*** ./.yarn/cache/has-flag-npm-4.0.0-32af9f0536-261a135703.zip/node_modules/has-flag/index.js ***!
  \*************************************************************************************************/
/***/ ((module) => {

"use strict";


module.exports = (flag, argv = process.argv) => {
	const prefix = flag.startsWith('-') ? '' : (flag.length === 1 ? '-' : '--');
	const position = argv.indexOf(prefix + flag);
	const terminatorPosition = argv.indexOf('--');
	return position !== -1 && (terminatorPosition === -1 || position < terminatorPosition);
};


/***/ }),

/***/ "./.yarn/cache/is-windows-npm-1.0.2-898cd6f3d7-438b7e5265.zip/node_modules/is-windows/index.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/is-windows-npm-1.0.2-898cd6f3d7-438b7e5265.zip/node_modules/is-windows/index.js ***!
  \*****************************************************************************************************/
/***/ ((module, exports) => {

var __WEBPACK_AMD_DEFINE_FACTORY__, __WEBPACK_AMD_DEFINE_ARRAY__, __WEBPACK_AMD_DEFINE_RESULT__;/*!
 * is-windows <https://github.com/jonschlinkert/is-windows>
 *
 * Copyright © 2015-2018, Jon Schlinkert.
 * Released under the MIT License.
 */

(function(factory) {
  if (exports && typeof exports === 'object' && "object" !== 'undefined') {
    module.exports = factory();
  } else if (true) {
    !(__WEBPACK_AMD_DEFINE_ARRAY__ = [], __WEBPACK_AMD_DEFINE_FACTORY__ = (factory),
		__WEBPACK_AMD_DEFINE_RESULT__ = (typeof __WEBPACK_AMD_DEFINE_FACTORY__ === 'function' ?
		(__WEBPACK_AMD_DEFINE_FACTORY__.apply(exports, __WEBPACK_AMD_DEFINE_ARRAY__)) : __WEBPACK_AMD_DEFINE_FACTORY__),
		__WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
  } else {}
})(function() {
  'use strict';
  return function isWindows() {
    return process && (process.platform === 'win32' || /^(msys|cygwin)$/.test(process.env.OSTYPE));
  };
});


/***/ }),

/***/ "./.yarn/cache/isexe-npm-2.0.0-b58870bd2e-26bf6c5480.zip/node_modules/isexe/index.js":
/*!*******************************************************************************************!*\
  !*** ./.yarn/cache/isexe-npm-2.0.0-b58870bd2e-26bf6c5480.zip/node_modules/isexe/index.js ***!
  \*******************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var fs = __webpack_require__(/*! fs */ "fs")
var core
if (process.platform === 'win32' || global.TESTING_WINDOWS) {
  core = __webpack_require__(/*! ./windows.js */ "./.yarn/cache/isexe-npm-2.0.0-b58870bd2e-26bf6c5480.zip/node_modules/isexe/windows.js")
} else {
  core = __webpack_require__(/*! ./mode.js */ "./.yarn/cache/isexe-npm-2.0.0-b58870bd2e-26bf6c5480.zip/node_modules/isexe/mode.js")
}

module.exports = isexe
isexe.sync = sync

function isexe (path, options, cb) {
  if (typeof options === 'function') {
    cb = options
    options = {}
  }

  if (!cb) {
    if (typeof Promise !== 'function') {
      throw new TypeError('callback not provided')
    }

    return new Promise(function (resolve, reject) {
      isexe(path, options || {}, function (er, is) {
        if (er) {
          reject(er)
        } else {
          resolve(is)
        }
      })
    })
  }

  core(path, options || {}, function (er, is) {
    // ignore EACCES because that just means we aren't allowed to run it
    if (er) {
      if (er.code === 'EACCES' || options && options.ignoreErrors) {
        er = null
        is = false
      }
    }
    cb(er, is)
  })
}

function sync (path, options) {
  // my kingdom for a filtered catch
  try {
    return core.sync(path, options || {})
  } catch (er) {
    if (options && options.ignoreErrors || er.code === 'EACCES') {
      return false
    } else {
      throw er
    }
  }
}


/***/ }),

/***/ "./.yarn/cache/isexe-npm-2.0.0-b58870bd2e-26bf6c5480.zip/node_modules/isexe/mode.js":
/*!******************************************************************************************!*\
  !*** ./.yarn/cache/isexe-npm-2.0.0-b58870bd2e-26bf6c5480.zip/node_modules/isexe/mode.js ***!
  \******************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

module.exports = isexe
isexe.sync = sync

var fs = __webpack_require__(/*! fs */ "fs")

function isexe (path, options, cb) {
  fs.stat(path, function (er, stat) {
    cb(er, er ? false : checkStat(stat, options))
  })
}

function sync (path, options) {
  return checkStat(fs.statSync(path), options)
}

function checkStat (stat, options) {
  return stat.isFile() && checkMode(stat, options)
}

function checkMode (stat, options) {
  var mod = stat.mode
  var uid = stat.uid
  var gid = stat.gid

  var myUid = options.uid !== undefined ?
    options.uid : process.getuid && process.getuid()
  var myGid = options.gid !== undefined ?
    options.gid : process.getgid && process.getgid()

  var u = parseInt('100', 8)
  var g = parseInt('010', 8)
  var o = parseInt('001', 8)
  var ug = u | g

  var ret = (mod & o) ||
    (mod & g) && gid === myGid ||
    (mod & u) && uid === myUid ||
    (mod & ug) && myUid === 0

  return ret
}


/***/ }),

/***/ "./.yarn/cache/isexe-npm-2.0.0-b58870bd2e-26bf6c5480.zip/node_modules/isexe/windows.js":
/*!*********************************************************************************************!*\
  !*** ./.yarn/cache/isexe-npm-2.0.0-b58870bd2e-26bf6c5480.zip/node_modules/isexe/windows.js ***!
  \*********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

module.exports = isexe
isexe.sync = sync

var fs = __webpack_require__(/*! fs */ "fs")

function checkPathExt (path, options) {
  var pathext = options.pathExt !== undefined ?
    options.pathExt : process.env.PATHEXT

  if (!pathext) {
    return true
  }

  pathext = pathext.split(';')
  if (pathext.indexOf('') !== -1) {
    return true
  }
  for (var i = 0; i < pathext.length; i++) {
    var p = pathext[i].toLowerCase()
    if (p && path.substr(-p.length).toLowerCase() === p) {
      return true
    }
  }
  return false
}

function checkStat (stat, path, options) {
  if (!stat.isSymbolicLink() && !stat.isFile()) {
    return false
  }
  return checkPathExt(path, options)
}

function isexe (path, options, cb) {
  fs.stat(path, function (er, stat) {
    cb(er, er ? false : checkStat(stat, path, options))
  })
}

function sync (path, options) {
  return checkStat(fs.statSync(path), path, options)
}


/***/ }),

/***/ "./.yarn/cache/minipass-npm-3.1.1-b51d7e264d-cfcfd86adc.zip/node_modules/minipass/index.js":
/*!*************************************************************************************************!*\
  !*** ./.yarn/cache/minipass-npm-3.1.1-b51d7e264d-cfcfd86adc.zip/node_modules/minipass/index.js ***!
  \*************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

const EE = __webpack_require__(/*! events */ "events")
const Stream = __webpack_require__(/*! stream */ "stream")
const Yallist = __webpack_require__(/*! yallist */ "./.yarn/cache/yallist-npm-4.0.0-b493d9e907-343617202a.zip/node_modules/yallist/yallist.js")
const SD = __webpack_require__(/*! string_decoder */ "string_decoder").StringDecoder

const EOF = Symbol('EOF')
const MAYBE_EMIT_END = Symbol('maybeEmitEnd')
const EMITTED_END = Symbol('emittedEnd')
const EMITTING_END = Symbol('emittingEnd')
const CLOSED = Symbol('closed')
const READ = Symbol('read')
const FLUSH = Symbol('flush')
const FLUSHCHUNK = Symbol('flushChunk')
const ENCODING = Symbol('encoding')
const DECODER = Symbol('decoder')
const FLOWING = Symbol('flowing')
const PAUSED = Symbol('paused')
const RESUME = Symbol('resume')
const BUFFERLENGTH = Symbol('bufferLength')
const BUFFERPUSH = Symbol('bufferPush')
const BUFFERSHIFT = Symbol('bufferShift')
const OBJECTMODE = Symbol('objectMode')
const DESTROYED = Symbol('destroyed')

// TODO remove when Node v8 support drops
const doIter = global._MP_NO_ITERATOR_SYMBOLS_  !== '1'
const ASYNCITERATOR = doIter && Symbol.asyncIterator
  || Symbol('asyncIterator not implemented')
const ITERATOR = doIter && Symbol.iterator
  || Symbol('iterator not implemented')

// events that mean 'the stream is over'
// these are treated specially, and re-emitted
// if they are listened for after emitting.
const isEndish = ev =>
  ev === 'end' ||
  ev === 'finish' ||
  ev === 'prefinish'

const isArrayBuffer = b => b instanceof ArrayBuffer ||
  typeof b === 'object' &&
  b.constructor &&
  b.constructor.name === 'ArrayBuffer' &&
  b.byteLength >= 0

const isArrayBufferView = b => !Buffer.isBuffer(b) && ArrayBuffer.isView(b)

module.exports = class Minipass extends Stream {
  constructor (options) {
    super()
    this[FLOWING] = false
    // whether we're explicitly paused
    this[PAUSED] = false
    this.pipes = new Yallist()
    this.buffer = new Yallist()
    this[OBJECTMODE] = options && options.objectMode || false
    if (this[OBJECTMODE])
      this[ENCODING] = null
    else
      this[ENCODING] = options && options.encoding || null
    if (this[ENCODING] === 'buffer')
      this[ENCODING] = null
    this[DECODER] = this[ENCODING] ? new SD(this[ENCODING]) : null
    this[EOF] = false
    this[EMITTED_END] = false
    this[EMITTING_END] = false
    this[CLOSED] = false
    this.writable = true
    this.readable = true
    this[BUFFERLENGTH] = 0
    this[DESTROYED] = false
  }

  get bufferLength () { return this[BUFFERLENGTH] }

  get encoding () { return this[ENCODING] }
  set encoding (enc) {
    if (this[OBJECTMODE])
      throw new Error('cannot set encoding in objectMode')

    if (this[ENCODING] && enc !== this[ENCODING] &&
        (this[DECODER] && this[DECODER].lastNeed || this[BUFFERLENGTH]))
      throw new Error('cannot change encoding')

    if (this[ENCODING] !== enc) {
      this[DECODER] = enc ? new SD(enc) : null
      if (this.buffer.length)
        this.buffer = this.buffer.map(chunk => this[DECODER].write(chunk))
    }

    this[ENCODING] = enc
  }

  setEncoding (enc) {
    this.encoding = enc
  }

  get objectMode () { return this[OBJECTMODE] }
  set objectMode (ॐ ) { this[OBJECTMODE] = this[OBJECTMODE] || !!ॐ  }

  write (chunk, encoding, cb) {
    if (this[EOF])
      throw new Error('write after end')

    if (this[DESTROYED]) {
      this.emit('error', Object.assign(
        new Error('Cannot call write after a stream was destroyed'),
        { code: 'ERR_STREAM_DESTROYED' }
      ))
      return true
    }

    if (typeof encoding === 'function')
      cb = encoding, encoding = 'utf8'

    if (!encoding)
      encoding = 'utf8'

    // convert array buffers and typed array views into buffers
    // at some point in the future, we may want to do the opposite!
    // leave strings and buffers as-is
    // anything else switches us into object mode
    if (!this[OBJECTMODE] && !Buffer.isBuffer(chunk)) {
      if (isArrayBufferView(chunk))
        chunk = Buffer.from(chunk.buffer, chunk.byteOffset, chunk.byteLength)
      else if (isArrayBuffer(chunk))
        chunk = Buffer.from(chunk)
      else if (typeof chunk !== 'string')
        // use the setter so we throw if we have encoding set
        this.objectMode = true
    }

    // this ensures at this point that the chunk is a buffer or string
    // don't buffer it up or send it to the decoder
    if (!this.objectMode && !chunk.length) {
      const ret = this.flowing
      if (this[BUFFERLENGTH] !== 0)
        this.emit('readable')
      if (cb)
        cb()
      return ret
    }

    // fast-path writing strings of same encoding to a stream with
    // an empty buffer, skipping the buffer/decoder dance
    if (typeof chunk === 'string' && !this[OBJECTMODE] &&
        // unless it is a string already ready for us to use
        !(encoding === this[ENCODING] && !this[DECODER].lastNeed)) {
      chunk = Buffer.from(chunk, encoding)
    }

    if (Buffer.isBuffer(chunk) && this[ENCODING])
      chunk = this[DECODER].write(chunk)

    try {
      return this.flowing
        ? (this.emit('data', chunk), this.flowing)
        : (this[BUFFERPUSH](chunk), false)
    } finally {
      if (this[BUFFERLENGTH] !== 0)
        this.emit('readable')
      if (cb)
        cb()
    }
  }

  read (n) {
    if (this[DESTROYED])
      return null

    try {
      if (this[BUFFERLENGTH] === 0 || n === 0 || n > this[BUFFERLENGTH])
        return null

      if (this[OBJECTMODE])
        n = null

      if (this.buffer.length > 1 && !this[OBJECTMODE]) {
        if (this.encoding)
          this.buffer = new Yallist([
            Array.from(this.buffer).join('')
          ])
        else
          this.buffer = new Yallist([
            Buffer.concat(Array.from(this.buffer), this[BUFFERLENGTH])
          ])
      }

      return this[READ](n || null, this.buffer.head.value)
    } finally {
      this[MAYBE_EMIT_END]()
    }
  }

  [READ] (n, chunk) {
    if (n === chunk.length || n === null)
      this[BUFFERSHIFT]()
    else {
      this.buffer.head.value = chunk.slice(n)
      chunk = chunk.slice(0, n)
      this[BUFFERLENGTH] -= n
    }

    this.emit('data', chunk)

    if (!this.buffer.length && !this[EOF])
      this.emit('drain')

    return chunk
  }

  end (chunk, encoding, cb) {
    if (typeof chunk === 'function')
      cb = chunk, chunk = null
    if (typeof encoding === 'function')
      cb = encoding, encoding = 'utf8'
    if (chunk)
      this.write(chunk, encoding)
    if (cb)
      this.once('end', cb)
    this[EOF] = true
    this.writable = false

    // if we haven't written anything, then go ahead and emit,
    // even if we're not reading.
    // we'll re-emit if a new 'end' listener is added anyway.
    // This makes MP more suitable to write-only use cases.
    if (this.flowing || !this[PAUSED])
      this[MAYBE_EMIT_END]()
    return this
  }

  // don't let the internal resume be overwritten
  [RESUME] () {
    if (this[DESTROYED])
      return

    this[PAUSED] = false
    this[FLOWING] = true
    this.emit('resume')
    if (this.buffer.length)
      this[FLUSH]()
    else if (this[EOF])
      this[MAYBE_EMIT_END]()
    else
      this.emit('drain')
  }

  resume () {
    return this[RESUME]()
  }

  pause () {
    this[FLOWING] = false
    this[PAUSED] = true
  }

  get destroyed () {
    return this[DESTROYED]
  }

  get flowing () {
    return this[FLOWING]
  }

  get paused () {
    return this[PAUSED]
  }

  [BUFFERPUSH] (chunk) {
    if (this[OBJECTMODE])
      this[BUFFERLENGTH] += 1
    else
      this[BUFFERLENGTH] += chunk.length
    return this.buffer.push(chunk)
  }

  [BUFFERSHIFT] () {
    if (this.buffer.length) {
      if (this[OBJECTMODE])
        this[BUFFERLENGTH] -= 1
      else
        this[BUFFERLENGTH] -= this.buffer.head.value.length
    }
    return this.buffer.shift()
  }

  [FLUSH] () {
    do {} while (this[FLUSHCHUNK](this[BUFFERSHIFT]()))

    if (!this.buffer.length && !this[EOF])
      this.emit('drain')
  }

  [FLUSHCHUNK] (chunk) {
    return chunk ? (this.emit('data', chunk), this.flowing) : false
  }

  pipe (dest, opts) {
    if (this[DESTROYED])
      return

    const ended = this[EMITTED_END]
    opts = opts || {}
    if (dest === process.stdout || dest === process.stderr)
      opts.end = false
    else
      opts.end = opts.end !== false

    const p = { dest: dest, opts: opts, ondrain: _ => this[RESUME]() }
    this.pipes.push(p)

    dest.on('drain', p.ondrain)
    this[RESUME]()
    // piping an ended stream ends immediately
    if (ended && p.opts.end)
      p.dest.end()
    return dest
  }

  addListener (ev, fn) {
    return this.on(ev, fn)
  }

  on (ev, fn) {
    try {
      return super.on(ev, fn)
    } finally {
      if (ev === 'data' && !this.pipes.length && !this.flowing)
        this[RESUME]()
      else if (isEndish(ev) && this[EMITTED_END]) {
        super.emit(ev)
        this.removeAllListeners(ev)
      }
    }
  }

  get emittedEnd () {
    return this[EMITTED_END]
  }

  [MAYBE_EMIT_END] () {
    if (!this[EMITTING_END] &&
        !this[EMITTED_END] &&
        !this[DESTROYED] &&
        this.buffer.length === 0 &&
        this[EOF]) {
      this[EMITTING_END] = true
      this.emit('end')
      this.emit('prefinish')
      this.emit('finish')
      if (this[CLOSED])
        this.emit('close')
      this[EMITTING_END] = false
    }
  }

  emit (ev, data) {
    // error and close are only events allowed after calling destroy()
    if (ev !== 'error' && ev !== 'close' && ev !== DESTROYED && this[DESTROYED])
      return
    else if (ev === 'data') {
      if (!data)
        return

      if (this.pipes.length)
        this.pipes.forEach(p =>
          p.dest.write(data) === false && this.pause())
    } else if (ev === 'end') {
      // only actual end gets this treatment
      if (this[EMITTED_END] === true)
        return

      this[EMITTED_END] = true
      this.readable = false

      if (this[DECODER]) {
        data = this[DECODER].end()
        if (data) {
          this.pipes.forEach(p => p.dest.write(data))
          super.emit('data', data)
        }
      }

      this.pipes.forEach(p => {
        p.dest.removeListener('drain', p.ondrain)
        if (p.opts.end)
          p.dest.end()
      })
    } else if (ev === 'close') {
      this[CLOSED] = true
      // don't emit close before 'end' and 'finish'
      if (!this[EMITTED_END] && !this[DESTROYED])
        return
    }

    // TODO: replace with a spread operator when Node v4 support drops
    const args = new Array(arguments.length)
    args[0] = ev
    args[1] = data
    if (arguments.length > 2) {
      for (let i = 2; i < arguments.length; i++) {
        args[i] = arguments[i]
      }
    }

    try {
      return super.emit.apply(this, args)
    } finally {
      if (!isEndish(ev))
        this[MAYBE_EMIT_END]()
      else
        this.removeAllListeners(ev)
    }
  }

  // const all = await stream.collect()
  collect () {
    const buf = []
    if (!this[OBJECTMODE])
      buf.dataLength = 0
    // set the promise first, in case an error is raised
    // by triggering the flow here.
    const p = this.promise()
    this.on('data', c => {
      buf.push(c)
      if (!this[OBJECTMODE])
        buf.dataLength += c.length
    })
    return p.then(() => buf)
  }

  // const data = await stream.concat()
  concat () {
    return this[OBJECTMODE]
      ? Promise.reject(new Error('cannot concat in objectMode'))
      : this.collect().then(buf =>
          this[OBJECTMODE]
            ? Promise.reject(new Error('cannot concat in objectMode'))
            : this[ENCODING] ? buf.join('') : Buffer.concat(buf, buf.dataLength))
  }

  // stream.promise().then(() => done, er => emitted error)
  promise () {
    return new Promise((resolve, reject) => {
      this.on(DESTROYED, () => reject(new Error('stream destroyed')))
      this.on('end', () => resolve())
      this.on('error', er => reject(er))
    })
  }

  // for await (let chunk of stream)
  [ASYNCITERATOR] () {
    const next = () => {
      const res = this.read()
      if (res !== null)
        return Promise.resolve({ done: false, value: res })

      if (this[EOF])
        return Promise.resolve({ done: true })

      let resolve = null
      let reject = null
      const onerr = er => {
        this.removeListener('data', ondata)
        this.removeListener('end', onend)
        reject(er)
      }
      const ondata = value => {
        this.removeListener('error', onerr)
        this.removeListener('end', onend)
        this.pause()
        resolve({ value: value, done: !!this[EOF] })
      }
      const onend = () => {
        this.removeListener('error', onerr)
        this.removeListener('data', ondata)
        resolve({ done: true })
      }
      const ondestroy = () => onerr(new Error('stream destroyed'))
      return new Promise((res, rej) => {
        reject = rej
        resolve = res
        this.once(DESTROYED, ondestroy)
        this.once('error', onerr)
        this.once('end', onend)
        this.once('data', ondata)
      })
    }

    return { next }
  }

  // for (let chunk of stream)
  [ITERATOR] () {
    const next = () => {
      const value = this.read()
      const done = value === null
      return { value, done }
    }
    return { next }
  }

  destroy (er) {
    if (this[DESTROYED]) {
      if (er)
        this.emit('error', er)
      else
        this.emit(DESTROYED)
      return this
    }

    this[DESTROYED] = true

    // throw away all buffered data, it's never coming out
    this.buffer = new Yallist()
    this[BUFFERLENGTH] = 0

    if (typeof this.close === 'function' && !this[CLOSED])
      this.close()

    if (er)
      this.emit('error', er)
    else // if no error to emit, still reject pending promises
      this.emit(DESTROYED)

    return this
  }

  static isStream (s) {
    return !!s && (s instanceof Minipass || s instanceof Stream ||
      s instanceof EE && (
        typeof s.pipe === 'function' || // readable
        (typeof s.write === 'function' && typeof s.end === 'function') // writable
      ))
  }
}


/***/ }),

/***/ "./.yarn/cache/minizlib-npm-2.1.0-9531be52b3-6a811aaa3a.zip/node_modules/minizlib/constants.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/minizlib-npm-2.1.0-9531be52b3-6a811aaa3a.zip/node_modules/minizlib/constants.js ***!
  \*****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

// Update with any zlib constants that are added or changed in the future.
// Node v6 didn't export this, so we just hard code the version and rely
// on all the other hard-coded values from zlib v4736.  When node v6
// support drops, we can just export the realZlibConstants object.
const realZlibConstants = __webpack_require__(/*! zlib */ "zlib").constants ||
  /* istanbul ignore next */ { ZLIB_VERNUM: 4736 }

module.exports = Object.freeze(Object.assign(Object.create(null), {
  Z_NO_FLUSH: 0,
  Z_PARTIAL_FLUSH: 1,
  Z_SYNC_FLUSH: 2,
  Z_FULL_FLUSH: 3,
  Z_FINISH: 4,
  Z_BLOCK: 5,
  Z_OK: 0,
  Z_STREAM_END: 1,
  Z_NEED_DICT: 2,
  Z_ERRNO: -1,
  Z_STREAM_ERROR: -2,
  Z_DATA_ERROR: -3,
  Z_MEM_ERROR: -4,
  Z_BUF_ERROR: -5,
  Z_VERSION_ERROR: -6,
  Z_NO_COMPRESSION: 0,
  Z_BEST_SPEED: 1,
  Z_BEST_COMPRESSION: 9,
  Z_DEFAULT_COMPRESSION: -1,
  Z_FILTERED: 1,
  Z_HUFFMAN_ONLY: 2,
  Z_RLE: 3,
  Z_FIXED: 4,
  Z_DEFAULT_STRATEGY: 0,
  DEFLATE: 1,
  INFLATE: 2,
  GZIP: 3,
  GUNZIP: 4,
  DEFLATERAW: 5,
  INFLATERAW: 6,
  UNZIP: 7,
  BROTLI_DECODE: 8,
  BROTLI_ENCODE: 9,
  Z_MIN_WINDOWBITS: 8,
  Z_MAX_WINDOWBITS: 15,
  Z_DEFAULT_WINDOWBITS: 15,
  Z_MIN_CHUNK: 64,
  Z_MAX_CHUNK: Infinity,
  Z_DEFAULT_CHUNK: 16384,
  Z_MIN_MEMLEVEL: 1,
  Z_MAX_MEMLEVEL: 9,
  Z_DEFAULT_MEMLEVEL: 8,
  Z_MIN_LEVEL: -1,
  Z_MAX_LEVEL: 9,
  Z_DEFAULT_LEVEL: -1,
  BROTLI_OPERATION_PROCESS: 0,
  BROTLI_OPERATION_FLUSH: 1,
  BROTLI_OPERATION_FINISH: 2,
  BROTLI_OPERATION_EMIT_METADATA: 3,
  BROTLI_MODE_GENERIC: 0,
  BROTLI_MODE_TEXT: 1,
  BROTLI_MODE_FONT: 2,
  BROTLI_DEFAULT_MODE: 0,
  BROTLI_MIN_QUALITY: 0,
  BROTLI_MAX_QUALITY: 11,
  BROTLI_DEFAULT_QUALITY: 11,
  BROTLI_MIN_WINDOW_BITS: 10,
  BROTLI_MAX_WINDOW_BITS: 24,
  BROTLI_LARGE_MAX_WINDOW_BITS: 30,
  BROTLI_DEFAULT_WINDOW: 22,
  BROTLI_MIN_INPUT_BLOCK_BITS: 16,
  BROTLI_MAX_INPUT_BLOCK_BITS: 24,
  BROTLI_PARAM_MODE: 0,
  BROTLI_PARAM_QUALITY: 1,
  BROTLI_PARAM_LGWIN: 2,
  BROTLI_PARAM_LGBLOCK: 3,
  BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING: 4,
  BROTLI_PARAM_SIZE_HINT: 5,
  BROTLI_PARAM_LARGE_WINDOW: 6,
  BROTLI_PARAM_NPOSTFIX: 7,
  BROTLI_PARAM_NDIRECT: 8,
  BROTLI_DECODER_RESULT_ERROR: 0,
  BROTLI_DECODER_RESULT_SUCCESS: 1,
  BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT: 2,
  BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT: 3,
  BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION: 0,
  BROTLI_DECODER_PARAM_LARGE_WINDOW: 1,
  BROTLI_DECODER_NO_ERROR: 0,
  BROTLI_DECODER_SUCCESS: 1,
  BROTLI_DECODER_NEEDS_MORE_INPUT: 2,
  BROTLI_DECODER_NEEDS_MORE_OUTPUT: 3,
  BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE: -1,
  BROTLI_DECODER_ERROR_FORMAT_RESERVED: -2,
  BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE: -3,
  BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET: -4,
  BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME: -5,
  BROTLI_DECODER_ERROR_FORMAT_CL_SPACE: -6,
  BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE: -7,
  BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT: -8,
  BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1: -9,
  BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2: -10,
  BROTLI_DECODER_ERROR_FORMAT_TRANSFORM: -11,
  BROTLI_DECODER_ERROR_FORMAT_DICTIONARY: -12,
  BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS: -13,
  BROTLI_DECODER_ERROR_FORMAT_PADDING_1: -14,
  BROTLI_DECODER_ERROR_FORMAT_PADDING_2: -15,
  BROTLI_DECODER_ERROR_FORMAT_DISTANCE: -16,
  BROTLI_DECODER_ERROR_DICTIONARY_NOT_SET: -19,
  BROTLI_DECODER_ERROR_INVALID_ARGUMENTS: -20,
  BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODES: -21,
  BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS: -22,
  BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MAP: -25,
  BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_1: -26,
  BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2: -27,
  BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES: -30,
  BROTLI_DECODER_ERROR_UNREACHABLE: -31,
}, realZlibConstants))


/***/ }),

/***/ "./.yarn/cache/minizlib-npm-2.1.0-9531be52b3-6a811aaa3a.zip/node_modules/minizlib/index.js":
/*!*************************************************************************************************!*\
  !*** ./.yarn/cache/minizlib-npm-2.1.0-9531be52b3-6a811aaa3a.zip/node_modules/minizlib/index.js ***!
  \*************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


const assert = __webpack_require__(/*! assert */ "assert")
const Buffer = __webpack_require__(/*! buffer */ "buffer").Buffer
const realZlib = __webpack_require__(/*! zlib */ "zlib")

const constants = exports.constants = __webpack_require__(/*! ./constants.js */ "./.yarn/cache/minizlib-npm-2.1.0-9531be52b3-6a811aaa3a.zip/node_modules/minizlib/constants.js")
const Minipass = __webpack_require__(/*! minipass */ "./.yarn/cache/minipass-npm-3.1.1-b51d7e264d-cfcfd86adc.zip/node_modules/minipass/index.js")

const OriginalBufferConcat = Buffer.concat

const _superWrite = Symbol('_superWrite')
class ZlibError extends Error {
  constructor (err) {
    super('zlib: ' + err.message)
    this.code = err.code
    this.errno = err.errno
    /* istanbul ignore if */
    if (!this.code)
      this.code = 'ZLIB_ERROR'

    this.message = 'zlib: ' + err.message
    Error.captureStackTrace(this, this.constructor)
  }

  get name () {
    return 'ZlibError'
  }
}

// the Zlib class they all inherit from
// This thing manages the queue of requests, and returns
// true or false if there is anything in the queue when
// you call the .write() method.
const _opts = Symbol('opts')
const _flushFlag = Symbol('flushFlag')
const _finishFlushFlag = Symbol('finishFlushFlag')
const _fullFlushFlag = Symbol('fullFlushFlag')
const _handle = Symbol('handle')
const _onError = Symbol('onError')
const _sawError = Symbol('sawError')
const _level = Symbol('level')
const _strategy = Symbol('strategy')
const _ended = Symbol('ended')
const _defaultFullFlush = Symbol('_defaultFullFlush')

class ZlibBase extends Minipass {
  constructor (opts, mode) {
    if (!opts || typeof opts !== 'object')
      throw new TypeError('invalid options for ZlibBase constructor')

    super(opts)
    this[_ended] = false
    this[_opts] = opts

    this[_flushFlag] = opts.flush
    this[_finishFlushFlag] = opts.finishFlush
    // this will throw if any options are invalid for the class selected
    try {
      this[_handle] = new realZlib[mode](opts)
    } catch (er) {
      // make sure that all errors get decorated properly
      throw new ZlibError(er)
    }

    this[_onError] = (err) => {
      this[_sawError] = true
      // there is no way to cleanly recover.
      // continuing only obscures problems.
      this.close()
      this.emit('error', err)
    }

    this[_handle].on('error', er => this[_onError](new ZlibError(er)))
    this.once('end', () => this.close)
  }

  close () {
    if (this[_handle]) {
      this[_handle].close()
      this[_handle] = null
      this.emit('close')
    }
  }

  reset () {
    if (!this[_sawError]) {
      assert(this[_handle], 'zlib binding closed')
      return this[_handle].reset()
    }
  }

  flush (flushFlag) {
    if (this.ended)
      return

    if (typeof flushFlag !== 'number')
      flushFlag = this[_fullFlushFlag]
    this.write(Object.assign(Buffer.alloc(0), { [_flushFlag]: flushFlag }))
  }

  end (chunk, encoding, cb) {
    if (chunk)
      this.write(chunk, encoding)
    this.flush(this[_finishFlushFlag])
    this[_ended] = true
    return super.end(null, null, cb)
  }

  get ended () {
    return this[_ended]
  }

  write (chunk, encoding, cb) {
    // process the chunk using the sync process
    // then super.write() all the outputted chunks
    if (typeof encoding === 'function')
      cb = encoding, encoding = 'utf8'

    if (typeof chunk === 'string')
      chunk = Buffer.from(chunk, encoding)

    if (this[_sawError])
      return
    assert(this[_handle], 'zlib binding closed')

    // _processChunk tries to .close() the native handle after it's done, so we
    // intercept that by temporarily making it a no-op.
    const nativeHandle = this[_handle]._handle
    const originalNativeClose = nativeHandle.close
    nativeHandle.close = () => {}
    const originalClose = this[_handle].close
    this[_handle].close = () => {}
    // It also calls `Buffer.concat()` at the end, which may be convenient
    // for some, but which we are not interested in as it slows us down.
    Buffer.concat = (args) => args
    let result
    try {
      const flushFlag = typeof chunk[_flushFlag] === 'number'
        ? chunk[_flushFlag] : this[_flushFlag]
      result = this[_handle]._processChunk(chunk, flushFlag)
      // if we don't throw, reset it back how it was
      Buffer.concat = OriginalBufferConcat
    } catch (err) {
      // or if we do, put Buffer.concat() back before we emit error
      // Error events call into user code, which may call Buffer.concat()
      Buffer.concat = OriginalBufferConcat
      this[_onError](new ZlibError(err))
    } finally {
      if (this[_handle]) {
        // Core zlib resets `_handle` to null after attempting to close the
        // native handle. Our no-op handler prevented actual closure, but we
        // need to restore the `._handle` property.
        this[_handle]._handle = nativeHandle
        nativeHandle.close = originalNativeClose
        this[_handle].close = originalClose
        // `_processChunk()` adds an 'error' listener. If we don't remove it
        // after each call, these handlers start piling up.
        this[_handle].removeAllListeners('error')
      }
    }

    let writeReturn
    if (result) {
      if (Array.isArray(result) && result.length > 0) {
        // The first buffer is always `handle._outBuffer`, which would be
        // re-used for later invocations; so, we always have to copy that one.
        writeReturn = this[_superWrite](Buffer.from(result[0]))
        for (let i = 1; i < result.length; i++) {
          writeReturn = this[_superWrite](result[i])
        }
      } else {
        writeReturn = this[_superWrite](Buffer.from(result))
      }
    }

    if (cb)
      cb()
    return writeReturn
  }

  [_superWrite] (data) {
    return super.write(data)
  }
}

class Zlib extends ZlibBase {
  constructor (opts, mode) {
    opts = opts || {}

    opts.flush = opts.flush || constants.Z_NO_FLUSH
    opts.finishFlush = opts.finishFlush || constants.Z_FINISH
    super(opts, mode)

    this[_fullFlushFlag] = constants.Z_FULL_FLUSH
    this[_level] = opts.level
    this[_strategy] = opts.strategy
  }

  params (level, strategy) {
    if (this[_sawError])
      return

    if (!this[_handle])
      throw new Error('cannot switch params when binding is closed')

    // no way to test this without also not supporting params at all
    /* istanbul ignore if */
    if (!this[_handle].params)
      throw new Error('not supported in this implementation')

    if (this[_level] !== level || this[_strategy] !== strategy) {
      this.flush(constants.Z_SYNC_FLUSH)
      assert(this[_handle], 'zlib binding closed')
      // .params() calls .flush(), but the latter is always async in the
      // core zlib. We override .flush() temporarily to intercept that and
      // flush synchronously.
      const origFlush = this[_handle].flush
      this[_handle].flush = (flushFlag, cb) => {
        this.flush(flushFlag)
        cb()
      }
      try {
        this[_handle].params(level, strategy)
      } finally {
        this[_handle].flush = origFlush
      }
      /* istanbul ignore else */
      if (this[_handle]) {
        this[_level] = level
        this[_strategy] = strategy
      }
    }
  }
}

// minimal 2-byte header
class Deflate extends Zlib {
  constructor (opts) {
    super(opts, 'Deflate')
  }
}

class Inflate extends Zlib {
  constructor (opts) {
    super(opts, 'Inflate')
  }
}

// gzip - bigger header, same deflate compression
const _portable = Symbol('_portable')
class Gzip extends Zlib {
  constructor (opts) {
    super(opts, 'Gzip')
    this[_portable] = opts && !!opts.portable
  }

  [_superWrite] (data) {
    if (!this[_portable])
      return super[_superWrite](data)

    // we'll always get the header emitted in one first chunk
    // overwrite the OS indicator byte with 0xFF
    this[_portable] = false
    data[9] = 255
    return super[_superWrite](data)
  }
}

class Gunzip extends Zlib {
  constructor (opts) {
    super(opts, 'Gunzip')
  }
}

// raw - no header
class DeflateRaw extends Zlib {
  constructor (opts) {
    super(opts, 'DeflateRaw')
  }
}

class InflateRaw extends Zlib {
  constructor (opts) {
    super(opts, 'InflateRaw')
  }
}

// auto-detect header.
class Unzip extends Zlib {
  constructor (opts) {
    super(opts, 'Unzip')
  }
}

class Brotli extends ZlibBase {
  constructor (opts, mode) {
    opts = opts || {}

    opts.flush = opts.flush || constants.BROTLI_OPERATION_PROCESS
    opts.finishFlush = opts.finishFlush || constants.BROTLI_OPERATION_FINISH

    super(opts, mode)

    this[_fullFlushFlag] = constants.BROTLI_OPERATION_FLUSH
  }
}

class BrotliCompress extends Brotli {
  constructor (opts) {
    super(opts, 'BrotliCompress')
  }
}

class BrotliDecompress extends Brotli {
  constructor (opts) {
    super(opts, 'BrotliDecompress')
  }
}

exports.Deflate = Deflate
exports.Inflate = Inflate
exports.Gzip = Gzip
exports.Gunzip = Gunzip
exports.DeflateRaw = DeflateRaw
exports.InflateRaw = InflateRaw
exports.Unzip = Unzip
/* istanbul ignore else */
if (typeof realZlib.BrotliCompress === 'function') {
  exports.BrotliCompress = BrotliCompress
  exports.BrotliDecompress = BrotliDecompress
} else {
  exports.BrotliCompress = exports.BrotliDecompress = class {
    constructor () {
      throw new Error('Brotli is not supported in this version of Node.js')
    }
  }
}


/***/ }),

/***/ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/index.js":
/*!*********************************************************************************************!*\
  !*** ./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/index.js ***!
  \*********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const optsArg = __webpack_require__(/*! ./lib/opts-arg.js */ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/opts-arg.js")
const pathArg = __webpack_require__(/*! ./lib/path-arg.js */ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/path-arg.js")

const {mkdirpNative, mkdirpNativeSync} = __webpack_require__(/*! ./lib/mkdirp-native.js */ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/mkdirp-native.js")
const {mkdirpManual, mkdirpManualSync} = __webpack_require__(/*! ./lib/mkdirp-manual.js */ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/mkdirp-manual.js")
const {useNative, useNativeSync} = __webpack_require__(/*! ./lib/use-native.js */ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/use-native.js")


const mkdirp = (path, opts) => {
  path = pathArg(path)
  opts = optsArg(opts)
  return useNative(opts)
    ? mkdirpNative(path, opts)
    : mkdirpManual(path, opts)
}

const mkdirpSync = (path, opts) => {
  path = pathArg(path)
  opts = optsArg(opts)
  return useNativeSync(opts)
    ? mkdirpNativeSync(path, opts)
    : mkdirpManualSync(path, opts)
}

mkdirp.sync = mkdirpSync
mkdirp.native = (path, opts) => mkdirpNative(pathArg(path), optsArg(opts))
mkdirp.manual = (path, opts) => mkdirpManual(pathArg(path), optsArg(opts))
mkdirp.nativeSync = (path, opts) => mkdirpNativeSync(pathArg(path), optsArg(opts))
mkdirp.manualSync = (path, opts) => mkdirpManualSync(pathArg(path), optsArg(opts))

module.exports = mkdirp


/***/ }),

/***/ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/find-made.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/find-made.js ***!
  \*****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const {dirname} = __webpack_require__(/*! path */ "path")

const findMade = (opts, parent, path = undefined) => {
  // we never want the 'made' return value to be a root directory
  if (path === parent)
    return Promise.resolve()

  return opts.statAsync(parent).then(
    st => st.isDirectory() ? path : undefined, // will fail later
    er => er.code === 'ENOENT'
      ? findMade(opts, dirname(parent), parent)
      : undefined
  )
}

const findMadeSync = (opts, parent, path = undefined) => {
  if (path === parent)
    return undefined

  try {
    return opts.statSync(parent).isDirectory() ? path : undefined
  } catch (er) {
    return er.code === 'ENOENT'
      ? findMadeSync(opts, dirname(parent), parent)
      : undefined
  }
}

module.exports = {findMade, findMadeSync}


/***/ }),

/***/ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/mkdirp-manual.js":
/*!*********************************************************************************************************!*\
  !*** ./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/mkdirp-manual.js ***!
  \*********************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const {dirname} = __webpack_require__(/*! path */ "path")

const mkdirpManual = (path, opts, made) => {
  opts.recursive = false
  const parent = dirname(path)
  if (parent === path) {
    return opts.mkdirAsync(path, opts).catch(er => {
      // swallowed by recursive implementation on posix systems
      // any other error is a failure
      if (er.code !== 'EISDIR')
        throw er
    })
  }

  return opts.mkdirAsync(path, opts).then(() => made || path, er => {
    if (er.code === 'ENOENT')
      return mkdirpManual(parent, opts)
        .then(made => mkdirpManual(path, opts, made))
    if (er.code !== 'EEXIST' && er.code !== 'EROFS')
      throw er
    return opts.statAsync(path).then(st => {
      if (st.isDirectory())
        return made
      else
        throw er
    }, () => { throw er })
  })
}

const mkdirpManualSync = (path, opts, made) => {
  const parent = dirname(path)
  opts.recursive = false

  if (parent === path) {
    try {
      return opts.mkdirSync(path, opts)
    } catch (er) {
      // swallowed by recursive implementation on posix systems
      // any other error is a failure
      if (er.code !== 'EISDIR')
        throw er
      else
        return
    }
  }

  try {
    opts.mkdirSync(path, opts)
    return made || path
  } catch (er) {
    if (er.code === 'ENOENT')
      return mkdirpManualSync(path, opts, mkdirpManualSync(parent, opts, made))
    if (er.code !== 'EEXIST' && er.code !== 'EROFS')
      throw er
    try {
      if (!opts.statSync(path).isDirectory())
        throw er
    } catch (_) {
      throw er
    }
  }
}

module.exports = {mkdirpManual, mkdirpManualSync}


/***/ }),

/***/ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/mkdirp-native.js":
/*!*********************************************************************************************************!*\
  !*** ./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/mkdirp-native.js ***!
  \*********************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const {dirname} = __webpack_require__(/*! path */ "path")
const {findMade, findMadeSync} = __webpack_require__(/*! ./find-made.js */ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/find-made.js")
const {mkdirpManual, mkdirpManualSync} = __webpack_require__(/*! ./mkdirp-manual.js */ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/mkdirp-manual.js")

const mkdirpNative = (path, opts) => {
  opts.recursive = true
  const parent = dirname(path)
  if (parent === path)
    return opts.mkdirAsync(path, opts)

  return findMade(opts, path).then(made =>
    opts.mkdirAsync(path, opts).then(() => made)
    .catch(er => {
      if (er.code === 'ENOENT')
        return mkdirpManual(path, opts)
      else
        throw er
    }))
}

const mkdirpNativeSync = (path, opts) => {
  opts.recursive = true
  const parent = dirname(path)
  if (parent === path)
    return opts.mkdirSync(path, opts)

  const made = findMadeSync(opts, path)
  try {
    opts.mkdirSync(path, opts)
    return made
  } catch (er) {
    if (er.code === 'ENOENT')
      return mkdirpManualSync(path, opts)
    else
      throw er
  }
}

module.exports = {mkdirpNative, mkdirpNativeSync}


/***/ }),

/***/ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/opts-arg.js":
/*!****************************************************************************************************!*\
  !*** ./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/opts-arg.js ***!
  \****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const { promisify } = __webpack_require__(/*! util */ "util")
const fs = __webpack_require__(/*! fs */ "fs")
const optsArg = opts => {
  if (!opts)
    opts = { mode: 0o777 & (~process.umask()), fs }
  else if (typeof opts === 'object')
    opts = { mode: 0o777 & (~process.umask()), fs, ...opts }
  else if (typeof opts === 'number')
    opts = { mode: opts, fs }
  else if (typeof opts === 'string')
    opts = { mode: parseInt(opts, 8), fs }
  else
    throw new TypeError('invalid options argument')

  opts.mkdir = opts.mkdir || opts.fs.mkdir || fs.mkdir
  opts.mkdirAsync = promisify(opts.mkdir)
  opts.stat = opts.stat || opts.fs.stat || fs.stat
  opts.statAsync = promisify(opts.stat)
  opts.statSync = opts.statSync || opts.fs.statSync || fs.statSync
  opts.mkdirSync = opts.mkdirSync || opts.fs.mkdirSync || fs.mkdirSync
  return opts
}
module.exports = optsArg


/***/ }),

/***/ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/path-arg.js":
/*!****************************************************************************************************!*\
  !*** ./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/path-arg.js ***!
  \****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const platform = process.env.__TESTING_MKDIRP_PLATFORM__ || process.platform
const { resolve, parse } = __webpack_require__(/*! path */ "path")
const pathArg = path => {
  if (/\0/.test(path)) {
    // simulate same failure that node raises
    throw Object.assign(
      new TypeError('path must be a string without null bytes'),
      {
        path,
        code: 'ERR_INVALID_ARG_VALUE',
      }
    )
  }

  path = resolve(path)
  if (platform === 'win32') {
    const badWinChars = /[*|"<>?:]/
    const {root} = parse(path)
    if (badWinChars.test(path.substr(root.length))) {
      throw Object.assign(new Error('Illegal characters in path.'), {
        path,
        code: 'EINVAL',
      })
    }
  }

  return path
}
module.exports = pathArg


/***/ }),

/***/ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/use-native.js":
/*!******************************************************************************************************!*\
  !*** ./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/lib/use-native.js ***!
  \******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const fs = __webpack_require__(/*! fs */ "fs")

const version = process.env.__TESTING_MKDIRP_NODE_VERSION__ || process.version
const versArr = version.replace(/^v/, '').split('.')
const hasNative = +versArr[0] > 10 || +versArr[0] === 10 && +versArr[1] >= 12

const useNative = !hasNative ? () => false : opts => opts.mkdir === fs.mkdir
const useNativeSync = !hasNative ? () => false : opts => opts.mkdirSync === fs.mkdirSync

module.exports = {useNative, useNativeSync}


/***/ }),

/***/ "./.yarn/cache/ms-npm-2.1.2-ec0c1512ff-673cdb2c31.zip/node_modules/ms/index.js":
/*!*************************************************************************************!*\
  !*** ./.yarn/cache/ms-npm-2.1.2-ec0c1512ff-673cdb2c31.zip/node_modules/ms/index.js ***!
  \*************************************************************************************/
/***/ ((module) => {

/**
 * Helpers.
 */

var s = 1000;
var m = s * 60;
var h = m * 60;
var d = h * 24;
var w = d * 7;
var y = d * 365.25;

/**
 * Parse or format the given `val`.
 *
 * Options:
 *
 *  - `long` verbose formatting [false]
 *
 * @param {String|Number} val
 * @param {Object} [options]
 * @throws {Error} throw an error if val is not a non-empty string or a number
 * @return {String|Number}
 * @api public
 */

module.exports = function(val, options) {
  options = options || {};
  var type = typeof val;
  if (type === 'string' && val.length > 0) {
    return parse(val);
  } else if (type === 'number' && isFinite(val)) {
    return options.long ? fmtLong(val) : fmtShort(val);
  }
  throw new Error(
    'val is not a non-empty string or a valid number. val=' +
      JSON.stringify(val)
  );
};

/**
 * Parse the given `str` and return milliseconds.
 *
 * @param {String} str
 * @return {Number}
 * @api private
 */

function parse(str) {
  str = String(str);
  if (str.length > 100) {
    return;
  }
  var match = /^(-?(?:\d+)?\.?\d+) *(milliseconds?|msecs?|ms|seconds?|secs?|s|minutes?|mins?|m|hours?|hrs?|h|days?|d|weeks?|w|years?|yrs?|y)?$/i.exec(
    str
  );
  if (!match) {
    return;
  }
  var n = parseFloat(match[1]);
  var type = (match[2] || 'ms').toLowerCase();
  switch (type) {
    case 'years':
    case 'year':
    case 'yrs':
    case 'yr':
    case 'y':
      return n * y;
    case 'weeks':
    case 'week':
    case 'w':
      return n * w;
    case 'days':
    case 'day':
    case 'd':
      return n * d;
    case 'hours':
    case 'hour':
    case 'hrs':
    case 'hr':
    case 'h':
      return n * h;
    case 'minutes':
    case 'minute':
    case 'mins':
    case 'min':
    case 'm':
      return n * m;
    case 'seconds':
    case 'second':
    case 'secs':
    case 'sec':
    case 's':
      return n * s;
    case 'milliseconds':
    case 'millisecond':
    case 'msecs':
    case 'msec':
    case 'ms':
      return n;
    default:
      return undefined;
  }
}

/**
 * Short format for `ms`.
 *
 * @param {Number} ms
 * @return {String}
 * @api private
 */

function fmtShort(ms) {
  var msAbs = Math.abs(ms);
  if (msAbs >= d) {
    return Math.round(ms / d) + 'd';
  }
  if (msAbs >= h) {
    return Math.round(ms / h) + 'h';
  }
  if (msAbs >= m) {
    return Math.round(ms / m) + 'm';
  }
  if (msAbs >= s) {
    return Math.round(ms / s) + 's';
  }
  return ms + 'ms';
}

/**
 * Long format for `ms`.
 *
 * @param {Number} ms
 * @return {String}
 * @api private
 */

function fmtLong(ms) {
  var msAbs = Math.abs(ms);
  if (msAbs >= d) {
    return plural(ms, msAbs, d, 'day');
  }
  if (msAbs >= h) {
    return plural(ms, msAbs, h, 'hour');
  }
  if (msAbs >= m) {
    return plural(ms, msAbs, m, 'minute');
  }
  if (msAbs >= s) {
    return plural(ms, msAbs, s, 'second');
  }
  return ms + ' ms';
}

/**
 * Pluralization helper.
 */

function plural(ms, msAbs, n, name) {
  var isPlural = msAbs >= n * 1.5;
  return Math.round(ms / n) + ' ' + name + (isPlural ? 's' : '');
}


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/comparator.js":
/*!**********************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/comparator.js ***!
  \**********************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const ANY = Symbol('SemVer ANY')
// hoisted class for cyclic dependency
class Comparator {
  static get ANY () {
    return ANY
  }
  constructor (comp, options) {
    if (!options || typeof options !== 'object') {
      options = {
        loose: !!options,
        includePrerelease: false
      }
    }

    if (comp instanceof Comparator) {
      if (comp.loose === !!options.loose) {
        return comp
      } else {
        comp = comp.value
      }
    }

    debug('comparator', comp, options)
    this.options = options
    this.loose = !!options.loose
    this.parse(comp)

    if (this.semver === ANY) {
      this.value = ''
    } else {
      this.value = this.operator + this.semver.version
    }

    debug('comp', this)
  }

  parse (comp) {
    const r = this.options.loose ? re[t.COMPARATORLOOSE] : re[t.COMPARATOR]
    const m = comp.match(r)

    if (!m) {
      throw new TypeError(`Invalid comparator: ${comp}`)
    }

    this.operator = m[1] !== undefined ? m[1] : ''
    if (this.operator === '=') {
      this.operator = ''
    }

    // if it literally is just '>' or '' then allow anything.
    if (!m[2]) {
      this.semver = ANY
    } else {
      this.semver = new SemVer(m[2], this.options.loose)
    }
  }

  toString () {
    return this.value
  }

  test (version) {
    debug('Comparator.test', version, this.options.loose)

    if (this.semver === ANY || version === ANY) {
      return true
    }

    if (typeof version === 'string') {
      try {
        version = new SemVer(version, this.options)
      } catch (er) {
        return false
      }
    }

    return cmp(version, this.operator, this.semver, this.options)
  }

  intersects (comp, options) {
    if (!(comp instanceof Comparator)) {
      throw new TypeError('a Comparator is required')
    }

    if (!options || typeof options !== 'object') {
      options = {
        loose: !!options,
        includePrerelease: false
      }
    }

    if (this.operator === '') {
      if (this.value === '') {
        return true
      }
      return new Range(comp.value, options).test(this.value)
    } else if (comp.operator === '') {
      if (comp.value === '') {
        return true
      }
      return new Range(this.value, options).test(comp.semver)
    }

    const sameDirectionIncreasing =
      (this.operator === '>=' || this.operator === '>') &&
      (comp.operator === '>=' || comp.operator === '>')
    const sameDirectionDecreasing =
      (this.operator === '<=' || this.operator === '<') &&
      (comp.operator === '<=' || comp.operator === '<')
    const sameSemVer = this.semver.version === comp.semver.version
    const differentDirectionsInclusive =
      (this.operator === '>=' || this.operator === '<=') &&
      (comp.operator === '>=' || comp.operator === '<=')
    const oppositeDirectionsLessThan =
      cmp(this.semver, '<', comp.semver, options) &&
      (this.operator === '>=' || this.operator === '>') &&
        (comp.operator === '<=' || comp.operator === '<')
    const oppositeDirectionsGreaterThan =
      cmp(this.semver, '>', comp.semver, options) &&
      (this.operator === '<=' || this.operator === '<') &&
        (comp.operator === '>=' || comp.operator === '>')

    return (
      sameDirectionIncreasing ||
      sameDirectionDecreasing ||
      (sameSemVer && differentDirectionsInclusive) ||
      oppositeDirectionsLessThan ||
      oppositeDirectionsGreaterThan
    )
  }
}

module.exports = Comparator

const {re, t} = __webpack_require__(/*! ../internal/re */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/re.js")
const cmp = __webpack_require__(/*! ../functions/cmp */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/cmp.js")
const debug = __webpack_require__(/*! ../internal/debug */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/debug.js")
const SemVer = __webpack_require__(/*! ./semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const Range = __webpack_require__(/*! ./range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js")


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js ***!
  \*****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

// hoisted class for cyclic dependency
class Range {
  constructor (range, options) {
    if (!options || typeof options !== 'object') {
      options = {
        loose: !!options,
        includePrerelease: false
      }
    }

    if (range instanceof Range) {
      if (
        range.loose === !!options.loose &&
        range.includePrerelease === !!options.includePrerelease
      ) {
        return range
      } else {
        return new Range(range.raw, options)
      }
    }

    if (range instanceof Comparator) {
      // just put it in the set and return
      this.raw = range.value
      this.set = [[range]]
      this.format()
      return this
    }

    this.options = options
    this.loose = !!options.loose
    this.includePrerelease = !!options.includePrerelease

    // First, split based on boolean or ||
    this.raw = range
    this.set = range
      .split(/\s*\|\|\s*/)
      // map the range to a 2d array of comparators
      .map(range => this.parseRange(range.trim()))
      // throw out any comparator lists that are empty
      // this generally means that it was not a valid range, which is allowed
      // in loose mode, but will still throw if the WHOLE range is invalid.
      .filter(c => c.length)

    if (!this.set.length) {
      throw new TypeError(`Invalid SemVer Range: ${range}`)
    }

    this.format()
  }

  format () {
    this.range = this.set
      .map((comps) => {
        return comps.join(' ').trim()
      })
      .join('||')
      .trim()
    return this.range
  }

  toString () {
    return this.range
  }

  parseRange (range) {
    const loose = this.options.loose
    range = range.trim()
    // `1.2.3 - 1.2.4` => `>=1.2.3 <=1.2.4`
    const hr = loose ? re[t.HYPHENRANGELOOSE] : re[t.HYPHENRANGE]
    range = range.replace(hr, hyphenReplace)
    debug('hyphen replace', range)
    // `> 1.2.3 < 1.2.5` => `>1.2.3 <1.2.5`
    range = range.replace(re[t.COMPARATORTRIM], comparatorTrimReplace)
    debug('comparator trim', range, re[t.COMPARATORTRIM])

    // `~ 1.2.3` => `~1.2.3`
    range = range.replace(re[t.TILDETRIM], tildeTrimReplace)

    // `^ 1.2.3` => `^1.2.3`
    range = range.replace(re[t.CARETTRIM], caretTrimReplace)

    // normalize spaces
    range = range.split(/\s+/).join(' ')

    // At this point, the range is completely trimmed and
    // ready to be split into comparators.

    const compRe = loose ? re[t.COMPARATORLOOSE] : re[t.COMPARATOR]
    return range
      .split(' ')
      .map(comp => parseComparator(comp, this.options))
      .join(' ')
      .split(/\s+/)
      // in loose mode, throw out any that are not valid comparators
      .filter(this.options.loose ? comp => !!comp.match(compRe) : () => true)
      .map(comp => new Comparator(comp, this.options))
  }

  intersects (range, options) {
    if (!(range instanceof Range)) {
      throw new TypeError('a Range is required')
    }

    return this.set.some((thisComparators) => {
      return (
        isSatisfiable(thisComparators, options) &&
        range.set.some((rangeComparators) => {
          return (
            isSatisfiable(rangeComparators, options) &&
            thisComparators.every((thisComparator) => {
              return rangeComparators.every((rangeComparator) => {
                return thisComparator.intersects(rangeComparator, options)
              })
            })
          )
        })
      )
    })
  }

  // if ANY of the sets match ALL of its comparators, then pass
  test (version) {
    if (!version) {
      return false
    }

    if (typeof version === 'string') {
      try {
        version = new SemVer(version, this.options)
      } catch (er) {
        return false
      }
    }

    for (let i = 0; i < this.set.length; i++) {
      if (testSet(this.set[i], version, this.options)) {
        return true
      }
    }
    return false
  }
}
module.exports = Range

const Comparator = __webpack_require__(/*! ./comparator */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/comparator.js")
const debug = __webpack_require__(/*! ../internal/debug */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/debug.js")
const SemVer = __webpack_require__(/*! ./semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const {
  re,
  t,
  comparatorTrimReplace,
  tildeTrimReplace,
  caretTrimReplace
} = __webpack_require__(/*! ../internal/re */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/re.js")

// take a set of comparators and determine whether there
// exists a version which can satisfy it
const isSatisfiable = (comparators, options) => {
  let result = true
  const remainingComparators = comparators.slice()
  let testComparator = remainingComparators.pop()

  while (result && remainingComparators.length) {
    result = remainingComparators.every((otherComparator) => {
      return testComparator.intersects(otherComparator, options)
    })

    testComparator = remainingComparators.pop()
  }

  return result
}

// comprised of xranges, tildes, stars, and gtlt's at this point.
// already replaced the hyphen ranges
// turn into a set of JUST comparators.
const parseComparator = (comp, options) => {
  debug('comp', comp, options)
  comp = replaceCarets(comp, options)
  debug('caret', comp)
  comp = replaceTildes(comp, options)
  debug('tildes', comp)
  comp = replaceXRanges(comp, options)
  debug('xrange', comp)
  comp = replaceStars(comp, options)
  debug('stars', comp)
  return comp
}

const isX = id => !id || id.toLowerCase() === 'x' || id === '*'

// ~, ~> --> * (any, kinda silly)
// ~2, ~2.x, ~2.x.x, ~>2, ~>2.x ~>2.x.x --> >=2.0.0 <3.0.0
// ~2.0, ~2.0.x, ~>2.0, ~>2.0.x --> >=2.0.0 <2.1.0
// ~1.2, ~1.2.x, ~>1.2, ~>1.2.x --> >=1.2.0 <1.3.0
// ~1.2.3, ~>1.2.3 --> >=1.2.3 <1.3.0
// ~1.2.0, ~>1.2.0 --> >=1.2.0 <1.3.0
const replaceTildes = (comp, options) =>
  comp.trim().split(/\s+/).map((comp) => {
    return replaceTilde(comp, options)
  }).join(' ')

const replaceTilde = (comp, options) => {
  const r = options.loose ? re[t.TILDELOOSE] : re[t.TILDE]
  return comp.replace(r, (_, M, m, p, pr) => {
    debug('tilde', comp, _, M, m, p, pr)
    let ret

    if (isX(M)) {
      ret = ''
    } else if (isX(m)) {
      ret = `>=${M}.0.0 <${+M + 1}.0.0`
    } else if (isX(p)) {
      // ~1.2 == >=1.2.0 <1.3.0
      ret = `>=${M}.${m}.0 <${M}.${+m + 1}.0`
    } else if (pr) {
      debug('replaceTilde pr', pr)
      ret = `>=${M}.${m}.${p}-${pr
      } <${M}.${+m + 1}.0`
    } else {
      // ~1.2.3 == >=1.2.3 <1.3.0
      ret = `>=${M}.${m}.${p
      } <${M}.${+m + 1}.0`
    }

    debug('tilde return', ret)
    return ret
  })
}

// ^ --> * (any, kinda silly)
// ^2, ^2.x, ^2.x.x --> >=2.0.0 <3.0.0
// ^2.0, ^2.0.x --> >=2.0.0 <3.0.0
// ^1.2, ^1.2.x --> >=1.2.0 <2.0.0
// ^1.2.3 --> >=1.2.3 <2.0.0
// ^1.2.0 --> >=1.2.0 <2.0.0
const replaceCarets = (comp, options) =>
  comp.trim().split(/\s+/).map((comp) => {
    return replaceCaret(comp, options)
  }).join(' ')

const replaceCaret = (comp, options) => {
  debug('caret', comp, options)
  const r = options.loose ? re[t.CARETLOOSE] : re[t.CARET]
  return comp.replace(r, (_, M, m, p, pr) => {
    debug('caret', comp, _, M, m, p, pr)
    let ret

    if (isX(M)) {
      ret = ''
    } else if (isX(m)) {
      ret = `>=${M}.0.0 <${+M + 1}.0.0`
    } else if (isX(p)) {
      if (M === '0') {
        ret = `>=${M}.${m}.0 <${M}.${+m + 1}.0`
      } else {
        ret = `>=${M}.${m}.0 <${+M + 1}.0.0`
      }
    } else if (pr) {
      debug('replaceCaret pr', pr)
      if (M === '0') {
        if (m === '0') {
          ret = `>=${M}.${m}.${p}-${pr
          } <${M}.${m}.${+p + 1}`
        } else {
          ret = `>=${M}.${m}.${p}-${pr
          } <${M}.${+m + 1}.0`
        }
      } else {
        ret = `>=${M}.${m}.${p}-${pr
        } <${+M + 1}.0.0`
      }
    } else {
      debug('no pr')
      if (M === '0') {
        if (m === '0') {
          ret = `>=${M}.${m}.${p
          } <${M}.${m}.${+p + 1}`
        } else {
          ret = `>=${M}.${m}.${p
          } <${M}.${+m + 1}.0`
        }
      } else {
        ret = `>=${M}.${m}.${p
        } <${+M + 1}.0.0`
      }
    }

    debug('caret return', ret)
    return ret
  })
}

const replaceXRanges = (comp, options) => {
  debug('replaceXRanges', comp, options)
  return comp.split(/\s+/).map((comp) => {
    return replaceXRange(comp, options)
  }).join(' ')
}

const replaceXRange = (comp, options) => {
  comp = comp.trim()
  const r = options.loose ? re[t.XRANGELOOSE] : re[t.XRANGE]
  return comp.replace(r, (ret, gtlt, M, m, p, pr) => {
    debug('xRange', comp, ret, gtlt, M, m, p, pr)
    const xM = isX(M)
    const xm = xM || isX(m)
    const xp = xm || isX(p)
    const anyX = xp

    if (gtlt === '=' && anyX) {
      gtlt = ''
    }

    // if we're including prereleases in the match, then we need
    // to fix this to -0, the lowest possible prerelease value
    pr = options.includePrerelease ? '-0' : ''

    if (xM) {
      if (gtlt === '>' || gtlt === '<') {
        // nothing is allowed
        ret = '<0.0.0-0'
      } else {
        // nothing is forbidden
        ret = '*'
      }
    } else if (gtlt && anyX) {
      // we know patch is an x, because we have any x at all.
      // replace X with 0
      if (xm) {
        m = 0
      }
      p = 0

      if (gtlt === '>') {
        // >1 => >=2.0.0
        // >1.2 => >=1.3.0
        gtlt = '>='
        if (xm) {
          M = +M + 1
          m = 0
          p = 0
        } else {
          m = +m + 1
          p = 0
        }
      } else if (gtlt === '<=') {
        // <=0.7.x is actually <0.8.0, since any 0.7.x should
        // pass.  Similarly, <=7.x is actually <8.0.0, etc.
        gtlt = '<'
        if (xm) {
          M = +M + 1
        } else {
          m = +m + 1
        }
      }

      ret = `${gtlt + M}.${m}.${p}${pr}`
    } else if (xm) {
      ret = `>=${M}.0.0${pr} <${+M + 1}.0.0${pr}`
    } else if (xp) {
      ret = `>=${M}.${m}.0${pr
      } <${M}.${+m + 1}.0${pr}`
    }

    debug('xRange return', ret)

    return ret
  })
}

// Because * is AND-ed with everything else in the comparator,
// and '' means "any version", just remove the *s entirely.
const replaceStars = (comp, options) => {
  debug('replaceStars', comp, options)
  // Looseness is ignored here.  star is always as loose as it gets!
  return comp.trim().replace(re[t.STAR], '')
}

// This function is passed to string.replace(re[t.HYPHENRANGE])
// M, m, patch, prerelease, build
// 1.2 - 3.4.5 => >=1.2.0 <=3.4.5
// 1.2.3 - 3.4 => >=1.2.0 <3.5.0 Any 3.4.x will do
// 1.2 - 3.4 => >=1.2.0 <3.5.0
const hyphenReplace = ($0,
  from, fM, fm, fp, fpr, fb,
  to, tM, tm, tp, tpr, tb) => {
  if (isX(fM)) {
    from = ''
  } else if (isX(fm)) {
    from = `>=${fM}.0.0`
  } else if (isX(fp)) {
    from = `>=${fM}.${fm}.0`
  } else {
    from = `>=${from}`
  }

  if (isX(tM)) {
    to = ''
  } else if (isX(tm)) {
    to = `<${+tM + 1}.0.0`
  } else if (isX(tp)) {
    to = `<${tM}.${+tm + 1}.0`
  } else if (tpr) {
    to = `<=${tM}.${tm}.${tp}-${tpr}`
  } else {
    to = `<=${to}`
  }

  return (`${from} ${to}`).trim()
}

const testSet = (set, version, options) => {
  for (let i = 0; i < set.length; i++) {
    if (!set[i].test(version)) {
      return false
    }
  }

  if (version.prerelease.length && !options.includePrerelease) {
    // Find the set of versions that are allowed to have prereleases
    // For example, ^1.2.3-pr.1 desugars to >=1.2.3-pr.1 <2.0.0
    // That should allow `1.2.3-pr.2` to pass.
    // However, `1.2.4-alpha.notready` should NOT be allowed,
    // even though it's within the range set by the comparators.
    for (let i = 0; i < set.length; i++) {
      debug(set[i].semver)
      if (set[i].semver === Comparator.ANY) {
        continue
      }

      if (set[i].semver.prerelease.length > 0) {
        const allowed = set[i].semver
        if (allowed.major === version.major &&
            allowed.minor === version.minor &&
            allowed.patch === version.patch) {
          return true
        }
      }
    }

    // Version has a -pre, but it's not one of the ones we like.
    return false
  }

  return true
}


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js":
/*!******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js ***!
  \******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const debug = __webpack_require__(/*! ../internal/debug */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/debug.js")
const { MAX_LENGTH, MAX_SAFE_INTEGER } = __webpack_require__(/*! ../internal/constants */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/constants.js")
const { re, t } = __webpack_require__(/*! ../internal/re */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/re.js")

const { compareIdentifiers } = __webpack_require__(/*! ../internal/identifiers */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/identifiers.js")
class SemVer {
  constructor (version, options) {
    if (!options || typeof options !== 'object') {
      options = {
        loose: !!options,
        includePrerelease: false
      }
    }
    if (version instanceof SemVer) {
      if (version.loose === !!options.loose &&
          version.includePrerelease === !!options.includePrerelease) {
        return version
      } else {
        version = version.version
      }
    } else if (typeof version !== 'string') {
      throw new TypeError(`Invalid Version: ${version}`)
    }

    if (version.length > MAX_LENGTH) {
      throw new TypeError(
        `version is longer than ${MAX_LENGTH} characters`
      )
    }

    debug('SemVer', version, options)
    this.options = options
    this.loose = !!options.loose
    // this isn't actually relevant for versions, but keep it so that we
    // don't run into trouble passing this.options around.
    this.includePrerelease = !!options.includePrerelease

    const m = version.trim().match(options.loose ? re[t.LOOSE] : re[t.FULL])

    if (!m) {
      throw new TypeError(`Invalid Version: ${version}`)
    }

    this.raw = version

    // these are actually numbers
    this.major = +m[1]
    this.minor = +m[2]
    this.patch = +m[3]

    if (this.major > MAX_SAFE_INTEGER || this.major < 0) {
      throw new TypeError('Invalid major version')
    }

    if (this.minor > MAX_SAFE_INTEGER || this.minor < 0) {
      throw new TypeError('Invalid minor version')
    }

    if (this.patch > MAX_SAFE_INTEGER || this.patch < 0) {
      throw new TypeError('Invalid patch version')
    }

    // numberify any prerelease numeric ids
    if (!m[4]) {
      this.prerelease = []
    } else {
      this.prerelease = m[4].split('.').map((id) => {
        if (/^[0-9]+$/.test(id)) {
          const num = +id
          if (num >= 0 && num < MAX_SAFE_INTEGER) {
            return num
          }
        }
        return id
      })
    }

    this.build = m[5] ? m[5].split('.') : []
    this.format()
  }

  format () {
    this.version = `${this.major}.${this.minor}.${this.patch}`
    if (this.prerelease.length) {
      this.version += `-${this.prerelease.join('.')}`
    }
    return this.version
  }

  toString () {
    return this.version
  }

  compare (other) {
    debug('SemVer.compare', this.version, this.options, other)
    if (!(other instanceof SemVer)) {
      if (typeof other === 'string' && other === this.version) {
        return 0
      }
      other = new SemVer(other, this.options)
    }

    if (other.version === this.version) {
      return 0
    }

    return this.compareMain(other) || this.comparePre(other)
  }

  compareMain (other) {
    if (!(other instanceof SemVer)) {
      other = new SemVer(other, this.options)
    }

    return (
      compareIdentifiers(this.major, other.major) ||
      compareIdentifiers(this.minor, other.minor) ||
      compareIdentifiers(this.patch, other.patch)
    )
  }

  comparePre (other) {
    if (!(other instanceof SemVer)) {
      other = new SemVer(other, this.options)
    }

    // NOT having a prerelease is > having one
    if (this.prerelease.length && !other.prerelease.length) {
      return -1
    } else if (!this.prerelease.length && other.prerelease.length) {
      return 1
    } else if (!this.prerelease.length && !other.prerelease.length) {
      return 0
    }

    let i = 0
    do {
      const a = this.prerelease[i]
      const b = other.prerelease[i]
      debug('prerelease compare', i, a, b)
      if (a === undefined && b === undefined) {
        return 0
      } else if (b === undefined) {
        return 1
      } else if (a === undefined) {
        return -1
      } else if (a === b) {
        continue
      } else {
        return compareIdentifiers(a, b)
      }
    } while (++i)
  }

  compareBuild (other) {
    if (!(other instanceof SemVer)) {
      other = new SemVer(other, this.options)
    }

    let i = 0
    do {
      const a = this.build[i]
      const b = other.build[i]
      debug('prerelease compare', i, a, b)
      if (a === undefined && b === undefined) {
        return 0
      } else if (b === undefined) {
        return 1
      } else if (a === undefined) {
        return -1
      } else if (a === b) {
        continue
      } else {
        return compareIdentifiers(a, b)
      }
    } while (++i)
  }

  // preminor will bump the version up to the next minor release, and immediately
  // down to pre-release. premajor and prepatch work the same way.
  inc (release, identifier) {
    switch (release) {
      case 'premajor':
        this.prerelease.length = 0
        this.patch = 0
        this.minor = 0
        this.major++
        this.inc('pre', identifier)
        break
      case 'preminor':
        this.prerelease.length = 0
        this.patch = 0
        this.minor++
        this.inc('pre', identifier)
        break
      case 'prepatch':
        // If this is already a prerelease, it will bump to the next version
        // drop any prereleases that might already exist, since they are not
        // relevant at this point.
        this.prerelease.length = 0
        this.inc('patch', identifier)
        this.inc('pre', identifier)
        break
      // If the input is a non-prerelease version, this acts the same as
      // prepatch.
      case 'prerelease':
        if (this.prerelease.length === 0) {
          this.inc('patch', identifier)
        }
        this.inc('pre', identifier)
        break

      case 'major':
        // If this is a pre-major version, bump up to the same major version.
        // Otherwise increment major.
        // 1.0.0-5 bumps to 1.0.0
        // 1.1.0 bumps to 2.0.0
        if (
          this.minor !== 0 ||
          this.patch !== 0 ||
          this.prerelease.length === 0
        ) {
          this.major++
        }
        this.minor = 0
        this.patch = 0
        this.prerelease = []
        break
      case 'minor':
        // If this is a pre-minor version, bump up to the same minor version.
        // Otherwise increment minor.
        // 1.2.0-5 bumps to 1.2.0
        // 1.2.1 bumps to 1.3.0
        if (this.patch !== 0 || this.prerelease.length === 0) {
          this.minor++
        }
        this.patch = 0
        this.prerelease = []
        break
      case 'patch':
        // If this is not a pre-release version, it will increment the patch.
        // If it is a pre-release it will bump up to the same patch version.
        // 1.2.0-5 patches to 1.2.0
        // 1.2.0 patches to 1.2.1
        if (this.prerelease.length === 0) {
          this.patch++
        }
        this.prerelease = []
        break
      // This probably shouldn't be used publicly.
      // 1.0.0 'pre' would become 1.0.0-0 which is the wrong direction.
      case 'pre':
        if (this.prerelease.length === 0) {
          this.prerelease = [0]
        } else {
          let i = this.prerelease.length
          while (--i >= 0) {
            if (typeof this.prerelease[i] === 'number') {
              this.prerelease[i]++
              i = -2
            }
          }
          if (i === -1) {
            // didn't increment anything
            this.prerelease.push(0)
          }
        }
        if (identifier) {
          // 1.2.0-beta.1 bumps to 1.2.0-beta.2,
          // 1.2.0-beta.fooblz or 1.2.0-beta bumps to 1.2.0-beta.0
          if (this.prerelease[0] === identifier) {
            if (isNaN(this.prerelease[1])) {
              this.prerelease = [identifier, 0]
            }
          } else {
            this.prerelease = [identifier, 0]
          }
        }
        break

      default:
        throw new Error(`invalid increment argument: ${release}`)
    }
    this.format()
    this.raw = this.version
    return this
  }
}

module.exports = SemVer


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/clean.js":
/*!*******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/clean.js ***!
  \*******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const parse = __webpack_require__(/*! ./parse */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/parse.js")
const clean = (version, options) => {
  const s = parse(version.trim().replace(/^[=v]+/, ''), options)
  return s ? s.version : null
}
module.exports = clean


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/cmp.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/cmp.js ***!
  \*****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const eq = __webpack_require__(/*! ./eq */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/eq.js")
const neq = __webpack_require__(/*! ./neq */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/neq.js")
const gt = __webpack_require__(/*! ./gt */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gt.js")
const gte = __webpack_require__(/*! ./gte */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gte.js")
const lt = __webpack_require__(/*! ./lt */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lt.js")
const lte = __webpack_require__(/*! ./lte */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lte.js")

const cmp = (a, op, b, loose) => {
  switch (op) {
    case '===':
      if (typeof a === 'object')
        a = a.version
      if (typeof b === 'object')
        b = b.version
      return a === b

    case '!==':
      if (typeof a === 'object')
        a = a.version
      if (typeof b === 'object')
        b = b.version
      return a !== b

    case '':
    case '=':
    case '==':
      return eq(a, b, loose)

    case '!=':
      return neq(a, b, loose)

    case '>':
      return gt(a, b, loose)

    case '>=':
      return gte(a, b, loose)

    case '<':
      return lt(a, b, loose)

    case '<=':
      return lte(a, b, loose)

    default:
      throw new TypeError(`Invalid operator: ${op}`)
  }
}
module.exports = cmp


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/coerce.js":
/*!********************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/coerce.js ***!
  \********************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const parse = __webpack_require__(/*! ./parse */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/parse.js")
const {re, t} = __webpack_require__(/*! ../internal/re */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/re.js")

const coerce = (version, options) => {
  if (version instanceof SemVer) {
    return version
  }

  if (typeof version === 'number') {
    version = String(version)
  }

  if (typeof version !== 'string') {
    return null
  }

  options = options || {}

  let match = null
  if (!options.rtl) {
    match = version.match(re[t.COERCE])
  } else {
    // Find the right-most coercible string that does not share
    // a terminus with a more left-ward coercible string.
    // Eg, '1.2.3.4' wants to coerce '2.3.4', not '3.4' or '4'
    //
    // Walk through the string checking with a /g regexp
    // Manually set the index so as to pick up overlapping matches.
    // Stop when we get a match that ends at the string end, since no
    // coercible string can be more right-ward without the same terminus.
    let next
    while ((next = re[t.COERCERTL].exec(version)) &&
        (!match || match.index + match[0].length !== version.length)
    ) {
      if (!match ||
            next.index + next[0].length !== match.index + match[0].length) {
        match = next
      }
      re[t.COERCERTL].lastIndex = next.index + next[1].length + next[2].length
    }
    // leave it in a clean state
    re[t.COERCERTL].lastIndex = -1
  }

  if (match === null)
    return null

  return parse(`${match[2]}.${match[3] || '0'}.${match[4] || '0'}`, options)
}
module.exports = coerce


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare-build.js":
/*!***************************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare-build.js ***!
  \***************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const compareBuild = (a, b, loose) => {
  const versionA = new SemVer(a, loose)
  const versionB = new SemVer(b, loose)
  return versionA.compare(versionB) || versionA.compareBuild(versionB)
}
module.exports = compareBuild


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare-loose.js":
/*!***************************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare-loose.js ***!
  \***************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compare = __webpack_require__(/*! ./compare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js")
const compareLoose = (a, b) => compare(a, b, true)
module.exports = compareLoose


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js":
/*!*********************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js ***!
  \*********************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const compare = (a, b, loose) =>
  new SemVer(a, loose).compare(new SemVer(b, loose))

module.exports = compare


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/diff.js":
/*!******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/diff.js ***!
  \******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const parse = __webpack_require__(/*! ./parse */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/parse.js")
const eq = __webpack_require__(/*! ./eq */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/eq.js")

const diff = (version1, version2) => {
  if (eq(version1, version2)) {
    return null
  } else {
    const v1 = parse(version1)
    const v2 = parse(version2)
    const hasPre = v1.prerelease.length || v2.prerelease.length
    const prefix = hasPre ? 'pre' : ''
    const defaultResult = hasPre ? 'prerelease' : ''
    for (const key in v1) {
      if (key === 'major' || key === 'minor' || key === 'patch') {
        if (v1[key] !== v2[key]) {
          return prefix + key
        }
      }
    }
    return defaultResult // may be undefined
  }
}
module.exports = diff


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/eq.js":
/*!****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/eq.js ***!
  \****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compare = __webpack_require__(/*! ./compare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js")
const eq = (a, b, loose) => compare(a, b, loose) === 0
module.exports = eq


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gt.js":
/*!****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gt.js ***!
  \****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compare = __webpack_require__(/*! ./compare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js")
const gt = (a, b, loose) => compare(a, b, loose) > 0
module.exports = gt


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gte.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gte.js ***!
  \*****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compare = __webpack_require__(/*! ./compare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js")
const gte = (a, b, loose) => compare(a, b, loose) >= 0
module.exports = gte


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/inc.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/inc.js ***!
  \*****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")

const inc = (version, release, options, identifier) => {
  if (typeof (options) === 'string') {
    identifier = options
    options = undefined
  }

  try {
    return new SemVer(version, options).inc(release, identifier).version
  } catch (er) {
    return null
  }
}
module.exports = inc


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lt.js":
/*!****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lt.js ***!
  \****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compare = __webpack_require__(/*! ./compare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js")
const lt = (a, b, loose) => compare(a, b, loose) < 0
module.exports = lt


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lte.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lte.js ***!
  \*****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compare = __webpack_require__(/*! ./compare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js")
const lte = (a, b, loose) => compare(a, b, loose) <= 0
module.exports = lte


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/major.js":
/*!*******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/major.js ***!
  \*******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const major = (a, loose) => new SemVer(a, loose).major
module.exports = major


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/minor.js":
/*!*******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/minor.js ***!
  \*******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const minor = (a, loose) => new SemVer(a, loose).minor
module.exports = minor


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/neq.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/neq.js ***!
  \*****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compare = __webpack_require__(/*! ./compare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js")
const neq = (a, b, loose) => compare(a, b, loose) !== 0
module.exports = neq


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/parse.js":
/*!*******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/parse.js ***!
  \*******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const {MAX_LENGTH} = __webpack_require__(/*! ../internal/constants */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/constants.js")
const { re, t } = __webpack_require__(/*! ../internal/re */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/re.js")
const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")

const parse = (version, options) => {
  if (!options || typeof options !== 'object') {
    options = {
      loose: !!options,
      includePrerelease: false
    }
  }

  if (version instanceof SemVer) {
    return version
  }

  if (typeof version !== 'string') {
    return null
  }

  if (version.length > MAX_LENGTH) {
    return null
  }

  const r = options.loose ? re[t.LOOSE] : re[t.FULL]
  if (!r.test(version)) {
    return null
  }

  try {
    return new SemVer(version, options)
  } catch (er) {
    return null
  }
}

module.exports = parse


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/patch.js":
/*!*******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/patch.js ***!
  \*******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const patch = (a, loose) => new SemVer(a, loose).patch
module.exports = patch


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/prerelease.js":
/*!************************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/prerelease.js ***!
  \************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const parse = __webpack_require__(/*! ./parse */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/parse.js")
const prerelease = (version, options) => {
  const parsed = parse(version, options)
  return (parsed && parsed.prerelease.length) ? parsed.prerelease : null
}
module.exports = prerelease


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/rcompare.js":
/*!**********************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/rcompare.js ***!
  \**********************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compare = __webpack_require__(/*! ./compare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js")
const rcompare = (a, b, loose) => compare(b, a, loose)
module.exports = rcompare


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/rsort.js":
/*!*******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/rsort.js ***!
  \*******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compareBuild = __webpack_require__(/*! ./compare-build */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare-build.js")
const rsort = (list, loose) => list.sort((a, b) => compareBuild(b, a, loose))
module.exports = rsort


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/satisfies.js":
/*!***********************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/satisfies.js ***!
  \***********************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const Range = __webpack_require__(/*! ../classes/range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js")
const satisfies = (version, range, options) => {
  try {
    range = new Range(range, options)
  } catch (er) {
    return false
  }
  return range.test(version)
}
module.exports = satisfies


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/sort.js":
/*!******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/sort.js ***!
  \******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const compareBuild = __webpack_require__(/*! ./compare-build */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare-build.js")
const sort = (list, loose) => list.sort((a, b) => compareBuild(a, b, loose))
module.exports = sort


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/valid.js":
/*!*******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/valid.js ***!
  \*******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const parse = __webpack_require__(/*! ./parse */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/parse.js")
const valid = (version, options) => {
  const v = parse(version, options)
  return v ? v.version : null
}
module.exports = valid


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/index.js":
/*!*********************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/index.js ***!
  \*********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

// just pre-load all the stuff that index.js lazily exports
const internalRe = __webpack_require__(/*! ./internal/re */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/re.js")
module.exports = {
  re: internalRe.re,
  src: internalRe.src,
  tokens: internalRe.t,
  SEMVER_SPEC_VERSION: __webpack_require__(/*! ./internal/constants */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/constants.js").SEMVER_SPEC_VERSION,
  SemVer: __webpack_require__(/*! ./classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js"),
  compareIdentifiers: __webpack_require__(/*! ./internal/identifiers */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/identifiers.js").compareIdentifiers,
  rcompareIdentifiers: __webpack_require__(/*! ./internal/identifiers */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/identifiers.js").rcompareIdentifiers,
  parse: __webpack_require__(/*! ./functions/parse */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/parse.js"),
  valid: __webpack_require__(/*! ./functions/valid */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/valid.js"),
  clean: __webpack_require__(/*! ./functions/clean */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/clean.js"),
  inc: __webpack_require__(/*! ./functions/inc */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/inc.js"),
  diff: __webpack_require__(/*! ./functions/diff */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/diff.js"),
  major: __webpack_require__(/*! ./functions/major */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/major.js"),
  minor: __webpack_require__(/*! ./functions/minor */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/minor.js"),
  patch: __webpack_require__(/*! ./functions/patch */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/patch.js"),
  prerelease: __webpack_require__(/*! ./functions/prerelease */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/prerelease.js"),
  compare: __webpack_require__(/*! ./functions/compare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare.js"),
  rcompare: __webpack_require__(/*! ./functions/rcompare */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/rcompare.js"),
  compareLoose: __webpack_require__(/*! ./functions/compare-loose */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare-loose.js"),
  compareBuild: __webpack_require__(/*! ./functions/compare-build */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/compare-build.js"),
  sort: __webpack_require__(/*! ./functions/sort */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/sort.js"),
  rsort: __webpack_require__(/*! ./functions/rsort */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/rsort.js"),
  gt: __webpack_require__(/*! ./functions/gt */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gt.js"),
  lt: __webpack_require__(/*! ./functions/lt */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lt.js"),
  eq: __webpack_require__(/*! ./functions/eq */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/eq.js"),
  neq: __webpack_require__(/*! ./functions/neq */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/neq.js"),
  gte: __webpack_require__(/*! ./functions/gte */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gte.js"),
  lte: __webpack_require__(/*! ./functions/lte */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lte.js"),
  cmp: __webpack_require__(/*! ./functions/cmp */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/cmp.js"),
  coerce: __webpack_require__(/*! ./functions/coerce */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/coerce.js"),
  Comparator: __webpack_require__(/*! ./classes/comparator */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/comparator.js"),
  Range: __webpack_require__(/*! ./classes/range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js"),
  satisfies: __webpack_require__(/*! ./functions/satisfies */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/satisfies.js"),
  toComparators: __webpack_require__(/*! ./ranges/to-comparators */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/to-comparators.js"),
  maxSatisfying: __webpack_require__(/*! ./ranges/max-satisfying */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/max-satisfying.js"),
  minSatisfying: __webpack_require__(/*! ./ranges/min-satisfying */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/min-satisfying.js"),
  minVersion: __webpack_require__(/*! ./ranges/min-version */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/min-version.js"),
  validRange: __webpack_require__(/*! ./ranges/valid */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/valid.js"),
  outside: __webpack_require__(/*! ./ranges/outside */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/outside.js"),
  gtr: __webpack_require__(/*! ./ranges/gtr */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/gtr.js"),
  ltr: __webpack_require__(/*! ./ranges/ltr */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/ltr.js"),
  intersects: __webpack_require__(/*! ./ranges/intersects */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/intersects.js"),
}


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/constants.js":
/*!**********************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/constants.js ***!
  \**********************************************************************************************************/
/***/ ((module) => {

// Note: this is the semver.org version of the spec that it implements
// Not necessarily the package version of this code.
const SEMVER_SPEC_VERSION = '2.0.0'

const MAX_LENGTH = 256
const MAX_SAFE_INTEGER = Number.MAX_SAFE_INTEGER ||
  /* istanbul ignore next */ 9007199254740991

// Max safe segment length for coercion.
const MAX_SAFE_COMPONENT_LENGTH = 16

module.exports = {
  SEMVER_SPEC_VERSION,
  MAX_LENGTH,
  MAX_SAFE_INTEGER,
  MAX_SAFE_COMPONENT_LENGTH
}


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/debug.js":
/*!******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/debug.js ***!
  \******************************************************************************************************/
/***/ ((module) => {

const debug = (
  typeof process === 'object' &&
  process.env &&
  process.env.NODE_DEBUG &&
  /\bsemver\b/i.test(process.env.NODE_DEBUG)
) ? (...args) => console.error('SEMVER', ...args)
  : () => {}

module.exports = debug


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/identifiers.js":
/*!************************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/identifiers.js ***!
  \************************************************************************************************************/
/***/ ((module) => {

const numeric = /^[0-9]+$/
const compareIdentifiers = (a, b) => {
  const anum = numeric.test(a)
  const bnum = numeric.test(b)

  if (anum && bnum) {
    a = +a
    b = +b
  }

  return a === b ? 0
    : (anum && !bnum) ? -1
    : (bnum && !anum) ? 1
    : a < b ? -1
    : 1
}

const rcompareIdentifiers = (a, b) => compareIdentifiers(b, a)

module.exports = {
  compareIdentifiers,
  rcompareIdentifiers
}


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/re.js":
/*!***************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/re.js ***!
  \***************************************************************************************************/
/***/ ((module, exports, __webpack_require__) => {

const { MAX_SAFE_COMPONENT_LENGTH } = __webpack_require__(/*! ./constants */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/constants.js")
const debug = __webpack_require__(/*! ./debug */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/internal/debug.js")
exports = module.exports = {}

// The actual regexps go on exports.re
const re = exports.re = []
const src = exports.src = []
const t = exports.t = {}
let R = 0

const createToken = (name, value, isGlobal) => {
  const index = R++
  debug(index, value)
  t[name] = index
  src[index] = value
  re[index] = new RegExp(value, isGlobal ? 'g' : undefined)
}

// The following Regular Expressions can be used for tokenizing,
// validating, and parsing SemVer version strings.

// ## Numeric Identifier
// A single `0`, or a non-zero digit followed by zero or more digits.

createToken('NUMERICIDENTIFIER', '0|[1-9]\\d*')
createToken('NUMERICIDENTIFIERLOOSE', '[0-9]+')

// ## Non-numeric Identifier
// Zero or more digits, followed by a letter or hyphen, and then zero or
// more letters, digits, or hyphens.

createToken('NONNUMERICIDENTIFIER', '\\d*[a-zA-Z-][a-zA-Z0-9-]*')

// ## Main Version
// Three dot-separated numeric identifiers.

createToken('MAINVERSION', `(${src[t.NUMERICIDENTIFIER]})\\.` +
                   `(${src[t.NUMERICIDENTIFIER]})\\.` +
                   `(${src[t.NUMERICIDENTIFIER]})`)

createToken('MAINVERSIONLOOSE', `(${src[t.NUMERICIDENTIFIERLOOSE]})\\.` +
                        `(${src[t.NUMERICIDENTIFIERLOOSE]})\\.` +
                        `(${src[t.NUMERICIDENTIFIERLOOSE]})`)

// ## Pre-release Version Identifier
// A numeric identifier, or a non-numeric identifier.

createToken('PRERELEASEIDENTIFIER', `(?:${src[t.NUMERICIDENTIFIER]
}|${src[t.NONNUMERICIDENTIFIER]})`)

createToken('PRERELEASEIDENTIFIERLOOSE', `(?:${src[t.NUMERICIDENTIFIERLOOSE]
}|${src[t.NONNUMERICIDENTIFIER]})`)

// ## Pre-release Version
// Hyphen, followed by one or more dot-separated pre-release version
// identifiers.

createToken('PRERELEASE', `(?:-(${src[t.PRERELEASEIDENTIFIER]
}(?:\\.${src[t.PRERELEASEIDENTIFIER]})*))`)

createToken('PRERELEASELOOSE', `(?:-?(${src[t.PRERELEASEIDENTIFIERLOOSE]
}(?:\\.${src[t.PRERELEASEIDENTIFIERLOOSE]})*))`)

// ## Build Metadata Identifier
// Any combination of digits, letters, or hyphens.

createToken('BUILDIDENTIFIER', '[0-9A-Za-z-]+')

// ## Build Metadata
// Plus sign, followed by one or more period-separated build metadata
// identifiers.

createToken('BUILD', `(?:\\+(${src[t.BUILDIDENTIFIER]
}(?:\\.${src[t.BUILDIDENTIFIER]})*))`)

// ## Full Version String
// A main version, followed optionally by a pre-release version and
// build metadata.

// Note that the only major, minor, patch, and pre-release sections of
// the version string are capturing groups.  The build metadata is not a
// capturing group, because it should not ever be used in version
// comparison.

createToken('FULLPLAIN', `v?${src[t.MAINVERSION]
}${src[t.PRERELEASE]}?${
  src[t.BUILD]}?`)

createToken('FULL', `^${src[t.FULLPLAIN]}$`)

// like full, but allows v1.2.3 and =1.2.3, which people do sometimes.
// also, 1.0.0alpha1 (prerelease without the hyphen) which is pretty
// common in the npm registry.
createToken('LOOSEPLAIN', `[v=\\s]*${src[t.MAINVERSIONLOOSE]
}${src[t.PRERELEASELOOSE]}?${
  src[t.BUILD]}?`)

createToken('LOOSE', `^${src[t.LOOSEPLAIN]}$`)

createToken('GTLT', '((?:<|>)?=?)')

// Something like "2.*" or "1.2.x".
// Note that "x.x" is a valid xRange identifer, meaning "any version"
// Only the first item is strictly required.
createToken('XRANGEIDENTIFIERLOOSE', `${src[t.NUMERICIDENTIFIERLOOSE]}|x|X|\\*`)
createToken('XRANGEIDENTIFIER', `${src[t.NUMERICIDENTIFIER]}|x|X|\\*`)

createToken('XRANGEPLAIN', `[v=\\s]*(${src[t.XRANGEIDENTIFIER]})` +
                   `(?:\\.(${src[t.XRANGEIDENTIFIER]})` +
                   `(?:\\.(${src[t.XRANGEIDENTIFIER]})` +
                   `(?:${src[t.PRERELEASE]})?${
                     src[t.BUILD]}?` +
                   `)?)?`)

createToken('XRANGEPLAINLOOSE', `[v=\\s]*(${src[t.XRANGEIDENTIFIERLOOSE]})` +
                        `(?:\\.(${src[t.XRANGEIDENTIFIERLOOSE]})` +
                        `(?:\\.(${src[t.XRANGEIDENTIFIERLOOSE]})` +
                        `(?:${src[t.PRERELEASELOOSE]})?${
                          src[t.BUILD]}?` +
                        `)?)?`)

createToken('XRANGE', `^${src[t.GTLT]}\\s*${src[t.XRANGEPLAIN]}$`)
createToken('XRANGELOOSE', `^${src[t.GTLT]}\\s*${src[t.XRANGEPLAINLOOSE]}$`)

// Coercion.
// Extract anything that could conceivably be a part of a valid semver
createToken('COERCE', `${'(^|[^\\d])' +
              '(\\d{1,'}${MAX_SAFE_COMPONENT_LENGTH}})` +
              `(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?` +
              `(?:\\.(\\d{1,${MAX_SAFE_COMPONENT_LENGTH}}))?` +
              `(?:$|[^\\d])`)
createToken('COERCERTL', src[t.COERCE], true)

// Tilde ranges.
// Meaning is "reasonably at or greater than"
createToken('LONETILDE', '(?:~>?)')

createToken('TILDETRIM', `(\\s*)${src[t.LONETILDE]}\\s+`, true)
exports.tildeTrimReplace = '$1~'

createToken('TILDE', `^${src[t.LONETILDE]}${src[t.XRANGEPLAIN]}$`)
createToken('TILDELOOSE', `^${src[t.LONETILDE]}${src[t.XRANGEPLAINLOOSE]}$`)

// Caret ranges.
// Meaning is "at least and backwards compatible with"
createToken('LONECARET', '(?:\\^)')

createToken('CARETTRIM', `(\\s*)${src[t.LONECARET]}\\s+`, true)
exports.caretTrimReplace = '$1^'

createToken('CARET', `^${src[t.LONECARET]}${src[t.XRANGEPLAIN]}$`)
createToken('CARETLOOSE', `^${src[t.LONECARET]}${src[t.XRANGEPLAINLOOSE]}$`)

// A simple gt/lt/eq thing, or just "" to indicate "any version"
createToken('COMPARATORLOOSE', `^${src[t.GTLT]}\\s*(${src[t.LOOSEPLAIN]})$|^$`)
createToken('COMPARATOR', `^${src[t.GTLT]}\\s*(${src[t.FULLPLAIN]})$|^$`)

// An expression to strip any whitespace between the gtlt and the thing
// it modifies, so that `> 1.2.3` ==> `>1.2.3`
createToken('COMPARATORTRIM', `(\\s*)${src[t.GTLT]
}\\s*(${src[t.LOOSEPLAIN]}|${src[t.XRANGEPLAIN]})`, true)
exports.comparatorTrimReplace = '$1$2$3'

// Something like `1.2.3 - 1.2.4`
// Note that these all use the loose form, because they'll be
// checked against either the strict or loose comparator form
// later.
createToken('HYPHENRANGE', `^\\s*(${src[t.XRANGEPLAIN]})` +
                   `\\s+-\\s+` +
                   `(${src[t.XRANGEPLAIN]})` +
                   `\\s*$`)

createToken('HYPHENRANGELOOSE', `^\\s*(${src[t.XRANGEPLAINLOOSE]})` +
                        `\\s+-\\s+` +
                        `(${src[t.XRANGEPLAINLOOSE]})` +
                        `\\s*$`)

// Star ranges basically just allow anything at all.
createToken('STAR', '(<|>)?=?\\s*\\*')


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/gtr.js":
/*!**************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/gtr.js ***!
  \**************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

// Determine if version is greater than all the versions possible in the range.
const outside = __webpack_require__(/*! ./outside */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/outside.js")
const gtr = (version, range, options) => outside(version, range, '>', options)
module.exports = gtr


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/intersects.js":
/*!*********************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/intersects.js ***!
  \*********************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const Range = __webpack_require__(/*! ../classes/range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js")
const intersects = (r1, r2, options) => {
  r1 = new Range(r1, options)
  r2 = new Range(r2, options)
  return r1.intersects(r2)
}
module.exports = intersects


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/ltr.js":
/*!**************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/ltr.js ***!
  \**************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const outside = __webpack_require__(/*! ./outside */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/outside.js")
// Determine if version is less than all the versions possible in the range
const ltr = (version, range, options) => outside(version, range, '<', options)
module.exports = ltr


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/max-satisfying.js":
/*!*************************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/max-satisfying.js ***!
  \*************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const Range = __webpack_require__(/*! ../classes/range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js")

const maxSatisfying = (versions, range, options) => {
  let max = null
  let maxSV = null
  let rangeObj = null
  try {
    rangeObj = new Range(range, options)
  } catch (er) {
    return null
  }
  versions.forEach((v) => {
    if (rangeObj.test(v)) {
      // satisfies(v, range, options)
      if (!max || maxSV.compare(v) === -1) {
        // compare(max, v, true)
        max = v
        maxSV = new SemVer(max, options)
      }
    }
  })
  return max
}
module.exports = maxSatisfying


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/min-satisfying.js":
/*!*************************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/min-satisfying.js ***!
  \*************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const Range = __webpack_require__(/*! ../classes/range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js")
const minSatisfying = (versions, range, options) => {
  let min = null
  let minSV = null
  let rangeObj = null
  try {
    rangeObj = new Range(range, options)
  } catch (er) {
    return null
  }
  versions.forEach((v) => {
    if (rangeObj.test(v)) {
      // satisfies(v, range, options)
      if (!min || minSV.compare(v) === 1) {
        // compare(min, v, true)
        min = v
        minSV = new SemVer(min, options)
      }
    }
  })
  return min
}
module.exports = minSatisfying


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/min-version.js":
/*!**********************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/min-version.js ***!
  \**********************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const Range = __webpack_require__(/*! ../classes/range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js")
const gt = __webpack_require__(/*! ../functions/gt */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gt.js")

const minVersion = (range, loose) => {
  range = new Range(range, loose)

  let minver = new SemVer('0.0.0')
  if (range.test(minver)) {
    return minver
  }

  minver = new SemVer('0.0.0-0')
  if (range.test(minver)) {
    return minver
  }

  minver = null
  for (let i = 0; i < range.set.length; ++i) {
    const comparators = range.set[i]

    comparators.forEach((comparator) => {
      // Clone to avoid manipulating the comparator's semver object.
      const compver = new SemVer(comparator.semver.version)
      switch (comparator.operator) {
        case '>':
          if (compver.prerelease.length === 0) {
            compver.patch++
          } else {
            compver.prerelease.push(0)
          }
          compver.raw = compver.format()
          /* fallthrough */
        case '':
        case '>=':
          if (!minver || gt(minver, compver)) {
            minver = compver
          }
          break
        case '<':
        case '<=':
          /* Ignore maximum versions */
          break
        /* istanbul ignore next */
        default:
          throw new Error(`Unexpected operation: ${comparator.operator}`)
      }
    })
  }

  if (minver && range.test(minver)) {
    return minver
  }

  return null
}
module.exports = minVersion


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/outside.js":
/*!******************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/outside.js ***!
  \******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const SemVer = __webpack_require__(/*! ../classes/semver */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/semver.js")
const Comparator = __webpack_require__(/*! ../classes/comparator */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/comparator.js")
const {ANY} = Comparator
const Range = __webpack_require__(/*! ../classes/range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js")
const satisfies = __webpack_require__(/*! ../functions/satisfies */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/satisfies.js")
const gt = __webpack_require__(/*! ../functions/gt */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gt.js")
const lt = __webpack_require__(/*! ../functions/lt */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lt.js")
const lte = __webpack_require__(/*! ../functions/lte */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/lte.js")
const gte = __webpack_require__(/*! ../functions/gte */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/functions/gte.js")

const outside = (version, range, hilo, options) => {
  version = new SemVer(version, options)
  range = new Range(range, options)

  let gtfn, ltefn, ltfn, comp, ecomp
  switch (hilo) {
    case '>':
      gtfn = gt
      ltefn = lte
      ltfn = lt
      comp = '>'
      ecomp = '>='
      break
    case '<':
      gtfn = lt
      ltefn = gte
      ltfn = gt
      comp = '<'
      ecomp = '<='
      break
    default:
      throw new TypeError('Must provide a hilo val of "<" or ">"')
  }

  // If it satisifes the range it is not outside
  if (satisfies(version, range, options)) {
    return false
  }

  // From now on, variable terms are as if we're in "gtr" mode.
  // but note that everything is flipped for the "ltr" function.

  for (let i = 0; i < range.set.length; ++i) {
    const comparators = range.set[i]

    let high = null
    let low = null

    comparators.forEach((comparator) => {
      if (comparator.semver === ANY) {
        comparator = new Comparator('>=0.0.0')
      }
      high = high || comparator
      low = low || comparator
      if (gtfn(comparator.semver, high.semver, options)) {
        high = comparator
      } else if (ltfn(comparator.semver, low.semver, options)) {
        low = comparator
      }
    })

    // If the edge version comparator has a operator then our version
    // isn't outside it
    if (high.operator === comp || high.operator === ecomp) {
      return false
    }

    // If the lowest version comparator has an operator and our version
    // is less than it then it isn't higher than the range
    if ((!low.operator || low.operator === comp) &&
        ltefn(version, low.semver)) {
      return false
    } else if (low.operator === ecomp && ltfn(version, low.semver)) {
      return false
    }
  }
  return true
}

module.exports = outside


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/to-comparators.js":
/*!*************************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/to-comparators.js ***!
  \*************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const Range = __webpack_require__(/*! ../classes/range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js")

// Mostly just for testing and legacy API reasons
const toComparators = (range, options) =>
  new Range(range, options).set
    .map(comp => comp.map(c => c.value).join(' ').trim().split(' '))

module.exports = toComparators


/***/ }),

/***/ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/valid.js":
/*!****************************************************************************************************!*\
  !*** ./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/ranges/valid.js ***!
  \****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const Range = __webpack_require__(/*! ../classes/range */ "./.yarn/cache/semver-npm-7.1.3-1741df6bad-a7700fd039.zip/node_modules/semver/classes/range.js")
const validRange = (range, options) => {
  try {
    // Return '*' instead of '' so that truthiness works.
    // This will throw if it's invalid anyway
    return new Range(range, options).range || '*'
  } catch (er) {
    return null
  }
}
module.exports = validRange


/***/ }),

/***/ "./.yarn/cache/supports-color-npm-7.1.0-df2ba1e338-899480ac85.zip/node_modules/supports-color/index.js":
/*!*************************************************************************************************************!*\
  !*** ./.yarn/cache/supports-color-npm-7.1.0-df2ba1e338-899480ac85.zip/node_modules/supports-color/index.js ***!
  \*************************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

const os = __webpack_require__(/*! os */ "os");
const tty = __webpack_require__(/*! tty */ "tty");
const hasFlag = __webpack_require__(/*! has-flag */ "./.yarn/cache/has-flag-npm-4.0.0-32af9f0536-261a135703.zip/node_modules/has-flag/index.js");

const {env} = process;

let forceColor;
if (hasFlag('no-color') ||
	hasFlag('no-colors') ||
	hasFlag('color=false') ||
	hasFlag('color=never')) {
	forceColor = 0;
} else if (hasFlag('color') ||
	hasFlag('colors') ||
	hasFlag('color=true') ||
	hasFlag('color=always')) {
	forceColor = 1;
}

if ('FORCE_COLOR' in env) {
	if (env.FORCE_COLOR === 'true') {
		forceColor = 1;
	} else if (env.FORCE_COLOR === 'false') {
		forceColor = 0;
	} else {
		forceColor = env.FORCE_COLOR.length === 0 ? 1 : Math.min(parseInt(env.FORCE_COLOR, 10), 3);
	}
}

function translateLevel(level) {
	if (level === 0) {
		return false;
	}

	return {
		level,
		hasBasic: true,
		has256: level >= 2,
		has16m: level >= 3
	};
}

function supportsColor(haveStream, streamIsTTY) {
	if (forceColor === 0) {
		return 0;
	}

	if (hasFlag('color=16m') ||
		hasFlag('color=full') ||
		hasFlag('color=truecolor')) {
		return 3;
	}

	if (hasFlag('color=256')) {
		return 2;
	}

	if (haveStream && !streamIsTTY && forceColor === undefined) {
		return 0;
	}

	const min = forceColor || 0;

	if (env.TERM === 'dumb') {
		return min;
	}

	if (process.platform === 'win32') {
		// Windows 10 build 10586 is the first Windows release that supports 256 colors.
		// Windows 10 build 14931 is the first release that supports 16m/TrueColor.
		const osRelease = os.release().split('.');
		if (
			Number(osRelease[0]) >= 10 &&
			Number(osRelease[2]) >= 10586
		) {
			return Number(osRelease[2]) >= 14931 ? 3 : 2;
		}

		return 1;
	}

	if ('CI' in env) {
		if (['TRAVIS', 'CIRCLECI', 'APPVEYOR', 'GITLAB_CI'].some(sign => sign in env) || env.CI_NAME === 'codeship') {
			return 1;
		}

		return min;
	}

	if ('TEAMCITY_VERSION' in env) {
		return /^(9\.(0*[1-9]\d*)\.|\d{2,}\.)/.test(env.TEAMCITY_VERSION) ? 1 : 0;
	}

	if ('GITHUB_ACTIONS' in env) {
		return 1;
	}

	if (env.COLORTERM === 'truecolor') {
		return 3;
	}

	if ('TERM_PROGRAM' in env) {
		const version = parseInt((env.TERM_PROGRAM_VERSION || '').split('.')[0], 10);

		switch (env.TERM_PROGRAM) {
			case 'iTerm.app':
				return version >= 3 ? 3 : 2;
			case 'Apple_Terminal':
				return 2;
			// No default
		}
	}

	if (/-256(color)?$/i.test(env.TERM)) {
		return 2;
	}

	if (/^screen|^xterm|^vt100|^vt220|^rxvt|color|ansi|cygwin|linux/i.test(env.TERM)) {
		return 1;
	}

	if ('COLORTERM' in env) {
		return 1;
	}

	return min;
}

function getSupportLevel(stream) {
	const level = supportsColor(stream, stream && stream.isTTY);
	return translateLevel(level);
}

module.exports = {
	supportsColor: getSupportLevel,
	stdout: translateLevel(supportsColor(true, tty.isatty(1))),
	stderr: translateLevel(supportsColor(true, tty.isatty(2)))
};


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/index.js":
/*!***************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/index.js ***!
  \***************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


// high-level commands
exports.c = exports.create = __webpack_require__(/*! ./lib/create.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/create.js")
exports.r = exports.replace = __webpack_require__(/*! ./lib/replace.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/replace.js")
exports.t = exports.list = __webpack_require__(/*! ./lib/list.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/list.js")
exports.u = exports.update = __webpack_require__(/*! ./lib/update.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/update.js")
exports.x = exports.extract = __webpack_require__(/*! ./lib/extract.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/extract.js")

// classes
exports.Pack = __webpack_require__(/*! ./lib/pack.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pack.js")
exports.Unpack = __webpack_require__(/*! ./lib/unpack.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/unpack.js")
exports.Parse = __webpack_require__(/*! ./lib/parse.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/parse.js")
exports.ReadEntry = __webpack_require__(/*! ./lib/read-entry.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/read-entry.js")
exports.WriteEntry = __webpack_require__(/*! ./lib/write-entry.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/write-entry.js")
exports.Header = __webpack_require__(/*! ./lib/header.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/header.js")
exports.Pax = __webpack_require__(/*! ./lib/pax.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pax.js")
exports.types = __webpack_require__(/*! ./lib/types.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/types.js")


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/create.js":
/*!********************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/create.js ***!
  \********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


// tar -c
const hlo = __webpack_require__(/*! ./high-level-opt.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/high-level-opt.js")

const Pack = __webpack_require__(/*! ./pack.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pack.js")
const fs = __webpack_require__(/*! fs */ "fs")
const fsm = __webpack_require__(/*! fs-minipass */ "./.yarn/cache/fs-minipass-npm-2.1.0-501ef87306-1b8d128dae.zip/node_modules/fs-minipass/index.js")
const t = __webpack_require__(/*! ./list.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/list.js")
const path = __webpack_require__(/*! path */ "path")

const c = module.exports = (opt_, files, cb) => {
  if (typeof files === 'function')
    cb = files

  if (Array.isArray(opt_))
    files = opt_, opt_ = {}

  if (!files || !Array.isArray(files) || !files.length)
    throw new TypeError('no files or directories specified')

  files = Array.from(files)

  const opt = hlo(opt_)

  if (opt.sync && typeof cb === 'function')
    throw new TypeError('callback not supported for sync tar functions')

  if (!opt.file && typeof cb === 'function')
    throw new TypeError('callback only supported with file option')

  return opt.file && opt.sync ? createFileSync(opt, files)
    : opt.file ? createFile(opt, files, cb)
    : opt.sync ? createSync(opt, files)
    : create(opt, files)
}

const createFileSync = (opt, files) => {
  const p = new Pack.Sync(opt)
  const stream = new fsm.WriteStreamSync(opt.file, {
    mode: opt.mode || 0o666
  })
  p.pipe(stream)
  addFilesSync(p, files)
}

const createFile = (opt, files, cb) => {
  const p = new Pack(opt)
  const stream = new fsm.WriteStream(opt.file, {
    mode: opt.mode || 0o666
  })
  p.pipe(stream)

  const promise = new Promise((res, rej) => {
    stream.on('error', rej)
    stream.on('close', res)
    p.on('error', rej)
  })

  addFilesAsync(p, files)

  return cb ? promise.then(cb, cb) : promise
}

const addFilesSync = (p, files) => {
  files.forEach(file => {
    if (file.charAt(0) === '@')
      t({
        file: path.resolve(p.cwd, file.substr(1)),
        sync: true,
        noResume: true,
        onentry: entry => p.add(entry)
      })
    else
      p.add(file)
  })
  p.end()
}

const addFilesAsync = (p, files) => {
  while (files.length) {
    const file = files.shift()
    if (file.charAt(0) === '@')
      return t({
        file: path.resolve(p.cwd, file.substr(1)),
        noResume: true,
        onentry: entry => p.add(entry)
      }).then(_ => addFilesAsync(p, files))
    else
      p.add(file)
  }
  p.end()
}

const createSync = (opt, files) => {
  const p = new Pack.Sync(opt)
  addFilesSync(p, files)
  return p
}

const create = (opt, files) => {
  const p = new Pack(opt)
  addFilesAsync(p, files)
  return p
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/extract.js":
/*!*********************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/extract.js ***!
  \*********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


// tar -x
const hlo = __webpack_require__(/*! ./high-level-opt.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/high-level-opt.js")
const Unpack = __webpack_require__(/*! ./unpack.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/unpack.js")
const fs = __webpack_require__(/*! fs */ "fs")
const fsm = __webpack_require__(/*! fs-minipass */ "./.yarn/cache/fs-minipass-npm-2.1.0-501ef87306-1b8d128dae.zip/node_modules/fs-minipass/index.js")
const path = __webpack_require__(/*! path */ "path")

const x = module.exports = (opt_, files, cb) => {
  if (typeof opt_ === 'function')
    cb = opt_, files = null, opt_ = {}
  else if (Array.isArray(opt_))
    files = opt_, opt_ = {}

  if (typeof files === 'function')
    cb = files, files = null

  if (!files)
    files = []
  else
    files = Array.from(files)

  const opt = hlo(opt_)

  if (opt.sync && typeof cb === 'function')
    throw new TypeError('callback not supported for sync tar functions')

  if (!opt.file && typeof cb === 'function')
    throw new TypeError('callback only supported with file option')

  if (files.length)
    filesFilter(opt, files)

  return opt.file && opt.sync ? extractFileSync(opt)
    : opt.file ? extractFile(opt, cb)
    : opt.sync ? extractSync(opt)
    : extract(opt)
}

// construct a filter that limits the file entries listed
// include child entries if a dir is included
const filesFilter = (opt, files) => {
  const map = new Map(files.map(f => [f.replace(/\/+$/, ''), true]))
  const filter = opt.filter

  const mapHas = (file, r) => {
    const root = r || path.parse(file).root || '.'
    const ret = file === root ? false
      : map.has(file) ? map.get(file)
      : mapHas(path.dirname(file), root)

    map.set(file, ret)
    return ret
  }

  opt.filter = filter
    ? (file, entry) => filter(file, entry) && mapHas(file.replace(/\/+$/, ''))
    : file => mapHas(file.replace(/\/+$/, ''))
}

const extractFileSync = opt => {
  const u = new Unpack.Sync(opt)

  const file = opt.file
  let threw = true
  let fd
  const stat = fs.statSync(file)
  // This trades a zero-byte read() syscall for a stat
  // However, it will usually result in less memory allocation
  const readSize = opt.maxReadSize || 16*1024*1024
  const stream = new fsm.ReadStreamSync(file, {
    readSize: readSize,
    size: stat.size
  })
  stream.pipe(u)
}

const extractFile = (opt, cb) => {
  const u = new Unpack(opt)
  const readSize = opt.maxReadSize || 16*1024*1024

  const file = opt.file
  const p = new Promise((resolve, reject) => {
    u.on('error', reject)
    u.on('close', resolve)

    // This trades a zero-byte read() syscall for a stat
    // However, it will usually result in less memory allocation
    fs.stat(file, (er, stat) => {
      if (er)
        reject(er)
      else {
        const stream = new fsm.ReadStream(file, {
          readSize: readSize,
          size: stat.size
        })
        stream.on('error', reject)
        stream.pipe(u)
      }
    })
  })
  return cb ? p.then(cb, cb) : p
}

const extractSync = opt => {
  return new Unpack.Sync(opt)
}

const extract = opt => {
  return new Unpack(opt)
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/get-write-flag.js":
/*!****************************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/get-write-flag.js ***!
  \****************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

// Get the appropriate flag to use for creating files
// We use fmap on Windows platforms for files less than
// 512kb.  This is a fairly low limit, but avoids making
// things slower in some cases.  Since most of what this
// library is used for is extracting tarballs of many
// relatively small files in npm packages and the like,
// it can be a big boost on Windows platforms.
// Only supported in Node v12.9.0 and above.
const platform = process.env.__FAKE_PLATFORM__ || process.platform
const isWindows = platform === 'win32'
const fs = global.__FAKE_TESTING_FS__ || __webpack_require__(/*! fs */ "fs")

/* istanbul ignore next */
const { O_CREAT, O_TRUNC, O_WRONLY, UV_FS_O_FILEMAP = 0 } = fs.constants

const fMapEnabled = isWindows && !!UV_FS_O_FILEMAP
const fMapLimit = 512 * 1024
const fMapFlag = UV_FS_O_FILEMAP | O_TRUNC | O_CREAT | O_WRONLY
module.exports = !fMapEnabled ? () => 'w'
  : size => size < fMapLimit ? fMapFlag : 'w'


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/header.js":
/*!********************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/header.js ***!
  \********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

// parse a 512-byte header block to a data object, or vice-versa
// encode returns `true` if a pax extended header is needed, because
// the data could not be faithfully encoded in a simple header.
// (Also, check header.needPax to see if it needs a pax header.)

const types = __webpack_require__(/*! ./types.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/types.js")
const pathModule = __webpack_require__(/*! path */ "path").posix
const large = __webpack_require__(/*! ./large-numbers.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/large-numbers.js")

const SLURP = Symbol('slurp')
const TYPE = Symbol('type')

class Header {
  constructor (data, off, ex, gex) {
    this.cksumValid = false
    this.needPax = false
    this.nullBlock = false

    this.block = null
    this.path = null
    this.mode = null
    this.uid = null
    this.gid = null
    this.size = null
    this.mtime = null
    this.cksum = null
    this[TYPE] = '0'
    this.linkpath = null
    this.uname = null
    this.gname = null
    this.devmaj = 0
    this.devmin = 0
    this.atime = null
    this.ctime = null

    if (Buffer.isBuffer(data))
      this.decode(data, off || 0, ex, gex)
    else if (data)
      this.set(data)
  }

  decode (buf, off, ex, gex) {
    if (!off)
      off = 0

    if (!buf || !(buf.length >= off + 512))
      throw new Error('need 512 bytes for header')

    this.path = decString(buf, off, 100)
    this.mode = decNumber(buf, off + 100, 8)
    this.uid = decNumber(buf, off + 108, 8)
    this.gid = decNumber(buf, off + 116, 8)
    this.size = decNumber(buf, off + 124, 12)
    this.mtime = decDate(buf, off + 136, 12)
    this.cksum = decNumber(buf, off + 148, 12)

    // if we have extended or global extended headers, apply them now
    // See https://github.com/npm/node-tar/pull/187
    this[SLURP](ex)
    this[SLURP](gex, true)

    // old tar versions marked dirs as a file with a trailing /
    this[TYPE] = decString(buf, off + 156, 1)
    if (this[TYPE] === '')
      this[TYPE] = '0'
    if (this[TYPE] === '0' && this.path.substr(-1) === '/')
      this[TYPE] = '5'

    // tar implementations sometimes incorrectly put the stat(dir).size
    // as the size in the tarball, even though Directory entries are
    // not able to have any body at all.  In the very rare chance that
    // it actually DOES have a body, we weren't going to do anything with
    // it anyway, and it'll just be a warning about an invalid header.
    if (this[TYPE] === '5')
      this.size = 0

    this.linkpath = decString(buf, off + 157, 100)
    if (buf.slice(off + 257, off + 265).toString() === 'ustar\u000000') {
      this.uname = decString(buf, off + 265, 32)
      this.gname = decString(buf, off + 297, 32)
      this.devmaj = decNumber(buf, off + 329, 8)
      this.devmin = decNumber(buf, off + 337, 8)
      if (buf[off + 475] !== 0) {
        // definitely a prefix, definitely >130 chars.
        const prefix = decString(buf, off + 345, 155)
        this.path = prefix + '/' + this.path
      } else {
        const prefix = decString(buf, off + 345, 130)
        if (prefix)
          this.path = prefix + '/' + this.path
        this.atime = decDate(buf, off + 476, 12)
        this.ctime = decDate(buf, off + 488, 12)
      }
    }

    let sum = 8 * 0x20
    for (let i = off; i < off + 148; i++) {
      sum += buf[i]
    }
    for (let i = off + 156; i < off + 512; i++) {
      sum += buf[i]
    }
    this.cksumValid = sum === this.cksum
    if (this.cksum === null && sum === 8 * 0x20)
      this.nullBlock = true
  }

  [SLURP] (ex, global) {
    for (let k in ex) {
      // we slurp in everything except for the path attribute in
      // a global extended header, because that's weird.
      if (ex[k] !== null && ex[k] !== undefined &&
          !(global && k === 'path'))
        this[k] = ex[k]
    }
  }

  encode (buf, off) {
    if (!buf) {
      buf = this.block = Buffer.alloc(512)
      off = 0
    }

    if (!off)
      off = 0

    if (!(buf.length >= off + 512))
      throw new Error('need 512 bytes for header')

    const prefixSize = this.ctime || this.atime ? 130 : 155
    const split = splitPrefix(this.path || '', prefixSize)
    const path = split[0]
    const prefix = split[1]
    this.needPax = split[2]

    this.needPax = encString(buf, off, 100, path) || this.needPax
    this.needPax = encNumber(buf, off + 100, 8, this.mode) || this.needPax
    this.needPax = encNumber(buf, off + 108, 8, this.uid) || this.needPax
    this.needPax = encNumber(buf, off + 116, 8, this.gid) || this.needPax
    this.needPax = encNumber(buf, off + 124, 12, this.size) || this.needPax
    this.needPax = encDate(buf, off + 136, 12, this.mtime) || this.needPax
    buf[off + 156] = this[TYPE].charCodeAt(0)
    this.needPax = encString(buf, off + 157, 100, this.linkpath) || this.needPax
    buf.write('ustar\u000000', off + 257, 8)
    this.needPax = encString(buf, off + 265, 32, this.uname) || this.needPax
    this.needPax = encString(buf, off + 297, 32, this.gname) || this.needPax
    this.needPax = encNumber(buf, off + 329, 8, this.devmaj) || this.needPax
    this.needPax = encNumber(buf, off + 337, 8, this.devmin) || this.needPax
    this.needPax = encString(buf, off + 345, prefixSize, prefix) || this.needPax
    if (buf[off + 475] !== 0)
      this.needPax = encString(buf, off + 345, 155, prefix) || this.needPax
    else {
      this.needPax = encString(buf, off + 345, 130, prefix) || this.needPax
      this.needPax = encDate(buf, off + 476, 12, this.atime) || this.needPax
      this.needPax = encDate(buf, off + 488, 12, this.ctime) || this.needPax
    }

    let sum = 8 * 0x20
    for (let i = off; i < off + 148; i++) {
      sum += buf[i]
    }
    for (let i = off + 156; i < off + 512; i++) {
      sum += buf[i]
    }
    this.cksum = sum
    encNumber(buf, off + 148, 8, this.cksum)
    this.cksumValid = true

    return this.needPax
  }

  set (data) {
    for (let i in data) {
      if (data[i] !== null && data[i] !== undefined)
        this[i] = data[i]
    }
  }

  get type () {
    return types.name.get(this[TYPE]) || this[TYPE]
  }

  get typeKey () {
    return this[TYPE]
  }

  set type (type) {
    if (types.code.has(type))
      this[TYPE] = types.code.get(type)
    else
      this[TYPE] = type
  }
}

const splitPrefix = (p, prefixSize) => {
  const pathSize = 100
  let pp = p
  let prefix = ''
  let ret
  const root = pathModule.parse(p).root || '.'

  if (Buffer.byteLength(pp) < pathSize)
    ret = [pp, prefix, false]
  else {
    // first set prefix to the dir, and path to the base
    prefix = pathModule.dirname(pp)
    pp = pathModule.basename(pp)

    do {
      // both fit!
      if (Buffer.byteLength(pp) <= pathSize &&
          Buffer.byteLength(prefix) <= prefixSize)
        ret = [pp, prefix, false]

      // prefix fits in prefix, but path doesn't fit in path
      else if (Buffer.byteLength(pp) > pathSize &&
          Buffer.byteLength(prefix) <= prefixSize)
        ret = [pp.substr(0, pathSize - 1), prefix, true]

      else {
        // make path take a bit from prefix
        pp = pathModule.join(pathModule.basename(prefix), pp)
        prefix = pathModule.dirname(prefix)
      }
    } while (prefix !== root && !ret)

    // at this point, found no resolution, just truncate
    if (!ret)
      ret = [p.substr(0, pathSize - 1), '', true]
  }
  return ret
}

const decString = (buf, off, size) =>
  buf.slice(off, off + size).toString('utf8').replace(/\0.*/, '')

const decDate = (buf, off, size) =>
  numToDate(decNumber(buf, off, size))

const numToDate = num => num === null ? null : new Date(num * 1000)

const decNumber = (buf, off, size) =>
  buf[off] & 0x80 ? large.parse(buf.slice(off, off + size))
    : decSmallNumber(buf, off, size)

const nanNull = value => isNaN(value) ? null : value

const decSmallNumber = (buf, off, size) =>
  nanNull(parseInt(
    buf.slice(off, off + size)
      .toString('utf8').replace(/\0.*$/, '').trim(), 8))

// the maximum encodable as a null-terminated octal, by field size
const MAXNUM = {
  12: 0o77777777777,
  8 : 0o7777777
}

const encNumber = (buf, off, size, number) =>
  number === null ? false :
  number > MAXNUM[size] || number < 0
    ? (large.encode(number, buf.slice(off, off + size)), true)
    : (encSmallNumber(buf, off, size, number), false)

const encSmallNumber = (buf, off, size, number) =>
  buf.write(octalString(number, size), off, size, 'ascii')

const octalString = (number, size) =>
  padOctal(Math.floor(number).toString(8), size)

const padOctal = (string, size) =>
  (string.length === size - 1 ? string
  : new Array(size - string.length - 1).join('0') + string + ' ') + '\0'

const encDate = (buf, off, size, date) =>
  date === null ? false :
  encNumber(buf, off, size, date.getTime() / 1000)

// enough to fill the longest string we've got
const NULLS = new Array(156).join('\0')
// pad with nulls, return true if it's longer or non-ascii
const encString = (buf, off, size, string) =>
  string === null ? false :
  (buf.write(string + NULLS, off, size, 'utf8'),
   string.length !== Buffer.byteLength(string) || string.length > size)

module.exports = Header


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/high-level-opt.js":
/*!****************************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/high-level-opt.js ***!
  \****************************************************************************************************/
/***/ ((module) => {

"use strict";


// turn tar(1) style args like `C` into the more verbose things like `cwd`

const argmap = new Map([
  ['C', 'cwd'],
  ['f', 'file'],
  ['z', 'gzip'],
  ['P', 'preservePaths'],
  ['U', 'unlink'],
  ['strip-components', 'strip'],
  ['stripComponents', 'strip'],
  ['keep-newer', 'newer'],
  ['keepNewer', 'newer'],
  ['keep-newer-files', 'newer'],
  ['keepNewerFiles', 'newer'],
  ['k', 'keep'],
  ['keep-existing', 'keep'],
  ['keepExisting', 'keep'],
  ['m', 'noMtime'],
  ['no-mtime', 'noMtime'],
  ['p', 'preserveOwner'],
  ['L', 'follow'],
  ['h', 'follow']
])

const parse = module.exports = opt => opt ? Object.keys(opt).map(k => [
  argmap.has(k) ? argmap.get(k) : k, opt[k]
]).reduce((set, kv) => (set[kv[0]] = kv[1], set), Object.create(null)) : {}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/large-numbers.js":
/*!***************************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/large-numbers.js ***!
  \***************************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {

"use strict";

// Tar can encode large and negative numbers using a leading byte of
// 0xff for negative, and 0x80 for positive.

const encode = exports.encode = (num, buf) => {
  if (!Number.isSafeInteger(num))
    // The number is so large that javascript cannot represent it with integer
    // precision.
    throw Error('cannot encode number outside of javascript safe integer range')
  else if (num < 0)
    encodeNegative(num, buf)
  else
    encodePositive(num, buf)
  return buf
}

const encodePositive = (num, buf) => {
  buf[0] = 0x80

  for (var i = buf.length; i > 1; i--) {
    buf[i-1] = num & 0xff
    num = Math.floor(num / 0x100)
  }
}

const encodeNegative = (num, buf) => {
  buf[0] = 0xff
  var flipped = false
  num = num * -1
  for (var i = buf.length; i > 1; i--) {
    var byte = num & 0xff
    num = Math.floor(num / 0x100)
    if (flipped)
      buf[i-1] = onesComp(byte)
    else if (byte === 0)
      buf[i-1] = 0
    else {
      flipped = true
      buf[i-1] = twosComp(byte)
    }
  }
}

const parse = exports.parse = (buf) => {
  var post = buf[buf.length - 1]
  var pre = buf[0]
  var value;
  if (pre === 0x80)
    value = pos(buf.slice(1, buf.length))
  else if (pre === 0xff)
    value = twos(buf)
  else
    throw Error('invalid base256 encoding')

  if (!Number.isSafeInteger(value))
    // The number is so large that javascript cannot represent it with integer
    // precision.
    throw Error('parsed number outside of javascript safe integer range')

  return value
}

const twos = (buf) => {
  var len = buf.length
  var sum = 0
  var flipped = false
  for (var i = len - 1; i > -1; i--) {
    var byte = buf[i]
    var f
    if (flipped)
      f = onesComp(byte)
    else if (byte === 0)
      f = byte
    else {
      flipped = true
      f = twosComp(byte)
    }
    if (f !== 0)
      sum -= f * Math.pow(256, len - i - 1)
  }
  return sum
}

const pos = (buf) => {
  var len = buf.length
  var sum = 0
  for (var i = len - 1; i > -1; i--) {
    var byte = buf[i]
    if (byte !== 0)
      sum += byte * Math.pow(256, len - i - 1)
  }
  return sum
}

const onesComp = byte => (0xff ^ byte) & 0xff

const twosComp = byte => ((0xff ^ byte) + 1) & 0xff


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/list.js":
/*!******************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/list.js ***!
  \******************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


// XXX: This shares a lot in common with extract.js
// maybe some DRY opportunity here?

// tar -t
const hlo = __webpack_require__(/*! ./high-level-opt.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/high-level-opt.js")
const Parser = __webpack_require__(/*! ./parse.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/parse.js")
const fs = __webpack_require__(/*! fs */ "fs")
const fsm = __webpack_require__(/*! fs-minipass */ "./.yarn/cache/fs-minipass-npm-2.1.0-501ef87306-1b8d128dae.zip/node_modules/fs-minipass/index.js")
const path = __webpack_require__(/*! path */ "path")

const t = module.exports = (opt_, files, cb) => {
  if (typeof opt_ === 'function')
    cb = opt_, files = null, opt_ = {}
  else if (Array.isArray(opt_))
    files = opt_, opt_ = {}

  if (typeof files === 'function')
    cb = files, files = null

  if (!files)
    files = []
  else
    files = Array.from(files)

  const opt = hlo(opt_)

  if (opt.sync && typeof cb === 'function')
    throw new TypeError('callback not supported for sync tar functions')

  if (!opt.file && typeof cb === 'function')
    throw new TypeError('callback only supported with file option')

  if (files.length)
    filesFilter(opt, files)

  if (!opt.noResume)
    onentryFunction(opt)

  return opt.file && opt.sync ? listFileSync(opt)
    : opt.file ? listFile(opt, cb)
    : list(opt)
}

const onentryFunction = opt => {
  const onentry = opt.onentry
  opt.onentry = onentry ? e => {
    onentry(e)
    e.resume()
  } : e => e.resume()
}

// construct a filter that limits the file entries listed
// include child entries if a dir is included
const filesFilter = (opt, files) => {
  const map = new Map(files.map(f => [f.replace(/\/+$/, ''), true]))
  const filter = opt.filter

  const mapHas = (file, r) => {
    const root = r || path.parse(file).root || '.'
    const ret = file === root ? false
      : map.has(file) ? map.get(file)
      : mapHas(path.dirname(file), root)

    map.set(file, ret)
    return ret
  }

  opt.filter = filter
    ? (file, entry) => filter(file, entry) && mapHas(file.replace(/\/+$/, ''))
    : file => mapHas(file.replace(/\/+$/, ''))
}

const listFileSync = opt => {
  const p = list(opt)
  const file = opt.file
  let threw = true
  let fd
  try {
    const stat = fs.statSync(file)
    const readSize = opt.maxReadSize || 16*1024*1024
    if (stat.size < readSize) {
      p.end(fs.readFileSync(file))
    } else {
      let pos = 0
      const buf = Buffer.allocUnsafe(readSize)
      fd = fs.openSync(file, 'r')
      while (pos < stat.size) {
        let bytesRead = fs.readSync(fd, buf, 0, readSize, pos)
        pos += bytesRead
        p.write(buf.slice(0, bytesRead))
      }
      p.end()
    }
    threw = false
  } finally {
    if (threw && fd)
      try { fs.closeSync(fd) } catch (er) {}
  }
}

const listFile = (opt, cb) => {
  const parse = new Parser(opt)
  const readSize = opt.maxReadSize || 16*1024*1024

  const file = opt.file
  const p = new Promise((resolve, reject) => {
    parse.on('error', reject)
    parse.on('end', resolve)

    fs.stat(file, (er, stat) => {
      if (er)
        reject(er)
      else {
        const stream = new fsm.ReadStream(file, {
          readSize: readSize,
          size: stat.size
        })
        stream.on('error', reject)
        stream.pipe(parse)
      }
    })
  })
  return cb ? p.then(cb, cb) : p
}

const list = opt => new Parser(opt)


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/mkdir.js":
/*!*******************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/mkdir.js ***!
  \*******************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

// wrapper around mkdirp for tar's needs.

// TODO: This should probably be a class, not functionally
// passing around state in a gazillion args.

const mkdirp = __webpack_require__(/*! mkdirp */ "./.yarn/cache/mkdirp-npm-1.0.3-02fc91ac70-c5c46ae417.zip/node_modules/mkdirp/index.js")
const fs = __webpack_require__(/*! fs */ "fs")
const path = __webpack_require__(/*! path */ "path")
const chownr = __webpack_require__(/*! chownr */ "./.yarn/cache/chownr-npm-1.1.4-5bd400ab08-115648f8eb.zip/node_modules/chownr/chownr.js")

class SymlinkError extends Error {
  constructor (symlink, path) {
    super('Cannot extract through symbolic link')
    this.path = path
    this.symlink = symlink
  }

  get name () {
    return 'SylinkError'
  }
}

class CwdError extends Error {
  constructor (path, code) {
    super(code + ': Cannot cd into \'' + path + '\'')
    this.path = path
    this.code = code
  }

  get name () {
    return 'CwdError'
  }
}

const mkdir = module.exports = (dir, opt, cb) => {
  // if there's any overlap between mask and mode,
  // then we'll need an explicit chmod
  const umask = opt.umask
  const mode = opt.mode | 0o0700
  const needChmod = (mode & umask) !== 0

  const uid = opt.uid
  const gid = opt.gid
  const doChown = typeof uid === 'number' &&
    typeof gid === 'number' &&
    ( uid !== opt.processUid || gid !== opt.processGid )

  const preserve = opt.preserve
  const unlink = opt.unlink
  const cache = opt.cache
  const cwd = opt.cwd

  const done = (er, created) => {
    if (er)
      cb(er)
    else {
      cache.set(dir, true)
      if (created && doChown)
        chownr(created, uid, gid, er => done(er))
      else if (needChmod)
        fs.chmod(dir, mode, cb)
      else
        cb()
    }
  }

  if (cache && cache.get(dir) === true)
    return done()

  if (dir === cwd)
    return fs.stat(dir, (er, st) => {
      if (er || !st.isDirectory())
        er = new CwdError(dir, er && er.code || 'ENOTDIR')
      done(er)
    })

  if (preserve)
    return mkdirp(dir, {mode}).then(made => done(null, made), done)

  const sub = path.relative(cwd, dir)
  const parts = sub.split(/\/|\\/)
  mkdir_(cwd, parts, mode, cache, unlink, cwd, null, done)
}

const mkdir_ = (base, parts, mode, cache, unlink, cwd, created, cb) => {
  if (!parts.length)
    return cb(null, created)
  const p = parts.shift()
  const part = base + '/' + p
  if (cache.get(part))
    return mkdir_(part, parts, mode, cache, unlink, cwd, created, cb)
  fs.mkdir(part, mode, onmkdir(part, parts, mode, cache, unlink, cwd, created, cb))
}

const onmkdir = (part, parts, mode, cache, unlink, cwd, created, cb) => er => {
  if (er) {
    if (er.path && path.dirname(er.path) === cwd &&
        (er.code === 'ENOTDIR' || er.code === 'ENOENT'))
      return cb(new CwdError(cwd, er.code))

    fs.lstat(part, (statEr, st) => {
      if (statEr)
        cb(statEr)
      else if (st.isDirectory())
        mkdir_(part, parts, mode, cache, unlink, cwd, created, cb)
      else if (unlink)
        fs.unlink(part, er => {
          if (er)
            return cb(er)
          fs.mkdir(part, mode, onmkdir(part, parts, mode, cache, unlink, cwd, created, cb))
        })
      else if (st.isSymbolicLink())
        return cb(new SymlinkError(part, part + '/' + parts.join('/')))
      else
        cb(er)
    })
  } else {
    created = created || part
    mkdir_(part, parts, mode, cache, unlink, cwd, created, cb)
  }
}

const mkdirSync = module.exports.sync = (dir, opt) => {
  // if there's any overlap between mask and mode,
  // then we'll need an explicit chmod
  const umask = opt.umask
  const mode = opt.mode | 0o0700
  const needChmod = (mode & umask) !== 0

  const uid = opt.uid
  const gid = opt.gid
  const doChown = typeof uid === 'number' &&
    typeof gid === 'number' &&
    ( uid !== opt.processUid || gid !== opt.processGid )

  const preserve = opt.preserve
  const unlink = opt.unlink
  const cache = opt.cache
  const cwd = opt.cwd

  const done = (created) => {
    cache.set(dir, true)
    if (created && doChown)
      chownr.sync(created, uid, gid)
    if (needChmod)
      fs.chmodSync(dir, mode)
  }

  if (cache && cache.get(dir) === true)
    return done()

  if (dir === cwd) {
    let ok = false
    let code = 'ENOTDIR'
    try {
      ok = fs.statSync(dir).isDirectory()
    } catch (er) {
      code = er.code
    } finally {
      if (!ok)
        throw new CwdError(dir, code)
    }
    done()
    return
  }

  if (preserve)
    return done(mkdirp.sync(dir, mode))

  const sub = path.relative(cwd, dir)
  const parts = sub.split(/\/|\\/)
  let created = null
  for (let p = parts.shift(), part = cwd;
       p && (part += '/' + p);
       p = parts.shift()) {

    if (cache.get(part))
      continue

    try {
      fs.mkdirSync(part, mode)
      created = created || part
      cache.set(part, true)
    } catch (er) {
      if (er.path && path.dirname(er.path) === cwd &&
          (er.code === 'ENOTDIR' || er.code === 'ENOENT'))
        return new CwdError(cwd, er.code)

      const st = fs.lstatSync(part)
      if (st.isDirectory()) {
        cache.set(part, true)
        continue
      } else if (unlink) {
        fs.unlinkSync(part)
        fs.mkdirSync(part, mode)
        created = created || part
        cache.set(part, true)
        continue
      } else if (st.isSymbolicLink())
        return new SymlinkError(part, part + '/' + parts.join('/'))
    }
  }

  return done(created)
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/mode-fix.js":
/*!**********************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/mode-fix.js ***!
  \**********************************************************************************************/
/***/ ((module) => {

"use strict";

module.exports = (mode, isDir, portable) => {
  mode &= 0o7777

  // in portable mode, use the minimum reasonable umask
  // if this system creates files with 0o664 by default
  // (as some linux distros do), then we'll write the
  // archive with 0o644 instead.  Also, don't ever create
  // a file that is not readable/writable by the owner.
  if (portable) {
    mode = (mode | 0o600) &~0o22
  }

  // if dirs are readable, then they should be listable
  if (isDir) {
    if (mode & 0o400)
      mode |= 0o100
    if (mode & 0o40)
      mode |= 0o10
    if (mode & 0o4)
      mode |= 0o1
  }
  return mode
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pack.js":
/*!******************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pack.js ***!
  \******************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


// A readable tar stream creator
// Technically, this is a transform stream that you write paths into,
// and tar format comes out of.
// The `add()` method is like `write()` but returns this,
// and end() return `this` as well, so you can
// do `new Pack(opt).add('files').add('dir').end().pipe(output)
// You could also do something like:
// streamOfPaths().pipe(new Pack()).pipe(new fs.WriteStream('out.tar'))

class PackJob {
  constructor (path, absolute) {
    this.path = path || './'
    this.absolute = absolute
    this.entry = null
    this.stat = null
    this.readdir = null
    this.pending = false
    this.ignore = false
    this.piped = false
  }
}

const MiniPass = __webpack_require__(/*! minipass */ "./.yarn/cache/minipass-npm-3.1.1-b51d7e264d-cfcfd86adc.zip/node_modules/minipass/index.js")
const zlib = __webpack_require__(/*! minizlib */ "./.yarn/cache/minizlib-npm-2.1.0-9531be52b3-6a811aaa3a.zip/node_modules/minizlib/index.js")
const ReadEntry = __webpack_require__(/*! ./read-entry.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/read-entry.js")
const WriteEntry = __webpack_require__(/*! ./write-entry.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/write-entry.js")
const WriteEntrySync = WriteEntry.Sync
const WriteEntryTar = WriteEntry.Tar
const Yallist = __webpack_require__(/*! yallist */ "./.yarn/cache/yallist-npm-4.0.0-b493d9e907-343617202a.zip/node_modules/yallist/yallist.js")
const EOF = Buffer.alloc(1024)
const ONSTAT = Symbol('onStat')
const ENDED = Symbol('ended')
const QUEUE = Symbol('queue')
const CURRENT = Symbol('current')
const PROCESS = Symbol('process')
const PROCESSING = Symbol('processing')
const PROCESSJOB = Symbol('processJob')
const JOBS = Symbol('jobs')
const JOBDONE = Symbol('jobDone')
const ADDFSENTRY = Symbol('addFSEntry')
const ADDTARENTRY = Symbol('addTarEntry')
const STAT = Symbol('stat')
const READDIR = Symbol('readdir')
const ONREADDIR = Symbol('onreaddir')
const PIPE = Symbol('pipe')
const ENTRY = Symbol('entry')
const ENTRYOPT = Symbol('entryOpt')
const WRITEENTRYCLASS = Symbol('writeEntryClass')
const WRITE = Symbol('write')
const ONDRAIN = Symbol('ondrain')

const fs = __webpack_require__(/*! fs */ "fs")
const path = __webpack_require__(/*! path */ "path")
const warner = __webpack_require__(/*! ./warn-mixin.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/warn-mixin.js")

const Pack = warner(class Pack extends MiniPass {
  constructor (opt) {
    super(opt)
    opt = opt || Object.create(null)
    this.opt = opt
    this.file = opt.file || ''
    this.cwd = opt.cwd || process.cwd()
    this.maxReadSize = opt.maxReadSize
    this.preservePaths = !!opt.preservePaths
    this.strict = !!opt.strict
    this.noPax = !!opt.noPax
    this.prefix = (opt.prefix || '').replace(/(\\|\/)+$/, '')
    this.linkCache = opt.linkCache || new Map()
    this.statCache = opt.statCache || new Map()
    this.readdirCache = opt.readdirCache || new Map()

    this[WRITEENTRYCLASS] = WriteEntry
    if (typeof opt.onwarn === 'function')
      this.on('warn', opt.onwarn)

    this.portable = !!opt.portable
    this.zip = null
    if (opt.gzip) {
      if (typeof opt.gzip !== 'object')
        opt.gzip = {}
      if (this.portable)
        opt.gzip.portable = true
      this.zip = new zlib.Gzip(opt.gzip)
      this.zip.on('data', chunk => super.write(chunk))
      this.zip.on('end', _ => super.end())
      this.zip.on('drain', _ => this[ONDRAIN]())
      this.on('resume', _ => this.zip.resume())
    } else
      this.on('drain', this[ONDRAIN])

    this.noDirRecurse = !!opt.noDirRecurse
    this.follow = !!opt.follow
    this.noMtime = !!opt.noMtime
    this.mtime = opt.mtime || null

    this.filter = typeof opt.filter === 'function' ? opt.filter : _ => true

    this[QUEUE] = new Yallist
    this[JOBS] = 0
    this.jobs = +opt.jobs || 4
    this[PROCESSING] = false
    this[ENDED] = false
  }

  [WRITE] (chunk) {
    return super.write(chunk)
  }

  add (path) {
    this.write(path)
    return this
  }

  end (path) {
    if (path)
      this.write(path)
    this[ENDED] = true
    this[PROCESS]()
    return this
  }

  write (path) {
    if (this[ENDED])
      throw new Error('write after end')

    if (path instanceof ReadEntry)
      this[ADDTARENTRY](path)
    else
      this[ADDFSENTRY](path)
    return this.flowing
  }

  [ADDTARENTRY] (p) {
    const absolute = path.resolve(this.cwd, p.path)
    if (this.prefix)
      p.path = this.prefix + '/' + p.path.replace(/^\.(\/+|$)/, '')

    // in this case, we don't have to wait for the stat
    if (!this.filter(p.path, p))
      p.resume()
    else {
      const job = new PackJob(p.path, absolute, false)
      job.entry = new WriteEntryTar(p, this[ENTRYOPT](job))
      job.entry.on('end', _ => this[JOBDONE](job))
      this[JOBS] += 1
      this[QUEUE].push(job)
    }

    this[PROCESS]()
  }

  [ADDFSENTRY] (p) {
    const absolute = path.resolve(this.cwd, p)
    if (this.prefix)
      p = this.prefix + '/' + p.replace(/^\.(\/+|$)/, '')

    this[QUEUE].push(new PackJob(p, absolute))
    this[PROCESS]()
  }

  [STAT] (job) {
    job.pending = true
    this[JOBS] += 1
    const stat = this.follow ? 'stat' : 'lstat'
    fs[stat](job.absolute, (er, stat) => {
      job.pending = false
      this[JOBS] -= 1
      if (er)
        this.emit('error', er)
      else
        this[ONSTAT](job, stat)
    })
  }

  [ONSTAT] (job, stat) {
    this.statCache.set(job.absolute, stat)
    job.stat = stat

    // now we have the stat, we can filter it.
    if (!this.filter(job.path, stat))
      job.ignore = true

    this[PROCESS]()
  }

  [READDIR] (job) {
    job.pending = true
    this[JOBS] += 1
    fs.readdir(job.absolute, (er, entries) => {
      job.pending = false
      this[JOBS] -= 1
      if (er)
        return this.emit('error', er)
      this[ONREADDIR](job, entries)
    })
  }

  [ONREADDIR] (job, entries) {
    this.readdirCache.set(job.absolute, entries)
    job.readdir = entries
    this[PROCESS]()
  }

  [PROCESS] () {
    if (this[PROCESSING])
      return

    this[PROCESSING] = true
    for (let w = this[QUEUE].head;
         w !== null && this[JOBS] < this.jobs;
         w = w.next) {
      this[PROCESSJOB](w.value)
      if (w.value.ignore) {
        const p = w.next
        this[QUEUE].removeNode(w)
        w.next = p
      }
    }

    this[PROCESSING] = false

    if (this[ENDED] && !this[QUEUE].length && this[JOBS] === 0) {
      if (this.zip)
        this.zip.end(EOF)
      else {
        super.write(EOF)
        super.end()
      }
    }
  }

  get [CURRENT] () {
    return this[QUEUE] && this[QUEUE].head && this[QUEUE].head.value
  }

  [JOBDONE] (job) {
    this[QUEUE].shift()
    this[JOBS] -= 1
    this[PROCESS]()
  }

  [PROCESSJOB] (job) {
    if (job.pending)
      return

    if (job.entry) {
      if (job === this[CURRENT] && !job.piped)
        this[PIPE](job)
      return
    }

    if (!job.stat) {
      if (this.statCache.has(job.absolute))
        this[ONSTAT](job, this.statCache.get(job.absolute))
      else
        this[STAT](job)
    }
    if (!job.stat)
      return

    // filtered out!
    if (job.ignore)
      return

    if (!this.noDirRecurse && job.stat.isDirectory() && !job.readdir) {
      if (this.readdirCache.has(job.absolute))
        this[ONREADDIR](job, this.readdirCache.get(job.absolute))
      else
        this[READDIR](job)
      if (!job.readdir)
        return
    }

    // we know it doesn't have an entry, because that got checked above
    job.entry = this[ENTRY](job)
    if (!job.entry) {
      job.ignore = true
      return
    }

    if (job === this[CURRENT] && !job.piped)
      this[PIPE](job)
  }

  [ENTRYOPT] (job) {
    return {
      onwarn: (code, msg, data) => this.warn(code, msg, data),
      noPax: this.noPax,
      cwd: this.cwd,
      absolute: job.absolute,
      preservePaths: this.preservePaths,
      maxReadSize: this.maxReadSize,
      strict: this.strict,
      portable: this.portable,
      linkCache: this.linkCache,
      statCache: this.statCache,
      noMtime: this.noMtime,
      mtime: this.mtime
    }
  }

  [ENTRY] (job) {
    this[JOBS] += 1
    try {
      return new this[WRITEENTRYCLASS](job.path, this[ENTRYOPT](job))
        .on('end', () => this[JOBDONE](job))
        .on('error', er => this.emit('error', er))
    } catch (er) {
      this.emit('error', er)
    }
  }

  [ONDRAIN] () {
    if (this[CURRENT] && this[CURRENT].entry)
      this[CURRENT].entry.resume()
  }

  // like .pipe() but using super, because our write() is special
  [PIPE] (job) {
    job.piped = true

    if (job.readdir)
      job.readdir.forEach(entry => {
        const p = this.prefix ?
          job.path.slice(this.prefix.length + 1) || './'
          : job.path

        const base = p === './' ? '' : p.replace(/\/*$/, '/')
        this[ADDFSENTRY](base + entry)
      })

    const source = job.entry
    const zip = this.zip

    if (zip)
      source.on('data', chunk => {
        if (!zip.write(chunk))
          source.pause()
      })
    else
      source.on('data', chunk => {
        if (!super.write(chunk))
          source.pause()
      })
  }

  pause () {
    if (this.zip)
      this.zip.pause()
    return super.pause()
  }
})

class PackSync extends Pack {
  constructor (opt) {
    super(opt)
    this[WRITEENTRYCLASS] = WriteEntrySync
  }

  // pause/resume are no-ops in sync streams.
  pause () {}
  resume () {}

  [STAT] (job) {
    const stat = this.follow ? 'statSync' : 'lstatSync'
    this[ONSTAT](job, fs[stat](job.absolute))
  }

  [READDIR] (job, stat) {
    this[ONREADDIR](job, fs.readdirSync(job.absolute))
  }

  // gotta get it all in this tick
  [PIPE] (job) {
    const source = job.entry
    const zip = this.zip

    if (job.readdir)
      job.readdir.forEach(entry => {
        const p = this.prefix ?
          job.path.slice(this.prefix.length + 1) || './'
          : job.path

        const base = p === './' ? '' : p.replace(/\/*$/, '/')
        this[ADDFSENTRY](base + entry)
      })

    if (zip)
      source.on('data', chunk => {
        zip.write(chunk)
      })
    else
      source.on('data', chunk => {
        super[WRITE](chunk)
      })
  }
}

Pack.Sync = PackSync

module.exports = Pack


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/parse.js":
/*!*******************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/parse.js ***!
  \*******************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


// this[BUFFER] is the remainder of a chunk if we're waiting for
// the full 512 bytes of a header to come in.  We will Buffer.concat()
// it to the next write(), which is a mem copy, but a small one.
//
// this[QUEUE] is a Yallist of entries that haven't been emitted
// yet this can only get filled up if the user keeps write()ing after
// a write() returns false, or does a write() with more than one entry
//
// We don't buffer chunks, we always parse them and either create an
// entry, or push it into the active entry.  The ReadEntry class knows
// to throw data away if .ignore=true
//
// Shift entry off the buffer when it emits 'end', and emit 'entry' for
// the next one in the list.
//
// At any time, we're pushing body chunks into the entry at WRITEENTRY,
// and waiting for 'end' on the entry at READENTRY
//
// ignored entries get .resume() called on them straight away

const warner = __webpack_require__(/*! ./warn-mixin.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/warn-mixin.js")
const path = __webpack_require__(/*! path */ "path")
const Header = __webpack_require__(/*! ./header.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/header.js")
const EE = __webpack_require__(/*! events */ "events")
const Yallist = __webpack_require__(/*! yallist */ "./.yarn/cache/yallist-npm-4.0.0-b493d9e907-343617202a.zip/node_modules/yallist/yallist.js")
const maxMetaEntrySize = 1024 * 1024
const Entry = __webpack_require__(/*! ./read-entry.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/read-entry.js")
const Pax = __webpack_require__(/*! ./pax.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pax.js")
const zlib = __webpack_require__(/*! minizlib */ "./.yarn/cache/minizlib-npm-2.1.0-9531be52b3-6a811aaa3a.zip/node_modules/minizlib/index.js")

const gzipHeader = Buffer.from([0x1f, 0x8b])
const STATE = Symbol('state')
const WRITEENTRY = Symbol('writeEntry')
const READENTRY = Symbol('readEntry')
const NEXTENTRY = Symbol('nextEntry')
const PROCESSENTRY = Symbol('processEntry')
const EX = Symbol('extendedHeader')
const GEX = Symbol('globalExtendedHeader')
const META = Symbol('meta')
const EMITMETA = Symbol('emitMeta')
const BUFFER = Symbol('buffer')
const QUEUE = Symbol('queue')
const ENDED = Symbol('ended')
const EMITTEDEND = Symbol('emittedEnd')
const EMIT = Symbol('emit')
const UNZIP = Symbol('unzip')
const CONSUMECHUNK = Symbol('consumeChunk')
const CONSUMECHUNKSUB = Symbol('consumeChunkSub')
const CONSUMEBODY = Symbol('consumeBody')
const CONSUMEMETA = Symbol('consumeMeta')
const CONSUMEHEADER = Symbol('consumeHeader')
const CONSUMING = Symbol('consuming')
const BUFFERCONCAT = Symbol('bufferConcat')
const MAYBEEND = Symbol('maybeEnd')
const WRITING = Symbol('writing')
const ABORTED = Symbol('aborted')
const DONE = Symbol('onDone')
const SAW_VALID_ENTRY = Symbol('sawValidEntry')
const SAW_NULL_BLOCK = Symbol('sawNullBlock')
const SAW_EOF = Symbol('sawEOF')

const noop = _ => true

module.exports = warner(class Parser extends EE {
  constructor (opt) {
    opt = opt || {}
    super(opt)

    this.file = opt.file || ''

    // set to boolean false when an entry starts.  1024 bytes of \0
    // is technically a valid tarball, albeit a boring one.
    this[SAW_VALID_ENTRY] = null

    // these BADARCHIVE errors can't be detected early. listen on DONE.
    this.on(DONE, _ => {
      if (this[STATE] === 'begin' || this[SAW_VALID_ENTRY] === false) {
        // either less than 1 block of data, or all entries were invalid.
        // Either way, probably not even a tarball.
        this.warn('TAR_BAD_ARCHIVE', 'Unrecognized archive format')
      }
    })

    if (opt.ondone)
      this.on(DONE, opt.ondone)
    else
      this.on(DONE, _ => {
        this.emit('prefinish')
        this.emit('finish')
        this.emit('end')
        this.emit('close')
      })

    this.strict = !!opt.strict
    this.maxMetaEntrySize = opt.maxMetaEntrySize || maxMetaEntrySize
    this.filter = typeof opt.filter === 'function' ? opt.filter : noop

    // have to set this so that streams are ok piping into it
    this.writable = true
    this.readable = false

    this[QUEUE] = new Yallist()
    this[BUFFER] = null
    this[READENTRY] = null
    this[WRITEENTRY] = null
    this[STATE] = 'begin'
    this[META] = ''
    this[EX] = null
    this[GEX] = null
    this[ENDED] = false
    this[UNZIP] = null
    this[ABORTED] = false
    this[SAW_NULL_BLOCK] = false
    this[SAW_EOF] = false
    if (typeof opt.onwarn === 'function')
      this.on('warn', opt.onwarn)
    if (typeof opt.onentry === 'function')
      this.on('entry', opt.onentry)
  }

  [CONSUMEHEADER] (chunk, position) {
    if (this[SAW_VALID_ENTRY] === null)
      this[SAW_VALID_ENTRY] = false
    let header
    try {
      header = new Header(chunk, position, this[EX], this[GEX])
    } catch (er) {
      return this.warn('TAR_ENTRY_INVALID', er)
    }

    if (header.nullBlock) {
      if (this[SAW_NULL_BLOCK]) {
        this[SAW_EOF] = true
        // ending an archive with no entries.  pointless, but legal.
        if (this[STATE] === 'begin')
          this[STATE] = 'header'
        this[EMIT]('eof')
      } else {
        this[SAW_NULL_BLOCK] = true
        this[EMIT]('nullBlock')
      }
    } else {
      this[SAW_NULL_BLOCK] = false
      if (!header.cksumValid)
        this.warn('TAR_ENTRY_INVALID', 'checksum failure', {header})
      else if (!header.path)
        this.warn('TAR_ENTRY_INVALID', 'path is required', {header})
      else {
        const type = header.type
        if (/^(Symbolic)?Link$/.test(type) && !header.linkpath)
          this.warn('TAR_ENTRY_INVALID', 'linkpath required', {header})
        else if (!/^(Symbolic)?Link$/.test(type) && header.linkpath)
          this.warn('TAR_ENTRY_INVALID', 'linkpath forbidden', {header})
        else {
          const entry = this[WRITEENTRY] = new Entry(header, this[EX], this[GEX])

          // we do this for meta & ignored entries as well, because they
          // are still valid tar, or else we wouldn't know to ignore them
          if (!this[SAW_VALID_ENTRY]) {
            if (entry.remain) {
              // this might be the one!
              const onend = () => {
                if (!entry.invalid)
                  this[SAW_VALID_ENTRY] = true
              }
              entry.on('end', onend)
            } else {
              this[SAW_VALID_ENTRY] = true
            }
          }

          if (entry.meta) {
            if (entry.size > this.maxMetaEntrySize) {
              entry.ignore = true
              this[EMIT]('ignoredEntry', entry)
              this[STATE] = 'ignore'
              entry.resume()
            } else if (entry.size > 0) {
              this[META] = ''
              entry.on('data', c => this[META] += c)
              this[STATE] = 'meta'
            }
          } else {
            this[EX] = null
            entry.ignore = entry.ignore || !this.filter(entry.path, entry)

            if (entry.ignore) {
              // probably valid, just not something we care about
              this[EMIT]('ignoredEntry', entry)
              this[STATE] = entry.remain ? 'ignore' : 'header'
              entry.resume()
            } else {
              if (entry.remain)
                this[STATE] = 'body'
              else {
                this[STATE] = 'header'
                entry.end()
              }

              if (!this[READENTRY]) {
                this[QUEUE].push(entry)
                this[NEXTENTRY]()
              } else
                this[QUEUE].push(entry)
            }
          }
        }
      }
    }
  }

  [PROCESSENTRY] (entry) {
    let go = true

    if (!entry) {
      this[READENTRY] = null
      go = false
    } else if (Array.isArray(entry))
      this.emit.apply(this, entry)
    else {
      this[READENTRY] = entry
      this.emit('entry', entry)
      if (!entry.emittedEnd) {
        entry.on('end', _ => this[NEXTENTRY]())
        go = false
      }
    }

    return go
  }

  [NEXTENTRY] () {
    do {} while (this[PROCESSENTRY](this[QUEUE].shift()))

    if (!this[QUEUE].length) {
      // At this point, there's nothing in the queue, but we may have an
      // entry which is being consumed (readEntry).
      // If we don't, then we definitely can handle more data.
      // If we do, and either it's flowing, or it has never had any data
      // written to it, then it needs more.
      // The only other possibility is that it has returned false from a
      // write() call, so we wait for the next drain to continue.
      const re = this[READENTRY]
      const drainNow = !re || re.flowing || re.size === re.remain
      if (drainNow) {
        if (!this[WRITING])
          this.emit('drain')
      } else
        re.once('drain', _ => this.emit('drain'))
     }
  }

  [CONSUMEBODY] (chunk, position) {
    // write up to but no  more than writeEntry.blockRemain
    const entry = this[WRITEENTRY]
    const br = entry.blockRemain
    const c = (br >= chunk.length && position === 0) ? chunk
      : chunk.slice(position, position + br)

    entry.write(c)

    if (!entry.blockRemain) {
      this[STATE] = 'header'
      this[WRITEENTRY] = null
      entry.end()
    }

    return c.length
  }

  [CONSUMEMETA] (chunk, position) {
    const entry = this[WRITEENTRY]
    const ret = this[CONSUMEBODY](chunk, position)

    // if we finished, then the entry is reset
    if (!this[WRITEENTRY])
      this[EMITMETA](entry)

    return ret
  }

  [EMIT] (ev, data, extra) {
    if (!this[QUEUE].length && !this[READENTRY])
      this.emit(ev, data, extra)
    else
      this[QUEUE].push([ev, data, extra])
  }

  [EMITMETA] (entry) {
    this[EMIT]('meta', this[META])
    switch (entry.type) {
      case 'ExtendedHeader':
      case 'OldExtendedHeader':
        this[EX] = Pax.parse(this[META], this[EX], false)
        break

      case 'GlobalExtendedHeader':
        this[GEX] = Pax.parse(this[META], this[GEX], true)
        break

      case 'NextFileHasLongPath':
      case 'OldGnuLongPath':
        this[EX] = this[EX] || Object.create(null)
        this[EX].path = this[META].replace(/\0.*/, '')
        break

      case 'NextFileHasLongLinkpath':
        this[EX] = this[EX] || Object.create(null)
        this[EX].linkpath = this[META].replace(/\0.*/, '')
        break

      /* istanbul ignore next */
      default: throw new Error('unknown meta: ' + entry.type)
    }
  }

  abort (error) {
    this[ABORTED] = true
    this.emit('abort', error)
    // always throws, even in non-strict mode
    this.warn('TAR_ABORT', error, { recoverable: false })
  }

  write (chunk) {
    if (this[ABORTED])
      return

    // first write, might be gzipped
    if (this[UNZIP] === null && chunk) {
      if (this[BUFFER]) {
        chunk = Buffer.concat([this[BUFFER], chunk])
        this[BUFFER] = null
      }
      if (chunk.length < gzipHeader.length) {
        this[BUFFER] = chunk
        return true
      }
      for (let i = 0; this[UNZIP] === null && i < gzipHeader.length; i++) {
        if (chunk[i] !== gzipHeader[i])
          this[UNZIP] = false
      }
      if (this[UNZIP] === null) {
        const ended = this[ENDED]
        this[ENDED] = false
        this[UNZIP] = new zlib.Unzip()
        this[UNZIP].on('data', chunk => this[CONSUMECHUNK](chunk))
        this[UNZIP].on('error', er => this.abort(er))
        this[UNZIP].on('end', _ => {
          this[ENDED] = true
          this[CONSUMECHUNK]()
        })
        this[WRITING] = true
        const ret = this[UNZIP][ended ? 'end' : 'write' ](chunk)
        this[WRITING] = false
        return ret
      }
    }

    this[WRITING] = true
    if (this[UNZIP])
      this[UNZIP].write(chunk)
    else
      this[CONSUMECHUNK](chunk)
    this[WRITING] = false

    // return false if there's a queue, or if the current entry isn't flowing
    const ret =
      this[QUEUE].length ? false :
      this[READENTRY] ? this[READENTRY].flowing :
      true

    // if we have no queue, then that means a clogged READENTRY
    if (!ret && !this[QUEUE].length)
      this[READENTRY].once('drain', _ => this.emit('drain'))

    return ret
  }

  [BUFFERCONCAT] (c) {
    if (c && !this[ABORTED])
      this[BUFFER] = this[BUFFER] ? Buffer.concat([this[BUFFER], c]) : c
  }

  [MAYBEEND] () {
    if (this[ENDED] &&
        !this[EMITTEDEND] &&
        !this[ABORTED] &&
        !this[CONSUMING]) {
      this[EMITTEDEND] = true
      const entry = this[WRITEENTRY]
      if (entry && entry.blockRemain) {
        // truncated, likely a damaged file
        const have = this[BUFFER] ? this[BUFFER].length : 0
        this.warn('TAR_BAD_ARCHIVE', `Truncated input (needed ${
          entry.blockRemain} more bytes, only ${have} available)`, {entry})
        if (this[BUFFER])
          entry.write(this[BUFFER])
        entry.end()
      }
      this[EMIT](DONE)
    }
  }

  [CONSUMECHUNK] (chunk) {
    if (this[CONSUMING])
      this[BUFFERCONCAT](chunk)
    else if (!chunk && !this[BUFFER])
      this[MAYBEEND]()
    else {
      this[CONSUMING] = true
      if (this[BUFFER]) {
        this[BUFFERCONCAT](chunk)
        const c = this[BUFFER]
        this[BUFFER] = null
        this[CONSUMECHUNKSUB](c)
      } else {
        this[CONSUMECHUNKSUB](chunk)
      }

      while (this[BUFFER] &&
          this[BUFFER].length >= 512 &&
          !this[ABORTED] &&
          !this[SAW_EOF]) {
        const c = this[BUFFER]
        this[BUFFER] = null
        this[CONSUMECHUNKSUB](c)
      }
      this[CONSUMING] = false
    }

    if (!this[BUFFER] || this[ENDED])
      this[MAYBEEND]()
  }

  [CONSUMECHUNKSUB] (chunk) {
    // we know that we are in CONSUMING mode, so anything written goes into
    // the buffer.  Advance the position and put any remainder in the buffer.
    let position = 0
    let length = chunk.length
    while (position + 512 <= length && !this[ABORTED] && !this[SAW_EOF]) {
      switch (this[STATE]) {
        case 'begin':
        case 'header':
          this[CONSUMEHEADER](chunk, position)
          position += 512
          break

        case 'ignore':
        case 'body':
          position += this[CONSUMEBODY](chunk, position)
          break

        case 'meta':
          position += this[CONSUMEMETA](chunk, position)
          break

        /* istanbul ignore next */
        default:
          throw new Error('invalid state: ' + this[STATE])
      }
    }

    if (position < length) {
      if (this[BUFFER])
        this[BUFFER] = Buffer.concat([chunk.slice(position), this[BUFFER]])
      else
        this[BUFFER] = chunk.slice(position)
    }
  }

  end (chunk) {
    if (!this[ABORTED]) {
      if (this[UNZIP])
        this[UNZIP].end(chunk)
      else {
        this[ENDED] = true
        this.write(chunk)
      }
    }
  }
})


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/path-reservations.js":
/*!*******************************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/path-reservations.js ***!
  \*******************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

// A path exclusive reservation system
// reserve([list, of, paths], fn)
// When the fn is first in line for all its paths, it
// is called with a cb that clears the reservation.
//
// Used by async unpack to avoid clobbering paths in use,
// while still allowing maximal safe parallelization.

const assert = __webpack_require__(/*! assert */ "assert")

module.exports = () => {
  // path => [function or Set]
  // A Set object means a directory reservation
  // A fn is a direct reservation on that path
  const queues = new Map()

  // fn => {paths:[path,...], dirs:[path, ...]}
  const reservations = new Map()

  // return a set of parent dirs for a given path
  const { join } = __webpack_require__(/*! path */ "path")
  const getDirs = path =>
    join(path).split(/[\\\/]/).slice(0, -1).reduce((set, path) =>
      set.length ? set.concat(join(set[set.length-1], path)) : [path], [])

  // functions currently running
  const running = new Set()

  // return the queues for each path the function cares about
  // fn => {paths, dirs}
  const getQueues = fn => {
    const res = reservations.get(fn)
    /* istanbul ignore if - unpossible */
    if (!res)
      throw new Error('function does not have any path reservations')
    return {
      paths: res.paths.map(path => queues.get(path)),
      dirs: [...res.dirs].map(path => queues.get(path)),
    }
  }

  // check if fn is first in line for all its paths, and is
  // included in the first set for all its dir queues
  const check = fn => {
    const {paths, dirs} = getQueues(fn)
    return paths.every(q => q[0] === fn) &&
      dirs.every(q => q[0] instanceof Set && q[0].has(fn))
  }

  // run the function if it's first in line and not already running
  const run = fn => {
    if (running.has(fn) || !check(fn))
      return false
    running.add(fn)
    fn(() => clear(fn))
    return true
  }

  const clear = fn => {
    if (!running.has(fn))
      return false

    const { paths, dirs } = reservations.get(fn)
    const next = new Set()

    paths.forEach(path => {
      const q = queues.get(path)
      assert.equal(q[0], fn)
      if (q.length === 1)
        queues.delete(path)
      else {
        q.shift()
        if (typeof q[0] === 'function')
          next.add(q[0])
        else
          q[0].forEach(fn => next.add(fn))
      }
    })

    dirs.forEach(dir => {
      const q = queues.get(dir)
      assert(q[0] instanceof Set)
      if (q[0].size === 1 && q.length === 1) {
        queues.delete(dir)
      } else if (q[0].size === 1) {
        q.shift()

        // must be a function or else the Set would've been reused
        next.add(q[0])
      } else
        q[0].delete(fn)
    })
    running.delete(fn)

    next.forEach(fn => run(fn))
    return true
  }

  const reserve = (paths, fn) => {
    const dirs = new Set(
      paths.map(path => getDirs(path)).reduce((a, b) => a.concat(b))
    )
    reservations.set(fn, {dirs, paths})
    paths.forEach(path => {
      const q = queues.get(path)
      if (!q)
        queues.set(path, [fn])
      else
        q.push(fn)
    })
    dirs.forEach(dir => {
      const q = queues.get(dir)
      if (!q)
        queues.set(dir, [new Set([fn])])
      else if (q[q.length-1] instanceof Set)
        q[q.length-1].add(fn)
      else
        q.push(new Set([fn]))
    })

    return run(fn)
  }

  return { check, reserve }
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pax.js":
/*!*****************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pax.js ***!
  \*****************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

const Header = __webpack_require__(/*! ./header.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/header.js")
const path = __webpack_require__(/*! path */ "path")

class Pax {
  constructor (obj, global) {
    this.atime = obj.atime || null
    this.charset = obj.charset || null
    this.comment = obj.comment || null
    this.ctime = obj.ctime || null
    this.gid = obj.gid || null
    this.gname = obj.gname || null
    this.linkpath = obj.linkpath || null
    this.mtime = obj.mtime || null
    this.path = obj.path || null
    this.size = obj.size || null
    this.uid = obj.uid || null
    this.uname = obj.uname || null
    this.dev = obj.dev || null
    this.ino = obj.ino || null
    this.nlink = obj.nlink || null
    this.global = global || false
  }

  encode () {
    const body = this.encodeBody()
    if (body === '')
      return null

    const bodyLen = Buffer.byteLength(body)
    // round up to 512 bytes
    // add 512 for header
    const bufLen = 512 * Math.ceil(1 + bodyLen / 512)
    const buf = Buffer.allocUnsafe(bufLen)

    // 0-fill the header section, it might not hit every field
    for (let i = 0; i < 512; i++) {
      buf[i] = 0
    }

    new Header({
      // XXX split the path
      // then the path should be PaxHeader + basename, but less than 99,
      // prepend with the dirname
      path: ('PaxHeader/' + path.basename(this.path)).slice(0, 99),
      mode: this.mode || 0o644,
      uid: this.uid || null,
      gid: this.gid || null,
      size: bodyLen,
      mtime: this.mtime || null,
      type: this.global ? 'GlobalExtendedHeader' : 'ExtendedHeader',
      linkpath: '',
      uname: this.uname || '',
      gname: this.gname || '',
      devmaj: 0,
      devmin: 0,
      atime: this.atime || null,
      ctime: this.ctime || null
    }).encode(buf)

    buf.write(body, 512, bodyLen, 'utf8')

    // null pad after the body
    for (let i = bodyLen + 512; i < buf.length; i++) {
      buf[i] = 0
    }

    return buf
  }

  encodeBody () {
    return (
      this.encodeField('path') +
      this.encodeField('ctime') +
      this.encodeField('atime') +
      this.encodeField('dev') +
      this.encodeField('ino') +
      this.encodeField('nlink') +
      this.encodeField('charset') +
      this.encodeField('comment') +
      this.encodeField('gid') +
      this.encodeField('gname') +
      this.encodeField('linkpath') +
      this.encodeField('mtime') +
      this.encodeField('size') +
      this.encodeField('uid') +
      this.encodeField('uname')
    )
  }

  encodeField (field) {
    if (this[field] === null || this[field] === undefined)
      return ''
    const v = this[field] instanceof Date ? this[field].getTime() / 1000
      : this[field]
    const s = ' ' +
      (field === 'dev' || field === 'ino' || field === 'nlink'
       ? 'SCHILY.' : '') +
      field + '=' + v + '\n'
    const byteLen = Buffer.byteLength(s)
    // the digits includes the length of the digits in ascii base-10
    // so if it's 9 characters, then adding 1 for the 9 makes it 10
    // which makes it 11 chars.
    let digits = Math.floor(Math.log(byteLen) / Math.log(10)) + 1
    if (byteLen + digits >= Math.pow(10, digits))
      digits += 1
    const len = digits + byteLen
    return len + s
  }
}

Pax.parse = (string, ex, g) => new Pax(merge(parseKV(string), ex), g)

const merge = (a, b) =>
  b ? Object.keys(a).reduce((s, k) => (s[k] = a[k], s), b) : a

const parseKV = string =>
  string
    .replace(/\n$/, '')
    .split('\n')
    .reduce(parseKVLine, Object.create(null))

const parseKVLine = (set, line) => {
  const n = parseInt(line, 10)

  // XXX Values with \n in them will fail this.
  // Refactor to not be a naive line-by-line parse.
  if (n !== Buffer.byteLength(line) + 1)
    return set

  line = line.substr((n + ' ').length)
  const kv = line.split('=')
  const k = kv.shift().replace(/^SCHILY\.(dev|ino|nlink)/, '$1')
  if (!k)
    return set

  const v = kv.join('=')
  set[k] = /^([A-Z]+\.)?([mac]|birth|creation)time$/.test(k)
    ?  new Date(v * 1000)
    : /^[0-9]+$/.test(v) ? +v
    : v
  return set
}

module.exports = Pax


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/read-entry.js":
/*!************************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/read-entry.js ***!
  \************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

const types = __webpack_require__(/*! ./types.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/types.js")
const MiniPass = __webpack_require__(/*! minipass */ "./.yarn/cache/minipass-npm-3.1.1-b51d7e264d-cfcfd86adc.zip/node_modules/minipass/index.js")

const SLURP = Symbol('slurp')
module.exports = class ReadEntry extends MiniPass {
  constructor (header, ex, gex) {
    super()
    // read entries always start life paused.  this is to avoid the
    // situation where Minipass's auto-ending empty streams results
    // in an entry ending before we're ready for it.
    this.pause()
    this.extended = ex
    this.globalExtended = gex
    this.header = header
    this.startBlockSize = 512 * Math.ceil(header.size / 512)
    this.blockRemain = this.startBlockSize
    this.remain = header.size
    this.type = header.type
    this.meta = false
    this.ignore = false
    switch (this.type) {
      case 'File':
      case 'OldFile':
      case 'Link':
      case 'SymbolicLink':
      case 'CharacterDevice':
      case 'BlockDevice':
      case 'Directory':
      case 'FIFO':
      case 'ContiguousFile':
      case 'GNUDumpDir':
        break

      case 'NextFileHasLongLinkpath':
      case 'NextFileHasLongPath':
      case 'OldGnuLongPath':
      case 'GlobalExtendedHeader':
      case 'ExtendedHeader':
      case 'OldExtendedHeader':
        this.meta = true
        break

      // NOTE: gnutar and bsdtar treat unrecognized types as 'File'
      // it may be worth doing the same, but with a warning.
      default:
        this.ignore = true
    }

    this.path = header.path
    this.mode = header.mode
    if (this.mode)
      this.mode = this.mode & 0o7777
    this.uid = header.uid
    this.gid = header.gid
    this.uname = header.uname
    this.gname = header.gname
    this.size = header.size
    this.mtime = header.mtime
    this.atime = header.atime
    this.ctime = header.ctime
    this.linkpath = header.linkpath
    this.uname = header.uname
    this.gname = header.gname

    if (ex) this[SLURP](ex)
    if (gex) this[SLURP](gex, true)
  }

  write (data) {
    const writeLen = data.length
    if (writeLen > this.blockRemain)
      throw new Error('writing more to entry than is appropriate')

    const r = this.remain
    const br = this.blockRemain
    this.remain = Math.max(0, r - writeLen)
    this.blockRemain = Math.max(0, br - writeLen)
    if (this.ignore)
      return true

    if (r >= writeLen)
      return super.write(data)

    // r < writeLen
    return super.write(data.slice(0, r))
  }

  [SLURP] (ex, global) {
    for (let k in ex) {
      // we slurp in everything except for the path attribute in
      // a global extended header, because that's weird.
      if (ex[k] !== null && ex[k] !== undefined &&
          !(global && k === 'path'))
        this[k] = ex[k]
    }
  }
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/replace.js":
/*!*********************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/replace.js ***!
  \*********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


// tar -r
const hlo = __webpack_require__(/*! ./high-level-opt.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/high-level-opt.js")
const Pack = __webpack_require__(/*! ./pack.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pack.js")
const Parse = __webpack_require__(/*! ./parse.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/parse.js")
const fs = __webpack_require__(/*! fs */ "fs")
const fsm = __webpack_require__(/*! fs-minipass */ "./.yarn/cache/fs-minipass-npm-2.1.0-501ef87306-1b8d128dae.zip/node_modules/fs-minipass/index.js")
const t = __webpack_require__(/*! ./list.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/list.js")
const path = __webpack_require__(/*! path */ "path")

// starting at the head of the file, read a Header
// If the checksum is invalid, that's our position to start writing
// If it is, jump forward by the specified size (round up to 512)
// and try again.
// Write the new Pack stream starting there.

const Header = __webpack_require__(/*! ./header.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/header.js")

const r = module.exports = (opt_, files, cb) => {
  const opt = hlo(opt_)

  if (!opt.file)
    throw new TypeError('file is required')

  if (opt.gzip)
    throw new TypeError('cannot append to compressed archives')

  if (!files || !Array.isArray(files) || !files.length)
    throw new TypeError('no files or directories specified')

  files = Array.from(files)

  return opt.sync ? replaceSync(opt, files)
    : replace(opt, files, cb)
}

const replaceSync = (opt, files) => {
  const p = new Pack.Sync(opt)

  let threw = true
  let fd
  let position

  try {
    try {
      fd = fs.openSync(opt.file, 'r+')
    } catch (er) {
      if (er.code === 'ENOENT')
        fd = fs.openSync(opt.file, 'w+')
      else
        throw er
    }

    const st = fs.fstatSync(fd)
    const headBuf = Buffer.alloc(512)

    POSITION: for (position = 0; position < st.size; position += 512) {
      for (let bufPos = 0, bytes = 0; bufPos < 512; bufPos += bytes) {
        bytes = fs.readSync(
          fd, headBuf, bufPos, headBuf.length - bufPos, position + bufPos
        )

        if (position === 0 && headBuf[0] === 0x1f && headBuf[1] === 0x8b)
          throw new Error('cannot append to compressed archives')

        if (!bytes)
          break POSITION
      }

      let h = new Header(headBuf)
      if (!h.cksumValid)
        break
      let entryBlockSize = 512 * Math.ceil(h.size / 512)
      if (position + entryBlockSize + 512 > st.size)
        break
      // the 512 for the header we just parsed will be added as well
      // also jump ahead all the blocks for the body
      position += entryBlockSize
      if (opt.mtimeCache)
        opt.mtimeCache.set(h.path, h.mtime)
    }
    threw = false

    streamSync(opt, p, position, fd, files)
  } finally {
    if (threw)
      try { fs.closeSync(fd) } catch (er) {}
  }
}

const streamSync = (opt, p, position, fd, files) => {
  const stream = new fsm.WriteStreamSync(opt.file, {
    fd: fd,
    start: position
  })
  p.pipe(stream)
  addFilesSync(p, files)
}

const replace = (opt, files, cb) => {
  files = Array.from(files)
  const p = new Pack(opt)

  const getPos = (fd, size, cb_) => {
    const cb = (er, pos) => {
      if (er)
        fs.close(fd, _ => cb_(er))
      else
        cb_(null, pos)
    }

    let position = 0
    if (size === 0)
      return cb(null, 0)

    let bufPos = 0
    const headBuf = Buffer.alloc(512)
    const onread = (er, bytes) => {
      if (er)
        return cb(er)
      bufPos += bytes
      if (bufPos < 512 && bytes)
        return fs.read(
          fd, headBuf, bufPos, headBuf.length - bufPos,
          position + bufPos, onread
        )

      if (position === 0 && headBuf[0] === 0x1f && headBuf[1] === 0x8b)
        return cb(new Error('cannot append to compressed archives'))

      // truncated header
      if (bufPos < 512)
        return cb(null, position)

      const h = new Header(headBuf)
      if (!h.cksumValid)
        return cb(null, position)

      const entryBlockSize = 512 * Math.ceil(h.size / 512)
      if (position + entryBlockSize + 512 > size)
        return cb(null, position)

      position += entryBlockSize + 512
      if (position >= size)
        return cb(null, position)

      if (opt.mtimeCache)
        opt.mtimeCache.set(h.path, h.mtime)
      bufPos = 0
      fs.read(fd, headBuf, 0, 512, position, onread)
    }
    fs.read(fd, headBuf, 0, 512, position, onread)
  }

  const promise = new Promise((resolve, reject) => {
    p.on('error', reject)
    let flag = 'r+'
    const onopen = (er, fd) => {
      if (er && er.code === 'ENOENT' && flag === 'r+') {
        flag = 'w+'
        return fs.open(opt.file, flag, onopen)
      }

      if (er)
        return reject(er)

      fs.fstat(fd, (er, st) => {
        if (er)
          return reject(er)
        getPos(fd, st.size, (er, position) => {
          if (er)
            return reject(er)
          const stream = new fsm.WriteStream(opt.file, {
            fd: fd,
            start: position
          })
          p.pipe(stream)
          stream.on('error', reject)
          stream.on('close', resolve)
          addFilesAsync(p, files)
        })
      })
    }
    fs.open(opt.file, flag, onopen)
  })

  return cb ? promise.then(cb, cb) : promise
}

const addFilesSync = (p, files) => {
  files.forEach(file => {
    if (file.charAt(0) === '@')
      t({
        file: path.resolve(p.cwd, file.substr(1)),
        sync: true,
        noResume: true,
        onentry: entry => p.add(entry)
      })
    else
      p.add(file)
  })
  p.end()
}

const addFilesAsync = (p, files) => {
  while (files.length) {
    const file = files.shift()
    if (file.charAt(0) === '@')
      return t({
        file: path.resolve(p.cwd, file.substr(1)),
        noResume: true,
        onentry: entry => p.add(entry)
      }).then(_ => addFilesAsync(p, files))
    else
      p.add(file)
  }
  p.end()
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/types.js":
/*!*******************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/types.js ***!
  \*******************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {

"use strict";

// map types from key to human-friendly name
exports.name = new Map([
  ['0', 'File'],
  // same as File
  ['', 'OldFile'],
  ['1', 'Link'],
  ['2', 'SymbolicLink'],
  // Devices and FIFOs aren't fully supported
  // they are parsed, but skipped when unpacking
  ['3', 'CharacterDevice'],
  ['4', 'BlockDevice'],
  ['5', 'Directory'],
  ['6', 'FIFO'],
  // same as File
  ['7', 'ContiguousFile'],
  // pax headers
  ['g', 'GlobalExtendedHeader'],
  ['x', 'ExtendedHeader'],
  // vendor-specific stuff
  // skip
  ['A', 'SolarisACL'],
  // like 5, but with data, which should be skipped
  ['D', 'GNUDumpDir'],
  // metadata only, skip
  ['I', 'Inode'],
  // data = link path of next file
  ['K', 'NextFileHasLongLinkpath'],
  // data = path of next file
  ['L', 'NextFileHasLongPath'],
  // skip
  ['M', 'ContinuationFile'],
  // like L
  ['N', 'OldGnuLongPath'],
  // skip
  ['S', 'SparseFile'],
  // skip
  ['V', 'TapeVolumeHeader'],
  // like x
  ['X', 'OldExtendedHeader']
])

// map the other direction
exports.code = new Map(Array.from(exports.name).map(kv => [kv[1], kv[0]]))


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/unpack.js":
/*!********************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/unpack.js ***!
  \********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


// the PEND/UNPEND stuff tracks whether we're ready to emit end/close yet.
// but the path reservations are required to avoid race conditions where
// parallelized unpack ops may mess with one another, due to dependencies
// (like a Link depending on its target) or destructive operations (like
// clobbering an fs object to create one of a different type.)

const assert = __webpack_require__(/*! assert */ "assert")
const EE = __webpack_require__(/*! events */ "events").EventEmitter
const Parser = __webpack_require__(/*! ./parse.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/parse.js")
const fs = __webpack_require__(/*! fs */ "fs")
const fsm = __webpack_require__(/*! fs-minipass */ "./.yarn/cache/fs-minipass-npm-2.1.0-501ef87306-1b8d128dae.zip/node_modules/fs-minipass/index.js")
const path = __webpack_require__(/*! path */ "path")
const mkdir = __webpack_require__(/*! ./mkdir.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/mkdir.js")
const mkdirSync = mkdir.sync
const wc = __webpack_require__(/*! ./winchars.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/winchars.js")
const pathReservations = __webpack_require__(/*! ./path-reservations.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/path-reservations.js")

const ONENTRY = Symbol('onEntry')
const CHECKFS = Symbol('checkFs')
const CHECKFS2 = Symbol('checkFs2')
const ISREUSABLE = Symbol('isReusable')
const MAKEFS = Symbol('makeFs')
const FILE = Symbol('file')
const DIRECTORY = Symbol('directory')
const LINK = Symbol('link')
const SYMLINK = Symbol('symlink')
const HARDLINK = Symbol('hardlink')
const UNSUPPORTED = Symbol('unsupported')
const UNKNOWN = Symbol('unknown')
const CHECKPATH = Symbol('checkPath')
const MKDIR = Symbol('mkdir')
const ONERROR = Symbol('onError')
const PENDING = Symbol('pending')
const PEND = Symbol('pend')
const UNPEND = Symbol('unpend')
const ENDED = Symbol('ended')
const MAYBECLOSE = Symbol('maybeClose')
const SKIP = Symbol('skip')
const DOCHOWN = Symbol('doChown')
const UID = Symbol('uid')
const GID = Symbol('gid')
const crypto = __webpack_require__(/*! crypto */ "crypto")
const getFlag = __webpack_require__(/*! ./get-write-flag.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/get-write-flag.js")

/* istanbul ignore next */
const neverCalled = () => {
  throw new Error('sync function called cb somehow?!?')
}

// Unlinks on Windows are not atomic.
//
// This means that if you have a file entry, followed by another
// file entry with an identical name, and you cannot re-use the file
// (because it's a hardlink, or because unlink:true is set, or it's
// Windows, which does not have useful nlink values), then the unlink
// will be committed to the disk AFTER the new file has been written
// over the old one, deleting the new file.
//
// To work around this, on Windows systems, we rename the file and then
// delete the renamed file.  It's a sloppy kludge, but frankly, I do not
// know of a better way to do this, given windows' non-atomic unlink
// semantics.
//
// See: https://github.com/npm/node-tar/issues/183
/* istanbul ignore next */
const unlinkFile = (path, cb) => {
  if (process.platform !== 'win32')
    return fs.unlink(path, cb)

  const name = path + '.DELETE.' + crypto.randomBytes(16).toString('hex')
  fs.rename(path, name, er => {
    if (er)
      return cb(er)
    fs.unlink(name, cb)
  })
}

/* istanbul ignore next */
const unlinkFileSync = path => {
  if (process.platform !== 'win32')
    return fs.unlinkSync(path)

  const name = path + '.DELETE.' + crypto.randomBytes(16).toString('hex')
  fs.renameSync(path, name)
  fs.unlinkSync(name)
}

// this.gid, entry.gid, this.processUid
const uint32 = (a, b, c) =>
  a === a >>> 0 ? a
  : b === b >>> 0 ? b
  : c

class Unpack extends Parser {
  constructor (opt) {
    if (!opt)
      opt = {}

    opt.ondone = _ => {
      this[ENDED] = true
      this[MAYBECLOSE]()
    }

    super(opt)

    this.reservations = pathReservations()

    this.transform = typeof opt.transform === 'function' ? opt.transform : null

    this.writable = true
    this.readable = false

    this[PENDING] = 0
    this[ENDED] = false

    this.dirCache = opt.dirCache || new Map()

    if (typeof opt.uid === 'number' || typeof opt.gid === 'number') {
      // need both or neither
      if (typeof opt.uid !== 'number' || typeof opt.gid !== 'number')
        throw new TypeError('cannot set owner without number uid and gid')
      if (opt.preserveOwner)
        throw new TypeError(
          'cannot preserve owner in archive and also set owner explicitly')
      this.uid = opt.uid
      this.gid = opt.gid
      this.setOwner = true
    } else {
      this.uid = null
      this.gid = null
      this.setOwner = false
    }

    // default true for root
    if (opt.preserveOwner === undefined && typeof opt.uid !== 'number')
      this.preserveOwner = process.getuid && process.getuid() === 0
    else
      this.preserveOwner = !!opt.preserveOwner

    this.processUid = (this.preserveOwner || this.setOwner) && process.getuid ?
      process.getuid() : null
    this.processGid = (this.preserveOwner || this.setOwner) && process.getgid ?
      process.getgid() : null

    // mostly just for testing, but useful in some cases.
    // Forcibly trigger a chown on every entry, no matter what
    this.forceChown = opt.forceChown === true

    // turn ><?| in filenames into 0xf000-higher encoded forms
    this.win32 = !!opt.win32 || process.platform === 'win32'

    // do not unpack over files that are newer than what's in the archive
    this.newer = !!opt.newer

    // do not unpack over ANY files
    this.keep = !!opt.keep

    // do not set mtime/atime of extracted entries
    this.noMtime = !!opt.noMtime

    // allow .., absolute path entries, and unpacking through symlinks
    // without this, warn and skip .., relativize absolutes, and error
    // on symlinks in extraction path
    this.preservePaths = !!opt.preservePaths

    // unlink files and links before writing. This breaks existing hard
    // links, and removes symlink directories rather than erroring
    this.unlink = !!opt.unlink

    this.cwd = path.resolve(opt.cwd || process.cwd())
    this.strip = +opt.strip || 0
    this.processUmask = process.umask()
    this.umask = typeof opt.umask === 'number' ? opt.umask : this.processUmask
    // default mode for dirs created as parents
    this.dmode = opt.dmode || (0o0777 & (~this.umask))
    this.fmode = opt.fmode || (0o0666 & (~this.umask))
    this.on('entry', entry => this[ONENTRY](entry))
  }

  // a bad or damaged archive is a warning for Parser, but an error
  // when extracting.  Mark those errors as unrecoverable, because
  // the Unpack contract cannot be met.
  warn (code, msg, data = {}) {
    if (code === 'TAR_BAD_ARCHIVE' || code === 'TAR_ABORT')
      data.recoverable = false
    return super.warn(code, msg, data)
  }

  [MAYBECLOSE] () {
    if (this[ENDED] && this[PENDING] === 0) {
      this.emit('prefinish')
      this.emit('finish')
      this.emit('end')
      this.emit('close')
    }
  }

  [CHECKPATH] (entry) {
    if (this.strip) {
      const parts = entry.path.split(/\/|\\/)
      if (parts.length < this.strip)
        return false
      entry.path = parts.slice(this.strip).join('/')

      if (entry.type === 'Link') {
        const linkparts = entry.linkpath.split(/\/|\\/)
        if (linkparts.length >= this.strip)
          entry.linkpath = linkparts.slice(this.strip).join('/')
      }
    }

    if (!this.preservePaths) {
      const p = entry.path
      if (p.match(/(^|\/|\\)\.\.(\\|\/|$)/)) {
        this.warn('TAR_ENTRY_ERROR', `path contains '..'`, {
          entry,
          path: p,
        })
        return false
      }

      // absolutes on posix are also absolutes on win32
      // so we only need to test this one to get both
      if (path.win32.isAbsolute(p)) {
        const parsed = path.win32.parse(p)
        entry.path = p.substr(parsed.root.length)
        const r = parsed.root
        this.warn('TAR_ENTRY_INFO', `stripping ${r} from absolute path`, {
          entry,
          path: p,
        })
      }
    }

    // only encode : chars that aren't drive letter indicators
    if (this.win32) {
      const parsed = path.win32.parse(entry.path)
      entry.path = parsed.root === '' ? wc.encode(entry.path)
        : parsed.root + wc.encode(entry.path.substr(parsed.root.length))
    }

    if (path.isAbsolute(entry.path))
      entry.absolute = entry.path
    else
      entry.absolute = path.resolve(this.cwd, entry.path)

    return true
  }

  [ONENTRY] (entry) {
    if (!this[CHECKPATH](entry))
      return entry.resume()

    assert.equal(typeof entry.absolute, 'string')

    switch (entry.type) {
      case 'Directory':
      case 'GNUDumpDir':
        if (entry.mode)
          entry.mode = entry.mode | 0o700

      case 'File':
      case 'OldFile':
      case 'ContiguousFile':
      case 'Link':
      case 'SymbolicLink':
        return this[CHECKFS](entry)

      case 'CharacterDevice':
      case 'BlockDevice':
      case 'FIFO':
        return this[UNSUPPORTED](entry)
    }
  }

  [ONERROR] (er, entry) {
    // Cwd has to exist, or else nothing works. That's serious.
    // Other errors are warnings, which raise the error in strict
    // mode, but otherwise continue on.
    if (er.name === 'CwdError')
      this.emit('error', er)
    else {
      this.warn('TAR_ENTRY_ERROR', er, {entry})
      this[UNPEND]()
      entry.resume()
    }
  }

  [MKDIR] (dir, mode, cb) {
    mkdir(dir, {
      uid: this.uid,
      gid: this.gid,
      processUid: this.processUid,
      processGid: this.processGid,
      umask: this.processUmask,
      preserve: this.preservePaths,
      unlink: this.unlink,
      cache: this.dirCache,
      cwd: this.cwd,
      mode: mode
    }, cb)
  }

  [DOCHOWN] (entry) {
    // in preserve owner mode, chown if the entry doesn't match process
    // in set owner mode, chown if setting doesn't match process
    return this.forceChown ||
      this.preserveOwner &&
      ( typeof entry.uid === 'number' && entry.uid !== this.processUid ||
        typeof entry.gid === 'number' && entry.gid !== this.processGid )
      ||
      ( typeof this.uid === 'number' && this.uid !== this.processUid ||
        typeof this.gid === 'number' && this.gid !== this.processGid )
  }

  [UID] (entry) {
    return uint32(this.uid, entry.uid, this.processUid)
  }

  [GID] (entry) {
    return uint32(this.gid, entry.gid, this.processGid)
  }

  [FILE] (entry, fullyDone) {
    const mode = entry.mode & 0o7777 || this.fmode
    const stream = new fsm.WriteStream(entry.absolute, {
      flags: getFlag(entry.size),
      mode: mode,
      autoClose: false
    })
    stream.on('error', er => this[ONERROR](er, entry))

    let actions = 1
    const done = er => {
      if (er)
        return this[ONERROR](er, entry)

      if (--actions === 0) {
        fs.close(stream.fd, er => {
          fullyDone()
          er ? this[ONERROR](er, entry) : this[UNPEND]()
        })
      }
    }

    stream.on('finish', _ => {
      // if futimes fails, try utimes
      // if utimes fails, fail with the original error
      // same for fchown/chown
      const abs = entry.absolute
      const fd = stream.fd

      if (entry.mtime && !this.noMtime) {
        actions++
        const atime = entry.atime || new Date()
        const mtime = entry.mtime
        fs.futimes(fd, atime, mtime, er =>
          er ? fs.utimes(abs, atime, mtime, er2 => done(er2 && er))
          : done())
      }

      if (this[DOCHOWN](entry)) {
        actions++
        const uid = this[UID](entry)
        const gid = this[GID](entry)
        fs.fchown(fd, uid, gid, er =>
          er ? fs.chown(abs, uid, gid, er2 => done(er2 && er))
          : done())
      }

      done()
    })

    const tx = this.transform ? this.transform(entry) || entry : entry
    if (tx !== entry) {
      tx.on('error', er => this[ONERROR](er, entry))
      entry.pipe(tx)
    }
    tx.pipe(stream)
  }

  [DIRECTORY] (entry, fullyDone) {
    const mode = entry.mode & 0o7777 || this.dmode
    this[MKDIR](entry.absolute, mode, er => {
      if (er) {
        fullyDone()
        return this[ONERROR](er, entry)
      }

      let actions = 1
      const done = _ => {
        if (--actions === 0) {
          fullyDone()
          this[UNPEND]()
          entry.resume()
        }
      }

      if (entry.mtime && !this.noMtime) {
        actions++
        fs.utimes(entry.absolute, entry.atime || new Date(), entry.mtime, done)
      }

      if (this[DOCHOWN](entry)) {
        actions++
        fs.chown(entry.absolute, this[UID](entry), this[GID](entry), done)
      }

      done()
    })
  }

  [UNSUPPORTED] (entry) {
    entry.unsupported = true
    this.warn('TAR_ENTRY_UNSUPPORTED',
      `unsupported entry type: ${entry.type}`, {entry})
    entry.resume()
  }

  [SYMLINK] (entry, done) {
    this[LINK](entry, entry.linkpath, 'symlink', done)
  }

  [HARDLINK] (entry, done) {
    this[LINK](entry, path.resolve(this.cwd, entry.linkpath), 'link', done)
  }

  [PEND] () {
    this[PENDING]++
  }

  [UNPEND] () {
    this[PENDING]--
    this[MAYBECLOSE]()
  }

  [SKIP] (entry) {
    this[UNPEND]()
    entry.resume()
  }

  // Check if we can reuse an existing filesystem entry safely and
  // overwrite it, rather than unlinking and recreating
  // Windows doesn't report a useful nlink, so we just never reuse entries
  [ISREUSABLE] (entry, st) {
    return entry.type === 'File' &&
      !this.unlink &&
      st.isFile() &&
      st.nlink <= 1 &&
      process.platform !== 'win32'
  }

  // check if a thing is there, and if so, try to clobber it
  [CHECKFS] (entry) {
    this[PEND]()
    const paths = [entry.path]
    if (entry.linkpath)
      paths.push(entry.linkpath)
    this.reservations.reserve(paths, done => this[CHECKFS2](entry, done))
  }
  [CHECKFS2] (entry, done) {
    this[MKDIR](path.dirname(entry.absolute), this.dmode, er => {
      if (er) {
        done()
        return this[ONERROR](er, entry)
      }
      fs.lstat(entry.absolute, (er, st) => {
        if (st && (this.keep || this.newer && st.mtime > entry.mtime)) {
          this[SKIP](entry)
          done()
        } else if (er || this[ISREUSABLE](entry, st)) {
          this[MAKEFS](null, entry, done)
        }
        else if (st.isDirectory()) {
          if (entry.type === 'Directory') {
            if (!entry.mode || (st.mode & 0o7777) === entry.mode)
              this[MAKEFS](null, entry, done)
            else
              fs.chmod(entry.absolute, entry.mode,
                er => this[MAKEFS](er, entry, done))
          } else
            fs.rmdir(entry.absolute, er => this[MAKEFS](er, entry, done))
        } else
          unlinkFile(entry.absolute, er => this[MAKEFS](er, entry, done))
      })
    })
  }

  [MAKEFS] (er, entry, done) {
    if (er)
      return this[ONERROR](er, entry)

    switch (entry.type) {
      case 'File':
      case 'OldFile':
      case 'ContiguousFile':
        return this[FILE](entry, done)

      case 'Link':
        return this[HARDLINK](entry, done)

      case 'SymbolicLink':
        return this[SYMLINK](entry, done)

      case 'Directory':
      case 'GNUDumpDir':
        return this[DIRECTORY](entry, done)
    }
  }

  [LINK] (entry, linkpath, link, done) {
    // XXX: get the type ('file' or 'dir') for windows
    fs[link](linkpath, entry.absolute, er => {
      if (er)
        return this[ONERROR](er, entry)
      done()
      this[UNPEND]()
      entry.resume()
    })
  }
}

class UnpackSync extends Unpack {
  constructor (opt) {
    super(opt)
  }

  [CHECKFS] (entry) {
    const er = this[MKDIR](path.dirname(entry.absolute), this.dmode, neverCalled)
    if (er)
      return this[ONERROR](er, entry)
    try {
      const st = fs.lstatSync(entry.absolute)
      if (this.keep || this.newer && st.mtime > entry.mtime)
        return this[SKIP](entry)
      else if (this[ISREUSABLE](entry, st))
        return this[MAKEFS](null, entry, neverCalled)
      else {
        try {
          if (st.isDirectory()) {
            if (entry.type === 'Directory') {
              if (entry.mode && (st.mode & 0o7777) !== entry.mode)
                fs.chmodSync(entry.absolute, entry.mode)
            } else
              fs.rmdirSync(entry.absolute)
          } else
            unlinkFileSync(entry.absolute)
          return this[MAKEFS](null, entry, neverCalled)
        } catch (er) {
          return this[ONERROR](er, entry)
        }
      }
    } catch (er) {
      return this[MAKEFS](null, entry, neverCalled)
    }
  }

  [FILE] (entry, _) {
    const mode = entry.mode & 0o7777 || this.fmode

    const oner = er => {
      let closeError
      try {
        fs.closeSync(fd)
      } catch (e) {
        closeError = e
      }
      if (er || closeError)
        this[ONERROR](er || closeError, entry)
    }

    let stream
    let fd
    try {
      fd = fs.openSync(entry.absolute, getFlag(entry.size), mode)
    } catch (er) {
      return oner(er)
    }
    const tx = this.transform ? this.transform(entry) || entry : entry
    if (tx !== entry) {
      tx.on('error', er => this[ONERROR](er, entry))
      entry.pipe(tx)
    }

    tx.on('data', chunk => {
      try {
        fs.writeSync(fd, chunk, 0, chunk.length)
      } catch (er) {
        oner(er)
      }
    })

    tx.on('end', _ => {
      let er = null
      // try both, falling futimes back to utimes
      // if either fails, handle the first error
      if (entry.mtime && !this.noMtime) {
        const atime = entry.atime || new Date()
        const mtime = entry.mtime
        try {
          fs.futimesSync(fd, atime, mtime)
        } catch (futimeser) {
          try {
            fs.utimesSync(entry.absolute, atime, mtime)
          } catch (utimeser) {
            er = futimeser
          }
        }
      }

      if (this[DOCHOWN](entry)) {
        const uid = this[UID](entry)
        const gid = this[GID](entry)

        try {
          fs.fchownSync(fd, uid, gid)
        } catch (fchowner) {
          try {
            fs.chownSync(entry.absolute, uid, gid)
          } catch (chowner) {
            er = er || fchowner
          }
        }
      }

      oner(er)
    })
  }

  [DIRECTORY] (entry, _) {
    const mode = entry.mode & 0o7777 || this.dmode
    const er = this[MKDIR](entry.absolute, mode)
    if (er)
      return this[ONERROR](er, entry)
    if (entry.mtime && !this.noMtime) {
      try {
        fs.utimesSync(entry.absolute, entry.atime || new Date(), entry.mtime)
      } catch (er) {}
    }
    if (this[DOCHOWN](entry)) {
      try {
        fs.chownSync(entry.absolute, this[UID](entry), this[GID](entry))
      } catch (er) {}
    }
    entry.resume()
  }

  [MKDIR] (dir, mode) {
    try {
      return mkdir.sync(dir, {
        uid: this.uid,
        gid: this.gid,
        processUid: this.processUid,
        processGid: this.processGid,
        umask: this.processUmask,
        preserve: this.preservePaths,
        unlink: this.unlink,
        cache: this.dirCache,
        cwd: this.cwd,
        mode: mode
      })
    } catch (er) {
      return er
    }
  }

  [LINK] (entry, linkpath, link, _) {
    try {
      fs[link + 'Sync'](linkpath, entry.absolute)
      entry.resume()
    } catch (er) {
      return this[ONERROR](er, entry)
    }
  }
}

Unpack.Sync = UnpackSync
module.exports = Unpack


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/update.js":
/*!********************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/update.js ***!
  \********************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


// tar -u

const hlo = __webpack_require__(/*! ./high-level-opt.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/high-level-opt.js")
const r = __webpack_require__(/*! ./replace.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/replace.js")
// just call tar.r with the filter and mtimeCache

const u = module.exports = (opt_, files, cb) => {
  const opt = hlo(opt_)

  if (!opt.file)
    throw new TypeError('file is required')

  if (opt.gzip)
    throw new TypeError('cannot append to compressed archives')

  if (!files || !Array.isArray(files) || !files.length)
    throw new TypeError('no files or directories specified')

  files = Array.from(files)

  mtimeFilter(opt)
  return r(opt, files, cb)
}

const mtimeFilter = opt => {
  const filter = opt.filter

  if (!opt.mtimeCache)
    opt.mtimeCache = new Map()

  opt.filter = filter ? (path, stat) =>
    filter(path, stat) && !(opt.mtimeCache.get(path) > stat.mtime)
    : (path, stat) => !(opt.mtimeCache.get(path) > stat.mtime)
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/warn-mixin.js":
/*!************************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/warn-mixin.js ***!
  \************************************************************************************************/
/***/ ((module) => {

"use strict";

module.exports = Base => class extends Base {
  warn (code, message, data = {}) {
    if (this.file)
      data.file = this.file
    if (this.cwd)
      data.cwd = this.cwd
    data.code = message instanceof Error && message.code || code
    data.tarCode = code
    if (!this.strict && data.recoverable !== false) {
      if (message instanceof Error) {
        data = Object.assign(message, data)
        message = message.message
      }
      this.emit('warn', data.tarCode, message, data)
    } else if (message instanceof Error) {
      this.emit('error', Object.assign(message, data))
    } else
      this.emit('error', Object.assign(new Error(`${code}: ${message}`), data))
  }
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/winchars.js":
/*!**********************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/winchars.js ***!
  \**********************************************************************************************/
/***/ ((module) => {

"use strict";


// When writing files on Windows, translate the characters to their
// 0xf000 higher-encoded versions.

const raw = [
  '|',
  '<',
  '>',
  '?',
  ':'
]

const win = raw.map(char =>
  String.fromCharCode(0xf000 + char.charCodeAt(0)))

const toWin = new Map(raw.map((char, i) => [char, win[i]]))
const toRaw = new Map(win.map((char, i) => [char, raw[i]]))

module.exports = {
  encode: s => raw.reduce((s, c) => s.split(c).join(toWin.get(c)), s),
  decode: s => win.reduce((s, c) => s.split(c).join(toRaw.get(c)), s)
}


/***/ }),

/***/ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/write-entry.js":
/*!*************************************************************************************************!*\
  !*** ./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/write-entry.js ***!
  \*************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

const MiniPass = __webpack_require__(/*! minipass */ "./.yarn/cache/minipass-npm-3.1.1-b51d7e264d-cfcfd86adc.zip/node_modules/minipass/index.js")
const Pax = __webpack_require__(/*! ./pax.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/pax.js")
const Header = __webpack_require__(/*! ./header.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/header.js")
const ReadEntry = __webpack_require__(/*! ./read-entry.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/read-entry.js")
const fs = __webpack_require__(/*! fs */ "fs")
const path = __webpack_require__(/*! path */ "path")

const types = __webpack_require__(/*! ./types.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/types.js")
const maxReadSize = 16 * 1024 * 1024
const PROCESS = Symbol('process')
const FILE = Symbol('file')
const DIRECTORY = Symbol('directory')
const SYMLINK = Symbol('symlink')
const HARDLINK = Symbol('hardlink')
const HEADER = Symbol('header')
const READ = Symbol('read')
const LSTAT = Symbol('lstat')
const ONLSTAT = Symbol('onlstat')
const ONREAD = Symbol('onread')
const ONREADLINK = Symbol('onreadlink')
const OPENFILE = Symbol('openfile')
const ONOPENFILE = Symbol('onopenfile')
const CLOSE = Symbol('close')
const MODE = Symbol('mode')
const warner = __webpack_require__(/*! ./warn-mixin.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/warn-mixin.js")
const winchars = __webpack_require__(/*! ./winchars.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/winchars.js")

const modeFix = __webpack_require__(/*! ./mode-fix.js */ "./.yarn/cache/tar-npm-6.0.1-aedef672fb-498c4a5671.zip/node_modules/tar/lib/mode-fix.js")

const WriteEntry = warner(class WriteEntry extends MiniPass {
  constructor (p, opt) {
    opt = opt || {}
    super(opt)
    if (typeof p !== 'string')
      throw new TypeError('path is required')
    this.path = p
    // suppress atime, ctime, uid, gid, uname, gname
    this.portable = !!opt.portable
    // until node has builtin pwnam functions, this'll have to do
    this.myuid = process.getuid && process.getuid()
    this.myuser = process.env.USER || ''
    this.maxReadSize = opt.maxReadSize || maxReadSize
    this.linkCache = opt.linkCache || new Map()
    this.statCache = opt.statCache || new Map()
    this.preservePaths = !!opt.preservePaths
    this.cwd = opt.cwd || process.cwd()
    this.strict = !!opt.strict
    this.noPax = !!opt.noPax
    this.noMtime = !!opt.noMtime
    this.mtime = opt.mtime || null

    if (typeof opt.onwarn === 'function')
      this.on('warn', opt.onwarn)

    let pathWarn = false
    if (!this.preservePaths && path.win32.isAbsolute(p)) {
      // absolutes on posix are also absolutes on win32
      // so we only need to test this one to get both
      const parsed = path.win32.parse(p)
      this.path = p.substr(parsed.root.length)
      pathWarn = parsed.root
    }

    this.win32 = !!opt.win32 || process.platform === 'win32'
    if (this.win32) {
      this.path = winchars.decode(this.path.replace(/\\/g, '/'))
      p = p.replace(/\\/g, '/')
    }

    this.absolute = opt.absolute || path.resolve(this.cwd, p)

    if (this.path === '')
      this.path = './'

    if (pathWarn) {
      this.warn('TAR_ENTRY_INFO', `stripping ${pathWarn} from absolute path`, {
        entry: this,
        path: pathWarn + this.path,
      })
    }

    if (this.statCache.has(this.absolute))
      this[ONLSTAT](this.statCache.get(this.absolute))
    else
      this[LSTAT]()
  }

  [LSTAT] () {
    fs.lstat(this.absolute, (er, stat) => {
      if (er)
        return this.emit('error', er)
      this[ONLSTAT](stat)
    })
  }

  [ONLSTAT] (stat) {
    this.statCache.set(this.absolute, stat)
    this.stat = stat
    if (!stat.isFile())
      stat.size = 0
    this.type = getType(stat)
    this.emit('stat', stat)
    this[PROCESS]()
  }

  [PROCESS] () {
    switch (this.type) {
      case 'File': return this[FILE]()
      case 'Directory': return this[DIRECTORY]()
      case 'SymbolicLink': return this[SYMLINK]()
      // unsupported types are ignored.
      default: return this.end()
    }
  }

  [MODE] (mode) {
    return modeFix(mode, this.type === 'Directory', this.portable)
  }

  [HEADER] () {
    if (this.type === 'Directory' && this.portable)
      this.noMtime = true

    this.header = new Header({
      path: this.path,
      linkpath: this.linkpath,
      // only the permissions and setuid/setgid/sticky bitflags
      // not the higher-order bits that specify file type
      mode: this[MODE](this.stat.mode),
      uid: this.portable ? null : this.stat.uid,
      gid: this.portable ? null : this.stat.gid,
      size: this.stat.size,
      mtime: this.noMtime ? null : this.mtime || this.stat.mtime,
      type: this.type,
      uname: this.portable ? null :
        this.stat.uid === this.myuid ? this.myuser : '',
      atime: this.portable ? null : this.stat.atime,
      ctime: this.portable ? null : this.stat.ctime
    })

    if (this.header.encode() && !this.noPax)
      this.write(new Pax({
        atime: this.portable ? null : this.header.atime,
        ctime: this.portable ? null : this.header.ctime,
        gid: this.portable ? null : this.header.gid,
        mtime: this.noMtime ? null : this.mtime || this.header.mtime,
        path: this.path,
        linkpath: this.linkpath,
        size: this.header.size,
        uid: this.portable ? null : this.header.uid,
        uname: this.portable ? null : this.header.uname,
        dev: this.portable ? null : this.stat.dev,
        ino: this.portable ? null : this.stat.ino,
        nlink: this.portable ? null : this.stat.nlink
      }).encode())
    this.write(this.header.block)
  }

  [DIRECTORY] () {
    if (this.path.substr(-1) !== '/')
      this.path += '/'
    this.stat.size = 0
    this[HEADER]()
    this.end()
  }

  [SYMLINK] () {
    fs.readlink(this.absolute, (er, linkpath) => {
      if (er)
        return this.emit('error', er)
      this[ONREADLINK](linkpath)
    })
  }

  [ONREADLINK] (linkpath) {
    this.linkpath = linkpath.replace(/\\/g, '/')
    this[HEADER]()
    this.end()
  }

  [HARDLINK] (linkpath) {
    this.type = 'Link'
    this.linkpath = path.relative(this.cwd, linkpath).replace(/\\/g, '/')
    this.stat.size = 0
    this[HEADER]()
    this.end()
  }

  [FILE] () {
    if (this.stat.nlink > 1) {
      const linkKey = this.stat.dev + ':' + this.stat.ino
      if (this.linkCache.has(linkKey)) {
        const linkpath = this.linkCache.get(linkKey)
        if (linkpath.indexOf(this.cwd) === 0)
          return this[HARDLINK](linkpath)
      }
      this.linkCache.set(linkKey, this.absolute)
    }

    this[HEADER]()
    if (this.stat.size === 0)
      return this.end()

    this[OPENFILE]()
  }

  [OPENFILE] () {
    fs.open(this.absolute, 'r', (er, fd) => {
      if (er)
        return this.emit('error', er)
      this[ONOPENFILE](fd)
    })
  }

  [ONOPENFILE] (fd) {
    const blockLen = 512 * Math.ceil(this.stat.size / 512)
    const bufLen = Math.min(blockLen, this.maxReadSize)
    const buf = Buffer.allocUnsafe(bufLen)
    this[READ](fd, buf, 0, buf.length, 0, this.stat.size, blockLen)
  }

  [READ] (fd, buf, offset, length, pos, remain, blockRemain) {
    fs.read(fd, buf, offset, length, pos, (er, bytesRead) => {
      if (er) {
        // ignoring the error from close(2) is a bad practice, but at
        // this point we already have an error, don't need another one
        return this[CLOSE](fd, () => this.emit('error', er))
      }
      this[ONREAD](fd, buf, offset, length, pos, remain, blockRemain, bytesRead)
    })
  }

  [CLOSE] (fd, cb) {
    fs.close(fd, cb)
  }

  [ONREAD] (fd, buf, offset, length, pos, remain, blockRemain, bytesRead) {
    if (bytesRead <= 0 && remain > 0) {
      const er = new Error('encountered unexpected EOF')
      er.path = this.absolute
      er.syscall = 'read'
      er.code = 'EOF'
      return this[CLOSE](fd, () => this.emit('error', er))
    }

    if (bytesRead > remain) {
      const er = new Error('did not encounter expected EOF')
      er.path = this.absolute
      er.syscall = 'read'
      er.code = 'EOF'
      return this[CLOSE](fd, () => this.emit('error', er))
    }

    // null out the rest of the buffer, if we could fit the block padding
    if (bytesRead === remain) {
      for (let i = bytesRead; i < length && bytesRead < blockRemain; i++) {
        buf[i + offset] = 0
        bytesRead ++
        remain ++
      }
    }

    const writeBuf = offset === 0 && bytesRead === buf.length ?
      buf : buf.slice(offset, offset + bytesRead)
    remain -= bytesRead
    blockRemain -= bytesRead
    pos += bytesRead
    offset += bytesRead

    this.write(writeBuf)

    if (!remain) {
      if (blockRemain)
        this.write(Buffer.alloc(blockRemain))
      return this[CLOSE](fd, er => er ? this.emit('error', er) : this.end())
    }

    if (offset >= length) {
      buf = Buffer.allocUnsafe(length)
      offset = 0
    }
    length = buf.length - offset
    this[READ](fd, buf, offset, length, pos, remain, blockRemain)
  }
})

class WriteEntrySync extends WriteEntry {
  constructor (path, opt) {
    super(path, opt)
  }

  [LSTAT] () {
    this[ONLSTAT](fs.lstatSync(this.absolute))
  }

  [SYMLINK] () {
    this[ONREADLINK](fs.readlinkSync(this.absolute))
  }

  [OPENFILE] () {
    this[ONOPENFILE](fs.openSync(this.absolute, 'r'))
  }

  [READ] (fd, buf, offset, length, pos, remain, blockRemain) {
    let threw = true
    try {
      const bytesRead = fs.readSync(fd, buf, offset, length, pos)
      this[ONREAD](fd, buf, offset, length, pos, remain, blockRemain, bytesRead)
      threw = false
    } finally {
      // ignoring the error from close(2) is a bad practice, but at
      // this point we already have an error, don't need another one
      if (threw)
        try { this[CLOSE](fd, () => {}) } catch (er) {}
    }
  }

  [CLOSE] (fd, cb) {
    fs.closeSync(fd)
    cb()
  }
}

const WriteEntryTar = warner(class WriteEntryTar extends MiniPass {
  constructor (readEntry, opt) {
    opt = opt || {}
    super(opt)
    this.preservePaths = !!opt.preservePaths
    this.portable = !!opt.portable
    this.strict = !!opt.strict
    this.noPax = !!opt.noPax
    this.noMtime = !!opt.noMtime

    this.readEntry = readEntry
    this.type = readEntry.type
    if (this.type === 'Directory' && this.portable)
      this.noMtime = true

    this.path = readEntry.path
    this.mode = this[MODE](readEntry.mode)
    this.uid = this.portable ? null : readEntry.uid
    this.gid = this.portable ? null : readEntry.gid
    this.uname = this.portable ? null : readEntry.uname
    this.gname = this.portable ? null : readEntry.gname
    this.size = readEntry.size
    this.mtime = this.noMtime ? null : opt.mtime || readEntry.mtime
    this.atime = this.portable ? null : readEntry.atime
    this.ctime = this.portable ? null : readEntry.ctime
    this.linkpath = readEntry.linkpath

    if (typeof opt.onwarn === 'function')
      this.on('warn', opt.onwarn)

    let pathWarn = false
    if (path.isAbsolute(this.path) && !this.preservePaths) {
      const parsed = path.parse(this.path)
      pathWarn = parsed.root
      this.path = this.path.substr(parsed.root.length)
    }

    this.remain = readEntry.size
    this.blockRemain = readEntry.startBlockSize

    this.header = new Header({
      path: this.path,
      linkpath: this.linkpath,
      // only the permissions and setuid/setgid/sticky bitflags
      // not the higher-order bits that specify file type
      mode: this.mode,
      uid: this.portable ? null : this.uid,
      gid: this.portable ? null : this.gid,
      size: this.size,
      mtime: this.noMtime ? null : this.mtime,
      type: this.type,
      uname: this.portable ? null : this.uname,
      atime: this.portable ? null : this.atime,
      ctime: this.portable ? null : this.ctime
    })

    if (pathWarn) {
      this.warn('TAR_ENTRY_INFO', `stripping ${pathWarn} from absolute path`, {
        entry: this,
        path: pathWarn + this.path,
      })
    }

    if (this.header.encode() && !this.noPax)
      super.write(new Pax({
        atime: this.portable ? null : this.atime,
        ctime: this.portable ? null : this.ctime,
        gid: this.portable ? null : this.gid,
        mtime: this.noMtime ? null : this.mtime,
        path: this.path,
        linkpath: this.linkpath,
        size: this.size,
        uid: this.portable ? null : this.uid,
        uname: this.portable ? null : this.uname,
        dev: this.portable ? null : this.readEntry.dev,
        ino: this.portable ? null : this.readEntry.ino,
        nlink: this.portable ? null : this.readEntry.nlink
      }).encode())

    super.write(this.header.block)
    readEntry.pipe(this)
  }

  [MODE] (mode) {
    return modeFix(mode, this.type === 'Directory', this.portable)
  }

  write (data) {
    const writeLen = data.length
    if (writeLen > this.blockRemain)
      throw new Error('writing more to entry than is appropriate')
    this.blockRemain -= writeLen
    return super.write(data)
  }

  end () {
    if (this.blockRemain)
      this.write(Buffer.alloc(this.blockRemain))
    return super.end()
  }
})

WriteEntry.Sync = WriteEntrySync
WriteEntry.Tar = WriteEntryTar

const getType = stat =>
  stat.isFile() ? 'File'
  : stat.isDirectory() ? 'Directory'
  : stat.isSymbolicLink() ? 'SymbolicLink'
  : 'Unsupported'

module.exports = WriteEntry


/***/ }),

/***/ "./.yarn/cache/typanion-npm-3.3.1-ee9997c7cf-212d2beb55.zip/node_modules/typanion/lib/index.js":
/*!*****************************************************************************************************!*\
  !*** ./.yarn/cache/typanion-npm-3.3.1-ee9997c7cf-212d2beb55.zip/node_modules/typanion/lib/index.js ***!
  \*****************************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {

"use strict";


Object.defineProperty(exports, "__esModule", ({ value: true }));

const simpleKeyRegExp = /^[a-zA-Z_][a-zA-Z0-9_]*$/;
const colorStringRegExp = /^#[0-9a-f]{6}$/i;
const colorStringAlphaRegExp = /^#[0-9a-f]{6}([0-9a-f]{2})?$/i;
// https://stackoverflow.com/a/475217/880703
const base64RegExp = /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/;
// https://stackoverflow.com/a/14166194/880703
const uuid4RegExp = /^[a-f0-9]{8}-[a-f0-9]{4}-4[a-f0-9]{3}-[89aAbB][a-f0-9]{3}-[a-f0-9]{12}$/i;
// https://stackoverflow.com/a/28022901/880703 + https://www.debuggex.com/r/bl8J35wMKk48a7u_
const iso8601RegExp = /^(?:[1-9]\d{3}(-?)(?:(?:0[1-9]|1[0-2])\1(?:0[1-9]|1\d|2[0-8])|(?:0[13-9]|1[0-2])\1(?:29|30)|(?:0[13578]|1[02])(?:\1)31|00[1-9]|0[1-9]\d|[12]\d{2}|3(?:[0-5]\d|6[0-5]))|(?:[1-9]\d(?:0[48]|[2468][048]|[13579][26])|(?:[2468][048]|[13579][26])00)(?:(-?)02(?:\2)29|-?366))T(?:[01]\d|2[0-3])(:?)[0-5]\d(?:\3[0-5]\d)?(?:Z|[+-][01]\d(?:\3[0-5]\d)?)$/;
const makeTrait = (value) => () => {
    return value;
};
function makeValidator({ test }) {
    return makeTrait(test)();
}
function getPrintable(value) {
    if (value === null)
        return `null`;
    if (value === undefined)
        return `undefined`;
    if (value === ``)
        return `an empty string`;
    return JSON.stringify(value);
}
function computeKey(state, key) {
    var _a, _b, _c;
    if (typeof key === `number`) {
        return `${(_a = state === null || state === void 0 ? void 0 : state.p) !== null && _a !== void 0 ? _a : `.`}[${key}]`;
    }
    else if (simpleKeyRegExp.test(key)) {
        return `${(_b = state === null || state === void 0 ? void 0 : state.p) !== null && _b !== void 0 ? _b : ``}.${key}`;
    }
    else {
        return `${(_c = state === null || state === void 0 ? void 0 : state.p) !== null && _c !== void 0 ? _c : `.`}[${JSON.stringify(key)}]`;
    }
}
function makeCoercionFn(target, key) {
    return (v) => {
        const previous = target[key];
        target[key] = v;
        return makeCoercionFn(target, key).bind(null, previous);
    };
}
function makeSetter(target, key) {
    return (v) => {
        target[key] = v;
    };
}
function plural(n, singular, plural) {
    return n === 1 ? singular : plural;
}
function pushError({ errors, p } = {}, message) {
    errors === null || errors === void 0 ? void 0 : errors.push(`${p !== null && p !== void 0 ? p : `.`}: ${message}`);
    return false;
}
const isUnknown = () => makeValidator({
    test: (value, state) => {
        return true;
    },
});
function isLiteral(expected) {
    return makeValidator({
        test: (value, state) => {
            if (value !== expected)
                return pushError(state, `Expected a literal (got ${getPrintable(expected)})`);
            return true;
        },
    });
}
const isString = () => makeValidator({
    test: (value, state) => {
        if (typeof value !== `string`)
            return pushError(state, `Expected a string (got ${getPrintable(value)})`);
        return true;
    },
});
function isEnum(enumSpec) {
    const valuesArray = Array.isArray(enumSpec) ? enumSpec : Object.values(enumSpec);
    const values = new Set(valuesArray);
    return makeValidator({
        test: (value, state) => {
            if (!values.has(value))
                return pushError(state, `Expected a valid enumeration value (got ${getPrintable(value)})`);
            return true;
        },
    });
}
const BOOLEAN_COERCIONS = new Map([
    [`true`, true],
    [`True`, true],
    [`1`, true],
    [1, true],
    [`false`, false],
    [`False`, false],
    [`0`, false],
    [0, false],
]);
const isBoolean = () => makeValidator({
    test: (value, state) => {
        var _a;
        if (typeof value !== `boolean`) {
            if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
                if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
                    return pushError(state, `Unbound coercion result`);
                const coercion = BOOLEAN_COERCIONS.get(value);
                if (typeof coercion !== `undefined`) {
                    state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, coercion)]);
                    return true;
                }
            }
            return pushError(state, `Expected a boolean (got ${getPrintable(value)})`);
        }
        return true;
    },
});
const isNumber = () => makeValidator({
    test: (value, state) => {
        var _a;
        if (typeof value !== `number`) {
            if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
                if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
                    return pushError(state, `Unbound coercion result`);
                let coercion;
                if (typeof value === `string`) {
                    let val;
                    try {
                        val = JSON.parse(value);
                    }
                    catch (_b) { }
                    // We check against JSON.stringify that the output is the same to ensure that the number can be safely represented in JS
                    if (typeof val === `number`) {
                        if (JSON.stringify(val) === value) {
                            coercion = val;
                        }
                        else {
                            return pushError(state, `Received a number that can't be safely represented by the runtime (${value})`);
                        }
                    }
                }
                if (typeof coercion !== `undefined`) {
                    state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, coercion)]);
                    return true;
                }
            }
            return pushError(state, `Expected a number (got ${getPrintable(value)})`);
        }
        return true;
    },
});
const isDate = () => makeValidator({
    test: (value, state) => {
        var _a;
        if (!(value instanceof Date)) {
            if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
                if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
                    return pushError(state, `Unbound coercion result`);
                let coercion;
                if (typeof value === `string` && iso8601RegExp.test(value)) {
                    coercion = new Date(value);
                }
                else {
                    let timestamp;
                    if (typeof value === `string`) {
                        let val;
                        try {
                            val = JSON.parse(value);
                        }
                        catch (_b) { }
                        if (typeof val === `number`) {
                            timestamp = val;
                        }
                    }
                    else if (typeof value === `number`) {
                        timestamp = value;
                    }
                    if (typeof timestamp !== `undefined`) {
                        if (Number.isSafeInteger(timestamp) || !Number.isSafeInteger(timestamp * 1000)) {
                            coercion = new Date(timestamp * 1000);
                        }
                        else {
                            return pushError(state, `Received a timestamp that can't be safely represented by the runtime (${value})`);
                        }
                    }
                }
                if (typeof coercion !== `undefined`) {
                    state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, coercion)]);
                    return true;
                }
            }
            return pushError(state, `Expected a date (got ${getPrintable(value)})`);
        }
        return true;
    },
});
const isArray = (spec, { delimiter } = {}) => makeValidator({
    test: (value, state) => {
        var _a;
        if (typeof value === `string` && typeof delimiter !== `undefined`) {
            if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
                if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
                    return pushError(state, `Unbound coercion result`);
                value = value.split(delimiter);
                state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, value)]);
            }
        }
        if (!Array.isArray(value))
            return pushError(state, `Expected an array (got ${getPrintable(value)})`);
        let valid = true;
        for (let t = 0, T = value.length; t < T; ++t) {
            valid = spec(value[t], Object.assign(Object.assign({}, state), { p: computeKey(state, t), coercion: makeCoercionFn(value, t) })) && valid;
            if (!valid && (state === null || state === void 0 ? void 0 : state.errors) == null) {
                break;
            }
        }
        return valid;
    },
});
const isTuple = (spec, { delimiter } = {}) => {
    const lengthValidator = hasExactLength(spec.length);
    return makeValidator({
        test: (value, state) => {
            var _a;
            if (typeof value === `string` && typeof delimiter !== `undefined`) {
                if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
                    if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
                        return pushError(state, `Unbound coercion result`);
                    value = value.split(delimiter);
                    state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, value)]);
                }
            }
            if (!Array.isArray(value))
                return pushError(state, `Expected a tuple (got ${getPrintable(value)})`);
            let valid = lengthValidator(value, Object.assign({}, state));
            for (let t = 0, T = value.length; t < T && t < spec.length; ++t) {
                valid = spec[t](value[t], Object.assign(Object.assign({}, state), { p: computeKey(state, t), coercion: makeCoercionFn(value, t) })) && valid;
                if (!valid && (state === null || state === void 0 ? void 0 : state.errors) == null) {
                    break;
                }
            }
            return valid;
        },
    });
};
const isDict = (spec, { keys: keySpec = null, } = {}) => makeValidator({
    test: (value, state) => {
        if (typeof value !== `object` || value === null)
            return pushError(state, `Expected an object (got ${getPrintable(value)})`);
        const keys = Object.keys(value);
        let valid = true;
        for (let t = 0, T = keys.length; t < T && (valid || (state === null || state === void 0 ? void 0 : state.errors) != null); ++t) {
            const key = keys[t];
            const sub = value[key];
            if (key === `__proto__` || key === `constructor`) {
                valid = pushError(Object.assign(Object.assign({}, state), { p: computeKey(state, key) }), `Unsafe property name`);
                continue;
            }
            if (keySpec !== null && !keySpec(key, state)) {
                valid = false;
                continue;
            }
            if (!spec(sub, Object.assign(Object.assign({}, state), { p: computeKey(state, key), coercion: makeCoercionFn(value, key) }))) {
                valid = false;
                continue;
            }
        }
        return valid;
    },
});
const isObject = (props, { extra: extraSpec = null, } = {}) => {
    const specKeys = Object.keys(props);
    return makeValidator({
        test: (value, state) => {
            if (typeof value !== `object` || value === null)
                return pushError(state, `Expected an object (got ${getPrintable(value)})`);
            const keys = new Set([...specKeys, ...Object.keys(value)]);
            const extra = {};
            let valid = true;
            for (const key of keys) {
                if (key === `constructor` || key === `__proto__`) {
                    valid = pushError(Object.assign(Object.assign({}, state), { p: computeKey(state, key) }), `Unsafe property name`);
                }
                else {
                    const spec = Object.prototype.hasOwnProperty.call(props, key)
                        ? props[key]
                        : undefined;
                    const sub = Object.prototype.hasOwnProperty.call(value, key)
                        ? value[key]
                        : undefined;
                    if (typeof spec !== `undefined`) {
                        valid = spec(sub, Object.assign(Object.assign({}, state), { p: computeKey(state, key), coercion: makeCoercionFn(value, key) })) && valid;
                    }
                    else if (extraSpec === null) {
                        valid = pushError(Object.assign(Object.assign({}, state), { p: computeKey(state, key) }), `Extraneous property (got ${getPrintable(sub)})`);
                    }
                    else {
                        Object.defineProperty(extra, key, {
                            enumerable: true,
                            get: () => sub,
                            set: makeSetter(value, key)
                        });
                    }
                }
                if (!valid && (state === null || state === void 0 ? void 0 : state.errors) == null) {
                    break;
                }
            }
            if (extraSpec !== null && (valid || (state === null || state === void 0 ? void 0 : state.errors) != null))
                valid = extraSpec(extra, state) && valid;
            return valid;
        },
    });
};
const isInstanceOf = (constructor) => makeValidator({
    test: (value, state) => {
        if (!(value instanceof constructor))
            return pushError(state, `Expected an instance of ${constructor.name} (got ${getPrintable(value)})`);
        return true;
    },
});
const isOneOf = (specs, { exclusive = false, } = {}) => makeValidator({
    test: (value, state) => {
        var _a, _b, _c;
        const matches = [];
        const errorBuffer = typeof (state === null || state === void 0 ? void 0 : state.errors) !== `undefined`
            ? [] : undefined;
        for (let t = 0, T = specs.length; t < T; ++t) {
            const subErrors = typeof (state === null || state === void 0 ? void 0 : state.errors) !== `undefined`
                ? [] : undefined;
            const subCoercions = typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`
                ? [] : undefined;
            if (specs[t](value, Object.assign(Object.assign({}, state), { errors: subErrors, coercions: subCoercions, p: `${(_a = state === null || state === void 0 ? void 0 : state.p) !== null && _a !== void 0 ? _a : `.`}#${t + 1}` }))) {
                matches.push([`#${t + 1}`, subCoercions]);
                if (!exclusive) {
                    break;
                }
            }
            else {
                errorBuffer === null || errorBuffer === void 0 ? void 0 : errorBuffer.push(subErrors[0]);
            }
        }
        if (matches.length === 1) {
            const [, subCoercions] = matches[0];
            if (typeof subCoercions !== `undefined`)
                (_b = state === null || state === void 0 ? void 0 : state.coercions) === null || _b === void 0 ? void 0 : _b.push(...subCoercions);
            return true;
        }
        if (matches.length > 1)
            pushError(state, `Expected to match exactly a single predicate (matched ${matches.join(`, `)})`);
        else
            (_c = state === null || state === void 0 ? void 0 : state.errors) === null || _c === void 0 ? void 0 : _c.push(...errorBuffer);
        return false;
    },
});
const applyCascade = (spec, followups) => makeValidator({
    test: (value, state) => {
        var _a, _b;
        const context = { value: value };
        const subCoercion = typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`
            ? makeCoercionFn(context, `value`) : undefined;
        const subCoercions = typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`
            ? [] : undefined;
        if (!spec(value, Object.assign(Object.assign({}, state), { coercion: subCoercion, coercions: subCoercions })))
            return false;
        const reverts = [];
        if (typeof subCoercions !== `undefined`)
            for (const [, coercion] of subCoercions)
                reverts.push(coercion());
        try {
            if (typeof (state === null || state === void 0 ? void 0 : state.coercions) !== `undefined`) {
                if (context.value !== value) {
                    if (typeof (state === null || state === void 0 ? void 0 : state.coercion) === `undefined`)
                        return pushError(state, `Unbound coercion result`);
                    state.coercions.push([(_a = state.p) !== null && _a !== void 0 ? _a : `.`, state.coercion.bind(null, context.value)]);
                }
                (_b = state === null || state === void 0 ? void 0 : state.coercions) === null || _b === void 0 ? void 0 : _b.push(...subCoercions);
            }
            return followups.every(spec => {
                return spec(context.value, state);
            });
        }
        finally {
            for (const revert of reverts) {
                revert();
            }
        }
    },
});
const isOptional = (spec) => makeValidator({
    test: (value, state) => {
        if (typeof value === `undefined`)
            return true;
        return spec(value, state);
    },
});
const isNullable = (spec) => makeValidator({
    test: (value, state) => {
        if (value === null)
            return true;
        return spec(value, state);
    },
});
const hasMinLength = (length) => makeValidator({
    test: (value, state) => {
        if (!(value.length >= length))
            return pushError(state, `Expected to have a length of at least ${length} elements (got ${value.length})`);
        return true;
    },
});
const hasMaxLength = (length) => makeValidator({
    test: (value, state) => {
        if (!(value.length <= length))
            return pushError(state, `Expected to have a length of at most ${length} elements (got ${value.length})`);
        return true;
    },
});
const hasExactLength = (length) => makeValidator({
    test: (value, state) => {
        if (!(value.length === length))
            return pushError(state, `Expected to have a length of exactly ${length} elements (got ${value.length})`);
        return true;
    },
});
const hasUniqueItems = ({ map, } = {}) => makeValidator({
    test: (value, state) => {
        const set = new Set();
        const dup = new Set();
        for (let t = 0, T = value.length; t < T; ++t) {
            const sub = value[t];
            const key = typeof map !== `undefined`
                ? map(sub)
                : sub;
            if (set.has(key)) {
                if (dup.has(key))
                    continue;
                pushError(state, `Expected to contain unique elements; got a duplicate with ${getPrintable(value)}`);
                dup.add(key);
            }
            else {
                set.add(key);
            }
        }
        return dup.size === 0;
    },
});
const isNegative = () => makeValidator({
    test: (value, state) => {
        if (!(value <= 0))
            return pushError(state, `Expected to be negative (got ${value})`);
        return true;
    },
});
const isPositive = () => makeValidator({
    test: (value, state) => {
        if (!(value >= 0))
            return pushError(state, `Expected to be positive (got ${value})`);
        return true;
    },
});
const isAtLeast = (n) => makeValidator({
    test: (value, state) => {
        if (!(value >= n))
            return pushError(state, `Expected to be at least ${n} (got ${value})`);
        return true;
    },
});
const isAtMost = (n) => makeValidator({
    test: (value, state) => {
        if (!(value <= n))
            return pushError(state, `Expected to be at most ${n} (got ${value})`);
        return true;
    },
});
const isInInclusiveRange = (a, b) => makeValidator({
    test: (value, state) => {
        if (!(value >= a && value <= b))
            return pushError(state, `Expected to be in the [${a}; ${b}] range (got ${value})`);
        return true;
    },
});
const isInExclusiveRange = (a, b) => makeValidator({
    test: (value, state) => {
        if (!(value >= a && value < b))
            return pushError(state, `Expected to be in the [${a}; ${b}[ range (got ${value})`);
        return true;
    },
});
const isInteger = ({ unsafe = false, } = {}) => makeValidator({
    test: (value, state) => {
        if (value !== Math.round(value))
            return pushError(state, `Expected to be an integer (got ${value})`);
        if (!Number.isSafeInteger(value))
            return pushError(state, `Expected to be a safe integer (got ${value})`);
        return true;
    },
});
const matchesRegExp = (regExp) => makeValidator({
    test: (value, state) => {
        if (!regExp.test(value))
            return pushError(state, `Expected to match the pattern ${regExp.toString()} (got ${getPrintable(value)})`);
        return true;
    },
});
const isLowerCase = () => makeValidator({
    test: (value, state) => {
        if (value !== value.toLowerCase())
            return pushError(state, `Expected to be all-lowercase (got ${value})`);
        return true;
    },
});
const isUpperCase = () => makeValidator({
    test: (value, state) => {
        if (value !== value.toUpperCase())
            return pushError(state, `Expected to be all-uppercase (got ${value})`);
        return true;
    },
});
const isUUID4 = () => makeValidator({
    test: (value, state) => {
        if (!uuid4RegExp.test(value))
            return pushError(state, `Expected to be a valid UUID v4 (got ${getPrintable(value)})`);
        return true;
    },
});
const isISO8601 = () => makeValidator({
    test: (value, state) => {
        if (!iso8601RegExp.test(value))
            return pushError(state, `Expected to be a valid ISO 8601 date string (got ${getPrintable(value)})`);
        return false;
    },
});
const isHexColor = ({ alpha = false, }) => makeValidator({
    test: (value, state) => {
        const res = alpha
            ? colorStringRegExp.test(value)
            : colorStringAlphaRegExp.test(value);
        if (!res)
            return pushError(state, `Expected to be a valid hexadecimal color string (got ${getPrintable(value)})`);
        return true;
    },
});
const isBase64 = () => makeValidator({
    test: (value, state) => {
        if (!base64RegExp.test(value))
            return pushError(state, `Expected to be a valid base 64 string (got ${getPrintable(value)})`);
        return true;
    },
});
const isJSON = (spec = isUnknown()) => makeValidator({
    test: (value, state) => {
        let data;
        try {
            data = JSON.parse(value);
        }
        catch (_a) {
            return pushError(state, `Expected to be a valid JSON string (got ${getPrintable(value)})`);
        }
        return spec(data, state);
    },
});
const hasRequiredKeys = (requiredKeys) => {
    const requiredSet = new Set(requiredKeys);
    return makeValidator({
        test: (value, state) => {
            const keys = new Set(Object.keys(value));
            const problems = [];
            for (const key of requiredSet)
                if (!keys.has(key))
                    problems.push(key);
            if (problems.length > 0)
                return pushError(state, `Missing required ${plural(problems.length, `property`, `properties`)} ${problems.map(name => `"${name}"`).join(`, `)}`);
            return true;
        },
    });
};
const hasForbiddenKeys = (forbiddenKeys) => {
    const forbiddenSet = new Set(forbiddenKeys);
    return makeValidator({
        test: (value, state) => {
            const keys = new Set(Object.keys(value));
            const problems = [];
            for (const key of forbiddenSet)
                if (keys.has(key))
                    problems.push(key);
            if (problems.length > 0)
                return pushError(state, `Forbidden ${plural(problems.length, `property`, `properties`)} ${problems.map(name => `"${name}"`).join(`, `)}`);
            return true;
        },
    });
};
const hasMutuallyExclusiveKeys = (exclusiveKeys) => {
    const exclusiveSet = new Set(exclusiveKeys);
    return makeValidator({
        test: (value, state) => {
            const keys = new Set(Object.keys(value));
            const used = [];
            for (const key of exclusiveSet)
                if (keys.has(key))
                    used.push(key);
            if (used.length > 1)
                return pushError(state, `Mutually exclusive properties ${used.map(name => `"${name}"`).join(`, `)}`);
            return true;
        },
    });
};
(function (KeyRelationship) {
    KeyRelationship["Forbids"] = "Forbids";
    KeyRelationship["Requires"] = "Requires";
})(exports.KeyRelationship || (exports.KeyRelationship = {}));
const keyRelationships = {
    [exports.KeyRelationship.Forbids]: {
        expect: false,
        message: `forbids using`,
    },
    [exports.KeyRelationship.Requires]: {
        expect: true,
        message: `requires using`,
    },
};
const hasKeyRelationship = (subject, relationship, others, { ignore = [], } = {}) => {
    const skipped = new Set(ignore);
    const otherSet = new Set(others);
    const spec = keyRelationships[relationship];
    return makeValidator({
        test: (value, state) => {
            const keys = new Set(Object.keys(value));
            if (!keys.has(subject) || skipped.has(value[subject]))
                return true;
            const problems = [];
            for (const key of otherSet)
                if ((keys.has(key) && !skipped.has(value[key])) !== spec.expect)
                    problems.push(key);
            if (problems.length >= 1)
                return pushError(state, `Property "${subject}" ${spec.message} ${plural(problems.length, `property`, `properties`)} ${problems.map(name => `"${name}"`).join(`, `)}`);
            return true;
        },
    });
};

exports.applyCascade = applyCascade;
exports.base64RegExp = base64RegExp;
exports.colorStringAlphaRegExp = colorStringAlphaRegExp;
exports.colorStringRegExp = colorStringRegExp;
exports.computeKey = computeKey;
exports.getPrintable = getPrintable;
exports.hasExactLength = hasExactLength;
exports.hasForbiddenKeys = hasForbiddenKeys;
exports.hasKeyRelationship = hasKeyRelationship;
exports.hasMaxLength = hasMaxLength;
exports.hasMinLength = hasMinLength;
exports.hasMutuallyExclusiveKeys = hasMutuallyExclusiveKeys;
exports.hasRequiredKeys = hasRequiredKeys;
exports.hasUniqueItems = hasUniqueItems;
exports.isArray = isArray;
exports.isAtLeast = isAtLeast;
exports.isAtMost = isAtMost;
exports.isBase64 = isBase64;
exports.isBoolean = isBoolean;
exports.isDate = isDate;
exports.isDict = isDict;
exports.isEnum = isEnum;
exports.isHexColor = isHexColor;
exports.isISO8601 = isISO8601;
exports.isInExclusiveRange = isInExclusiveRange;
exports.isInInclusiveRange = isInInclusiveRange;
exports.isInstanceOf = isInstanceOf;
exports.isInteger = isInteger;
exports.isJSON = isJSON;
exports.isLiteral = isLiteral;
exports.isLowerCase = isLowerCase;
exports.isNegative = isNegative;
exports.isNullable = isNullable;
exports.isNumber = isNumber;
exports.isObject = isObject;
exports.isOneOf = isOneOf;
exports.isOptional = isOptional;
exports.isPositive = isPositive;
exports.isString = isString;
exports.isTuple = isTuple;
exports.isUUID4 = isUUID4;
exports.isUnknown = isUnknown;
exports.isUpperCase = isUpperCase;
exports.iso8601RegExp = iso8601RegExp;
exports.makeCoercionFn = makeCoercionFn;
exports.makeSetter = makeSetter;
exports.makeTrait = makeTrait;
exports.makeValidator = makeValidator;
exports.matchesRegExp = matchesRegExp;
exports.plural = plural;
exports.pushError = pushError;
exports.simpleKeyRegExp = simpleKeyRegExp;
exports.uuid4RegExp = uuid4RegExp;


/***/ }),

/***/ "./.yarn/cache/which-npm-2.0.2-320ddf72f7-1a5c563d3c.zip/node_modules/which/which.js":
/*!*******************************************************************************************!*\
  !*** ./.yarn/cache/which-npm-2.0.2-320ddf72f7-1a5c563d3c.zip/node_modules/which/which.js ***!
  \*******************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

const isWindows = process.platform === 'win32' ||
    process.env.OSTYPE === 'cygwin' ||
    process.env.OSTYPE === 'msys'

const path = __webpack_require__(/*! path */ "path")
const COLON = isWindows ? ';' : ':'
const isexe = __webpack_require__(/*! isexe */ "./.yarn/cache/isexe-npm-2.0.0-b58870bd2e-26bf6c5480.zip/node_modules/isexe/index.js")

const getNotFoundError = (cmd) =>
  Object.assign(new Error(`not found: ${cmd}`), { code: 'ENOENT' })

const getPathInfo = (cmd, opt) => {
  const colon = opt.colon || COLON

  // If it has a slash, then we don't bother searching the pathenv.
  // just check the file itself, and that's it.
  const pathEnv = cmd.match(/\//) || isWindows && cmd.match(/\\/) ? ['']
    : (
      [
        // windows always checks the cwd first
        ...(isWindows ? [process.cwd()] : []),
        ...(opt.path || process.env.PATH ||
          /* istanbul ignore next: very unusual */ '').split(colon),
      ]
    )
  const pathExtExe = isWindows
    ? opt.pathExt || process.env.PATHEXT || '.EXE;.CMD;.BAT;.COM'
    : ''
  const pathExt = isWindows ? pathExtExe.split(colon) : ['']

  if (isWindows) {
    if (cmd.indexOf('.') !== -1 && pathExt[0] !== '')
      pathExt.unshift('')
  }

  return {
    pathEnv,
    pathExt,
    pathExtExe,
  }
}

const which = (cmd, opt, cb) => {
  if (typeof opt === 'function') {
    cb = opt
    opt = {}
  }
  if (!opt)
    opt = {}

  const { pathEnv, pathExt, pathExtExe } = getPathInfo(cmd, opt)
  const found = []

  const step = i => new Promise((resolve, reject) => {
    if (i === pathEnv.length)
      return opt.all && found.length ? resolve(found)
        : reject(getNotFoundError(cmd))

    const ppRaw = pathEnv[i]
    const pathPart = /^".*"$/.test(ppRaw) ? ppRaw.slice(1, -1) : ppRaw

    const pCmd = path.join(pathPart, cmd)
    const p = !pathPart && /^\.[\\\/]/.test(cmd) ? cmd.slice(0, 2) + pCmd
      : pCmd

    resolve(subStep(p, i, 0))
  })

  const subStep = (p, i, ii) => new Promise((resolve, reject) => {
    if (ii === pathExt.length)
      return resolve(step(i + 1))
    const ext = pathExt[ii]
    isexe(p + ext, { pathExt: pathExtExe }, (er, is) => {
      if (!er && is) {
        if (opt.all)
          found.push(p + ext)
        else
          return resolve(p + ext)
      }
      return resolve(subStep(p, i, ii + 1))
    })
  })

  return cb ? step(0).then(res => cb(null, res), cb) : step(0)
}

const whichSync = (cmd, opt) => {
  opt = opt || {}

  const { pathEnv, pathExt, pathExtExe } = getPathInfo(cmd, opt)
  const found = []

  for (let i = 0; i < pathEnv.length; i ++) {
    const ppRaw = pathEnv[i]
    const pathPart = /^".*"$/.test(ppRaw) ? ppRaw.slice(1, -1) : ppRaw

    const pCmd = path.join(pathPart, cmd)
    const p = !pathPart && /^\.[\\\/]/.test(cmd) ? cmd.slice(0, 2) + pCmd
      : pCmd

    for (let j = 0; j < pathExt.length; j ++) {
      const cur = p + pathExt[j]
      try {
        const is = isexe.sync(cur, { pathExt: pathExtExe })
        if (is) {
          if (opt.all)
            found.push(cur)
          else
            return cur
        }
      } catch (ex) {}
    }
  }

  if (opt.all && found.length)
    return found

  if (opt.nothrow)
    return null

  throw getNotFoundError(cmd)
}

module.exports = which
which.sync = whichSync


/***/ }),

/***/ "./.yarn/cache/yallist-npm-4.0.0-b493d9e907-343617202a.zip/node_modules/yallist/iterator.js":
/*!**************************************************************************************************!*\
  !*** ./.yarn/cache/yallist-npm-4.0.0-b493d9e907-343617202a.zip/node_modules/yallist/iterator.js ***!
  \**************************************************************************************************/
/***/ ((module) => {

"use strict";

module.exports = function (Yallist) {
  Yallist.prototype[Symbol.iterator] = function* () {
    for (let walker = this.head; walker; walker = walker.next) {
      yield walker.value
    }
  }
}


/***/ }),

/***/ "./.yarn/cache/yallist-npm-4.0.0-b493d9e907-343617202a.zip/node_modules/yallist/yallist.js":
/*!*************************************************************************************************!*\
  !*** ./.yarn/cache/yallist-npm-4.0.0-b493d9e907-343617202a.zip/node_modules/yallist/yallist.js ***!
  \*************************************************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";

module.exports = Yallist

Yallist.Node = Node
Yallist.create = Yallist

function Yallist (list) {
  var self = this
  if (!(self instanceof Yallist)) {
    self = new Yallist()
  }

  self.tail = null
  self.head = null
  self.length = 0

  if (list && typeof list.forEach === 'function') {
    list.forEach(function (item) {
      self.push(item)
    })
  } else if (arguments.length > 0) {
    for (var i = 0, l = arguments.length; i < l; i++) {
      self.push(arguments[i])
    }
  }

  return self
}

Yallist.prototype.removeNode = function (node) {
  if (node.list !== this) {
    throw new Error('removing node which does not belong to this list')
  }

  var next = node.next
  var prev = node.prev

  if (next) {
    next.prev = prev
  }

  if (prev) {
    prev.next = next
  }

  if (node === this.head) {
    this.head = next
  }
  if (node === this.tail) {
    this.tail = prev
  }

  node.list.length--
  node.next = null
  node.prev = null
  node.list = null

  return next
}

Yallist.prototype.unshiftNode = function (node) {
  if (node === this.head) {
    return
  }

  if (node.list) {
    node.list.removeNode(node)
  }

  var head = this.head
  node.list = this
  node.next = head
  if (head) {
    head.prev = node
  }

  this.head = node
  if (!this.tail) {
    this.tail = node
  }
  this.length++
}

Yallist.prototype.pushNode = function (node) {
  if (node === this.tail) {
    return
  }

  if (node.list) {
    node.list.removeNode(node)
  }

  var tail = this.tail
  node.list = this
  node.prev = tail
  if (tail) {
    tail.next = node
  }

  this.tail = node
  if (!this.head) {
    this.head = node
  }
  this.length++
}

Yallist.prototype.push = function () {
  for (var i = 0, l = arguments.length; i < l; i++) {
    push(this, arguments[i])
  }
  return this.length
}

Yallist.prototype.unshift = function () {
  for (var i = 0, l = arguments.length; i < l; i++) {
    unshift(this, arguments[i])
  }
  return this.length
}

Yallist.prototype.pop = function () {
  if (!this.tail) {
    return undefined
  }

  var res = this.tail.value
  this.tail = this.tail.prev
  if (this.tail) {
    this.tail.next = null
  } else {
    this.head = null
  }
  this.length--
  return res
}

Yallist.prototype.shift = function () {
  if (!this.head) {
    return undefined
  }

  var res = this.head.value
  this.head = this.head.next
  if (this.head) {
    this.head.prev = null
  } else {
    this.tail = null
  }
  this.length--
  return res
}

Yallist.prototype.forEach = function (fn, thisp) {
  thisp = thisp || this
  for (var walker = this.head, i = 0; walker !== null; i++) {
    fn.call(thisp, walker.value, i, this)
    walker = walker.next
  }
}

Yallist.prototype.forEachReverse = function (fn, thisp) {
  thisp = thisp || this
  for (var walker = this.tail, i = this.length - 1; walker !== null; i--) {
    fn.call(thisp, walker.value, i, this)
    walker = walker.prev
  }
}

Yallist.prototype.get = function (n) {
  for (var i = 0, walker = this.head; walker !== null && i < n; i++) {
    // abort out of the list early if we hit a cycle
    walker = walker.next
  }
  if (i === n && walker !== null) {
    return walker.value
  }
}

Yallist.prototype.getReverse = function (n) {
  for (var i = 0, walker = this.tail; walker !== null && i < n; i++) {
    // abort out of the list early if we hit a cycle
    walker = walker.prev
  }
  if (i === n && walker !== null) {
    return walker.value
  }
}

Yallist.prototype.map = function (fn, thisp) {
  thisp = thisp || this
  var res = new Yallist()
  for (var walker = this.head; walker !== null;) {
    res.push(fn.call(thisp, walker.value, this))
    walker = walker.next
  }
  return res
}

Yallist.prototype.mapReverse = function (fn, thisp) {
  thisp = thisp || this
  var res = new Yallist()
  for (var walker = this.tail; walker !== null;) {
    res.push(fn.call(thisp, walker.value, this))
    walker = walker.prev
  }
  return res
}

Yallist.prototype.reduce = function (fn, initial) {
  var acc
  var walker = this.head
  if (arguments.length > 1) {
    acc = initial
  } else if (this.head) {
    walker = this.head.next
    acc = this.head.value
  } else {
    throw new TypeError('Reduce of empty list with no initial value')
  }

  for (var i = 0; walker !== null; i++) {
    acc = fn(acc, walker.value, i)
    walker = walker.next
  }

  return acc
}

Yallist.prototype.reduceReverse = function (fn, initial) {
  var acc
  var walker = this.tail
  if (arguments.length > 1) {
    acc = initial
  } else if (this.tail) {
    walker = this.tail.prev
    acc = this.tail.value
  } else {
    throw new TypeError('Reduce of empty list with no initial value')
  }

  for (var i = this.length - 1; walker !== null; i--) {
    acc = fn(acc, walker.value, i)
    walker = walker.prev
  }

  return acc
}

Yallist.prototype.toArray = function () {
  var arr = new Array(this.length)
  for (var i = 0, walker = this.head; walker !== null; i++) {
    arr[i] = walker.value
    walker = walker.next
  }
  return arr
}

Yallist.prototype.toArrayReverse = function () {
  var arr = new Array(this.length)
  for (var i = 0, walker = this.tail; walker !== null; i++) {
    arr[i] = walker.value
    walker = walker.prev
  }
  return arr
}

Yallist.prototype.slice = function (from, to) {
  to = to || this.length
  if (to < 0) {
    to += this.length
  }
  from = from || 0
  if (from < 0) {
    from += this.length
  }
  var ret = new Yallist()
  if (to < from || to < 0) {
    return ret
  }
  if (from < 0) {
    from = 0
  }
  if (to > this.length) {
    to = this.length
  }
  for (var i = 0, walker = this.head; walker !== null && i < from; i++) {
    walker = walker.next
  }
  for (; walker !== null && i < to; i++, walker = walker.next) {
    ret.push(walker.value)
  }
  return ret
}

Yallist.prototype.sliceReverse = function (from, to) {
  to = to || this.length
  if (to < 0) {
    to += this.length
  }
  from = from || 0
  if (from < 0) {
    from += this.length
  }
  var ret = new Yallist()
  if (to < from || to < 0) {
    return ret
  }
  if (from < 0) {
    from = 0
  }
  if (to > this.length) {
    to = this.length
  }
  for (var i = this.length, walker = this.tail; walker !== null && i > to; i--) {
    walker = walker.prev
  }
  for (; walker !== null && i > from; i--, walker = walker.prev) {
    ret.push(walker.value)
  }
  return ret
}

Yallist.prototype.splice = function (start, deleteCount, ...nodes) {
  if (start > this.length) {
    start = this.length - 1
  }
  if (start < 0) {
    start = this.length + start;
  }

  for (var i = 0, walker = this.head; walker !== null && i < start; i++) {
    walker = walker.next
  }

  var ret = []
  for (var i = 0; walker && i < deleteCount; i++) {
    ret.push(walker.value)
    walker = this.removeNode(walker)
  }
  if (walker === null) {
    walker = this.tail
  }

  if (walker !== this.head && walker !== this.tail) {
    walker = walker.prev
  }

  for (var i = 0; i < nodes.length; i++) {
    walker = insert(this, walker, nodes[i])
  }
  return ret;
}

Yallist.prototype.reverse = function () {
  var head = this.head
  var tail = this.tail
  for (var walker = head; walker !== null; walker = walker.prev) {
    var p = walker.prev
    walker.prev = walker.next
    walker.next = p
  }
  this.head = tail
  this.tail = head
  return this
}

function insert (self, node, value) {
  var inserted = node === self.head ?
    new Node(value, null, node, self) :
    new Node(value, node, node.next, self)

  if (inserted.next === null) {
    self.tail = inserted
  }
  if (inserted.prev === null) {
    self.head = inserted
  }

  self.length++

  return inserted
}

function push (self, item) {
  self.tail = new Node(item, self.tail, null, self)
  if (!self.head) {
    self.head = self.tail
  }
  self.length++
}

function unshift (self, item) {
  self.head = new Node(item, null, self.head, self)
  if (!self.tail) {
    self.tail = self.head
  }
  self.length++
}

function Node (value, prev, next, list) {
  if (!(this instanceof Node)) {
    return new Node(value, prev, next, list)
  }

  this.list = list
  this.value = value

  if (prev) {
    prev.next = this
    this.prev = prev
  } else {
    this.prev = null
  }

  if (next) {
    next.prev = this
    this.next = next
  } else {
    this.next = null
  }
}

try {
  // add if support for Symbol.iterator is present
  __webpack_require__(/*! ./iterator.js */ "./.yarn/cache/yallist-npm-4.0.0-b493d9e907-343617202a.zip/node_modules/yallist/iterator.js")(Yallist)
} catch (er) {}


/***/ }),

/***/ "./config.json":
/*!*********************!*\
  !*** ./config.json ***!
  \*********************/
/***/ ((module) => {

"use strict";
module.exports = JSON.parse('{"definitions":{"npm":{"default":"7.20.1","transparent":{"commands":[["npm","init"],["npx"]]},"ranges":{"*":{"url":"https://registry.npmjs.org/npm/-/npm-{}.tgz","bin":{"npm":"./bin/npm-cli.js","npx":"./bin/npx-cli.js"},"registry":{"type":"npm","package":"npm"}}}},"pnpm":{"default":"6.11.0","transparent":{"commands":[["pnpm","init"],["pnpx"]]},"ranges":{"<6.0.0":{"url":"https://registry.npmjs.org/pnpm/-/pnpm-{}.tgz","bin":{"pnpm":"./bin/pnpm.js","pnpx":"./bin/pnpx.js"},"registry":{"type":"npm","package":"pnpm"}},">=6.0.0":{"url":"https://registry.npmjs.org/pnpm/-/pnpm-{}.tgz","bin":{"pnpm":"./bin/pnpm.cjs","pnpx":"./bin/pnpx.cjs"},"registry":{"type":"npm","package":"pnpm"}}}},"yarn":{"default":"1.22.15","transparent":{"default":"3.0.0","commands":[["yarn","dlx"]]},"ranges":{"<2.0.0":{"url":"https://registry.yarnpkg.com/yarn/-/yarn-{}.tgz","bin":{"yarn":"./bin/yarn.js","yarnpkg":"./bin/yarn.js"},"registry":{"type":"npm","package":"yarn"}},">=2.0.0":{"name":"yarn","url":"https://repo.yarnpkg.com/{}/packages/yarnpkg-cli/bin/yarn.js","bin":["yarn","yarnpkg"],"registry":{"type":"url","url":"https://repo.yarnpkg.com/tags","fields":{"tags":"latest","versions":"tags"}}}}}}}');

/***/ }),

/***/ "./package.json":
/*!**********************!*\
  !*** ./package.json ***!
  \**********************/
/***/ ((module) => {

"use strict";
module.exports = JSON.parse('{"name":"corepack","version":"0.10.0","homepage":"https://github.com/nodejs/corepack#readme","bugs":{"url":"https://github.com/nodejs/corepack/issues"},"repository":{"type":"git","url":"https://github.com/nodejs/corepack.git"},"license":"MIT","bin":{"corepack":"./dist/corepack.js","pnpm":"./dist/pnpm.js","pnpx":"./dist/pnpx.js","yarn":"./dist/yarn.js","yarnpkg":"./dist/yarnpkg.js"},"packageManager":"yarn@3.0.0","devDependencies":{"@babel/core":"^7.14.3","@babel/plugin-proposal-class-properties":"^7.13.0","@babel/plugin-proposal-decorators":"^7.14.2","@babel/plugin-proposal-nullish-coalescing-operator":"^7.10.4","@babel/plugin-transform-modules-commonjs":"^7.14.0","@babel/preset-typescript":"^7.13.0","@types/debug":"^4.1.5","@types/jest":"^26.0.23","@types/node":"^13.9.2","@types/semver":"^7.1.0","@types/tar":"^4.0.3","@types/which":"^1.3.2","@typescript-eslint/eslint-plugin":"^2.0.0","@typescript-eslint/parser":"^4.2.0","@yarnpkg/eslint-config":"^0.1.0","@yarnpkg/fslib":"^2.1.0","@zkochan/cmd-shim":"^5.0.0","babel-plugin-dynamic-import-node":"^2.3.3","clipanion":"^3.0.1","debug":"^4.1.1","eslint":"^7.10.0","eslint-plugin-arca":"^0.9.5","jest":"^26.0.0","nock":"^13.0.4","semver":"^7.1.3","supports-color":"^7.1.0","tar":"^6.0.1","terser-webpack-plugin":"^5.1.2","ts-loader":"^8.0.2","ts-node":"^8.10.2","typescript":"^4.3.2","v8-compile-cache":"^2.3.0","webpack":"^5.38.1","webpack-cli":"^3.3.11","which":"^2.0.2"},"scripts":{"build":"rm -rf dist && webpack && ts-node ./mkshims.ts","corepack":"ts-node ./sources/main.ts","prepack":"node ./.yarn/releases/*.*js build","postpack":"rm -rf dist shims","test":"yarn jest"},"files":["dist","shims","LICENSE.md"],"publishConfig":{"executableFiles":["./dist/npm.js","./dist/npx.js","./dist/pnpm.js","./dist/pnpx.js","./dist/yarn.js","./dist/yarnpkg.js","./dist/corepack.js","./shims/npm","./shims/npm.ps1","./shims/npx","./shims/npx.ps1","./shims/pnpm","./shims/pnpm.ps1","./shims/pnpx","./shims/pnpx.ps1","./shims/yarn","./shims/yarn.ps1","./shims/yarnpkg","./shims/yarnpkg.ps1"]}}');

/***/ }),

/***/ "assert":
/*!*************************!*\
  !*** external "assert" ***!
  \*************************/
/***/ ((module) => {

"use strict";
module.exports = require("assert");;

/***/ }),

/***/ "buffer":
/*!*************************!*\
  !*** external "buffer" ***!
  \*************************/
/***/ ((module) => {

"use strict";
module.exports = require("buffer");;

/***/ }),

/***/ "child_process":
/*!********************************!*\
  !*** external "child_process" ***!
  \********************************/
/***/ ((module) => {

"use strict";
module.exports = require("child_process");;

/***/ }),

/***/ "crypto":
/*!*************************!*\
  !*** external "crypto" ***!
  \*************************/
/***/ ((module) => {

"use strict";
module.exports = require("crypto");;

/***/ }),

/***/ "events":
/*!*************************!*\
  !*** external "events" ***!
  \*************************/
/***/ ((module) => {

"use strict";
module.exports = require("events");;

/***/ }),

/***/ "fs":
/*!*********************!*\
  !*** external "fs" ***!
  \*********************/
/***/ ((module) => {

"use strict";
module.exports = require("fs");;

/***/ }),

/***/ "https":
/*!************************!*\
  !*** external "https" ***!
  \************************/
/***/ ((module) => {

"use strict";
module.exports = require("https");;

/***/ }),

/***/ "os":
/*!*********************!*\
  !*** external "os" ***!
  \*********************/
/***/ ((module) => {

"use strict";
module.exports = require("os");;

/***/ }),

/***/ "path":
/*!***********************!*\
  !*** external "path" ***!
  \***********************/
/***/ ((module) => {

"use strict";
module.exports = require("path");;

/***/ }),

/***/ "stream":
/*!*************************!*\
  !*** external "stream" ***!
  \*************************/
/***/ ((module) => {

"use strict";
module.exports = require("stream");;

/***/ }),

/***/ "string_decoder":
/*!*********************************!*\
  !*** external "string_decoder" ***!
  \*********************************/
/***/ ((module) => {

"use strict";
module.exports = require("string_decoder");;

/***/ }),

/***/ "tty":
/*!**********************!*\
  !*** external "tty" ***!
  \**********************/
/***/ ((module) => {

"use strict";
module.exports = require("tty");;

/***/ }),

/***/ "util":
/*!***********************!*\
  !*** external "util" ***!
  \***********************/
/***/ ((module) => {

"use strict";
module.exports = require("util");;

/***/ }),

/***/ "zlib":
/*!***********************!*\
  !*** external "zlib" ***!
  \***********************/
/***/ ((module) => {

"use strict";
module.exports = require("zlib");;

/***/ })

/******/ 	});
/************************************************************************/
/******/ 	// The module cache
/******/ 	var __webpack_module_cache__ = {};
/******/ 	
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/ 		// Check if module is in cache
/******/ 		var cachedModule = __webpack_module_cache__[moduleId];
/******/ 		if (cachedModule !== undefined) {
/******/ 			return cachedModule.exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = __webpack_module_cache__[moduleId] = {
/******/ 			// no module.id needed
/******/ 			// no module.loaded needed
/******/ 			exports: {}
/******/ 		};
/******/ 	
/******/ 		// Execute the module function
/******/ 		__webpack_modules__[moduleId](module, module.exports, __webpack_require__);
/******/ 	
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/ 	
/************************************************************************/
/******/ 	/* webpack/runtime/compat get default export */
/******/ 	(() => {
/******/ 		// getDefaultExport function for compatibility with non-harmony modules
/******/ 		__webpack_require__.n = (module) => {
/******/ 			var getter = module && module.__esModule ?
/******/ 				() => (module['default']) :
/******/ 				() => (module);
/******/ 			__webpack_require__.d(getter, { a: getter });
/******/ 			return getter;
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/create fake namespace object */
/******/ 	(() => {
/******/ 		var getProto = Object.getPrototypeOf ? (obj) => (Object.getPrototypeOf(obj)) : (obj) => (obj.__proto__);
/******/ 		var leafPrototypes;
/******/ 		// create a fake namespace object
/******/ 		// mode & 1: value is a module id, require it
/******/ 		// mode & 2: merge all properties of value into the ns
/******/ 		// mode & 4: return value when already ns object
/******/ 		// mode & 16: return value when it's Promise-like
/******/ 		// mode & 8|1: behave like require
/******/ 		__webpack_require__.t = function(value, mode) {
/******/ 			if(mode & 1) value = this(value);
/******/ 			if(mode & 8) return value;
/******/ 			if(typeof value === 'object' && value) {
/******/ 				if((mode & 4) && value.__esModule) return value;
/******/ 				if((mode & 16) && typeof value.then === 'function') return value;
/******/ 			}
/******/ 			var ns = Object.create(null);
/******/ 			__webpack_require__.r(ns);
/******/ 			var def = {};
/******/ 			leafPrototypes = leafPrototypes || [null, getProto({}), getProto([]), getProto(getProto)];
/******/ 			for(var current = mode & 2 && value; typeof current == 'object' && !~leafPrototypes.indexOf(current); current = getProto(current)) {
/******/ 				Object.getOwnPropertyNames(current).forEach((key) => (def[key] = () => (value[key])));
/******/ 			}
/******/ 			def['default'] = () => (value);
/******/ 			__webpack_require__.d(ns, def);
/******/ 			return ns;
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/define property getters */
/******/ 	(() => {
/******/ 		// define getter functions for harmony exports
/******/ 		__webpack_require__.d = (exports, definition) => {
/******/ 			for(var key in definition) {
/******/ 				if(__webpack_require__.o(definition, key) && !__webpack_require__.o(exports, key)) {
/******/ 					Object.defineProperty(exports, key, { enumerable: true, get: definition[key] });
/******/ 				}
/******/ 			}
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/hasOwnProperty shorthand */
/******/ 	(() => {
/******/ 		__webpack_require__.o = (obj, prop) => (Object.prototype.hasOwnProperty.call(obj, prop))
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/make namespace object */
/******/ 	(() => {
/******/ 		// define __esModule on exports
/******/ 		__webpack_require__.r = (exports) => {
/******/ 			if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 				Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 			}
/******/ 			Object.defineProperty(exports, '__esModule', { value: true });
/******/ 		};
/******/ 	})();
/******/ 	
/************************************************************************/
var __webpack_exports__ = {};
// This entry need to be wrapped in an IIFE because it need to be in strict mode.
(() => {
"use strict";
/*!*************************!*\
  !*** ./sources/main.ts ***!
  \*************************/
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "main": () => (/* binding */ main),
/* harmony export */   "runMain": () => (/* binding */ runMain)
/* harmony export */ });
/* harmony import */ var clipanion__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(/*! clipanion */ "./.yarn/__virtual__/clipanion-virtual-119dc92083/0/cache/clipanion-npm-3.0.1-901533eeed-3a4b0c1e7d.zip/node_modules/clipanion/lib/advanced/index.js");
/* harmony import */ var _Engine__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./Engine */ "./sources/Engine.ts");
/* harmony import */ var _commands_Disable__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./commands/Disable */ "./sources/commands/Disable.ts");
/* harmony import */ var _commands_Enable__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ./commands/Enable */ "./sources/commands/Enable.ts");
/* harmony import */ var _commands_Hydrate__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ./commands/Hydrate */ "./sources/commands/Hydrate.ts");
/* harmony import */ var _commands_Prepare__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ./commands/Prepare */ "./sources/commands/Prepare.ts");
/* harmony import */ var _miscUtils__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ./miscUtils */ "./sources/miscUtils.ts");
/* harmony import */ var _corepackUtils__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(/*! ./corepackUtils */ "./sources/corepackUtils.ts");
/* harmony import */ var _specUtils__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(/*! ./specUtils */ "./sources/specUtils.ts");









function getPackageManagerRequestFromCli(parameter, context) {
    if (!parameter)
        return null;
    const match = parameter.match(/^([^@]*)(?:@(.*))?$/);
    if (!match)
        return null;
    const [, binaryName, binaryVersion] = match;
    const packageManager = context.engine.getPackageManagerFor(binaryName);
    if (!packageManager)
        return null;
    return {
        packageManager,
        binaryName,
        binaryVersion: binaryVersion || null,
    };
}
async function executePackageManagerRequest({ packageManager, binaryName, binaryVersion }, args, context) {
    var _a;
    const defaultVersion = await context.engine.getDefaultVersion(packageManager);
    const definition = context.engine.config.definitions[packageManager];
    // If all leading segments match one of the patterns defined in the `transparent`
    // key, we tolerate calling this binary even if the local project isn't explicitly
    // configured for it, and we use the special default version if requested.
    let isTransparentCommand = false;
    for (const transparentPath of definition.transparent.commands) {
        if (transparentPath[0] === binaryName && transparentPath.slice(1).every((segment, index) => segment === args[index])) {
            isTransparentCommand = true;
            break;
        }
    }
    const fallbackReference = isTransparentCommand
        ? (_a = definition.transparent.default) !== null && _a !== void 0 ? _a : defaultVersion
        : defaultVersion;
    const fallbackLocator = {
        name: packageManager,
        reference: fallbackReference,
    };
    let descriptor;
    try {
        descriptor = await _specUtils__WEBPACK_IMPORTED_MODULE_7__.findProjectSpec(context.cwd, fallbackLocator, { transparent: isTransparentCommand });
    }
    catch (err) {
        if (err instanceof _miscUtils__WEBPACK_IMPORTED_MODULE_5__.Cancellation) {
            return 1;
        }
        else {
            throw err;
        }
    }
    if (binaryVersion)
        descriptor.range = binaryVersion;
    const resolved = await context.engine.resolveDescriptor(descriptor, { allowTags: true });
    if (resolved === null)
        throw new clipanion__WEBPACK_IMPORTED_MODULE_8__.UsageError(`Failed to successfully resolve '${descriptor.range}' to a valid ${descriptor.name} release`);
    const installSpec = await context.engine.ensurePackageManager(resolved);
    const exitCode = await _corepackUtils__WEBPACK_IMPORTED_MODULE_6__.runVersion(installSpec, resolved, binaryName, args, context);
    return exitCode;
}
async function main(argv, context) {
    const corepackVersion = __webpack_require__(/*! ../package.json */ "./package.json").version;
    const [firstArg, ...restArgs] = argv;
    const request = getPackageManagerRequestFromCli(firstArg, context);
    let cli;
    if (!request) {
        // If the first argument doesn't match any supported package manager, we fallback to the standard Corepack CLI
        cli = new clipanion__WEBPACK_IMPORTED_MODULE_8__.Cli({
            binaryLabel: `Corepack`,
            binaryName: `corepack`,
            binaryVersion: corepackVersion,
        });
        cli.register(clipanion__WEBPACK_IMPORTED_MODULE_8__.Builtins.HelpCommand);
        cli.register(clipanion__WEBPACK_IMPORTED_MODULE_8__.Builtins.VersionCommand);
        cli.register(_commands_Enable__WEBPACK_IMPORTED_MODULE_2__.EnableCommand);
        cli.register(_commands_Disable__WEBPACK_IMPORTED_MODULE_1__.DisableCommand);
        cli.register(_commands_Hydrate__WEBPACK_IMPORTED_MODULE_3__.HydrateCommand);
        cli.register(_commands_Prepare__WEBPACK_IMPORTED_MODULE_4__.PrepareCommand);
        return await cli.run(argv, Object.assign(Object.assign({}, clipanion__WEBPACK_IMPORTED_MODULE_8__.Cli.defaultContext), context));
    }
    else {
        // Otherwise, we create a single-command CLI to run the specified package manager (we still use Clipanion in order to pretty-print usage errors).
        const cli = new clipanion__WEBPACK_IMPORTED_MODULE_8__.Cli({
            binaryLabel: `'${request.binaryName}', via Corepack`,
            binaryName: request.binaryName,
            binaryVersion: `corepack/${corepackVersion}`,
        });
        cli.register(class BinaryCommand extends clipanion__WEBPACK_IMPORTED_MODULE_8__.Command {
            constructor() {
                super(...arguments);
                this.proxy = clipanion__WEBPACK_IMPORTED_MODULE_8__.Option.Proxy();
            }
            async execute() {
                return executePackageManagerRequest(request, this.proxy, this.context);
            }
        });
        return await cli.run(restArgs, Object.assign(Object.assign({}, clipanion__WEBPACK_IMPORTED_MODULE_8__.Cli.defaultContext), context));
    }
}
function runMain(argv) {
    main(argv, {
        cwd: process.cwd(),
        engine: new _Engine__WEBPACK_IMPORTED_MODULE_0__.Engine(),
    }).then(exitCode => {
        process.exitCode = exitCode;
    }, err => {
        console.error(err.stack);
        process.exitCode = 1;
    });
}
// Using `eval` to be sure that Webpack doesn't transform it
if (process.mainModule === eval(`module`))
    runMain(process.argv.slice(2));

})();

var __webpack_export_target__ = exports;
for(var i in __webpack_exports__) __webpack_export_target__[i] = __webpack_exports__[i];
if(__webpack_exports__.__esModule) Object.defineProperty(__webpack_export_target__, "__esModule", { value: true });
/******/ })()
;