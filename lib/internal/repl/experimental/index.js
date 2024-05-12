'use strict';

const {
  ArrayFrom,
  ArrayPrototypeAt,
  ArrayPrototypeFilter,
  ArrayPrototypeFind,
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypeMap,
  FunctionPrototypeBind,
  MathFloor,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyNames,
  ReflectApply,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolSplit,
  SafeMap,
  StringPrototypeIndexOf,
  StringPrototypeRepeat,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  StringPrototypeSubstring,
  StringPrototypeTrim,
  Symbol,
} = primordials;

const { createInterface, clearScreenDown } = require('readline');
const { parse: acornParse } = require('internal/deps/acorn/acorn/dist/acorn');
const highlight = require('internal/repl/experimental/highlight');
const walk = require('internal/deps/acorn/acorn-walk/dist/walk');
const {
  isIdentifier,
  strEscape,
  underlineIgnoreANSI,
  isStaticIdentifier,
  isScopeOpen,
  stripVTControlCharacters: stripAnsi,
  acornParseOptions,
} = require('internal/repl/experimental/util');
const getHistory = require('internal/repl/experimental/history');
const {
  inspect: utilInspect,
  styleText: utilStyleText,
} = require('util');
const { createContext, runInNewContext: vmRunInNewContext, Script } = require('vm');
const { runInContext } = Script.prototype;
const { Module: CJSModule } = require('internal/modules/cjs/loader');
const { makeRequireFunction, addBuiltinLibsToObject } = require('internal/modules/helpers');
const { makeContextifyScript } = require('internal/vm');
const { getOrInitializeCascadedLoader } = require('internal/modules/esm/loader');
const { pathToFileURL } = require('internal/url');
const { emitExperimentalWarning, kEmptyObject } = require('internal/util');
const {
  join: pathJoin,
} = require('path');
const EventEmitter = require('events');

function makePrompt(i) {
  return utilStyleText('green', `In [${utilStyleText('bold', '' + i)}]: `);
}

function makePromptOut(inspected, i) {
  if (RegExpPrototypeExec(/[\r\n\u2028\u2029]/u, inspected)) {
    return '';
  }
  return utilStyleText('red', `Out[${utilStyleText('bold', '' + i)}]: `);
}

function promptLength(i) {
  return `In [${i}]: `.length;
}

function importModuleDynamically(specifier, _, importAttributes) {
  const cascadedLoader = getOrInitializeCascadedLoader();
  return cascadedLoader.import(specifier, pathToFileURL(pathJoin(process.cwd(), 'repl')).href, importAttributes);
}

const wrappedRegexPos = /^\s*{/;
const wrappedRegexNeg = /;\s*$/;
class ExperimentalREPLServer extends EventEmitter {
  #options;

  constructor() {
    super();

    this.context = createContext();

    this.replCommands = new SafeMap([
      ['help', {
        __proto__: null,
        help: 'Show this help message',
        fn: FunctionPrototypeBind(this.#showHelp, this),
      }],
      ['exit', {
        __proto__: null,
        help: 'Exit the REPL',
        fn: FunctionPrototypeBind(() => {
          this.close();
          process.exit();
        }, this),
      }],
      ['clear', {
        __proto__: null,
        help: 'Clear the screen',
        fn: FunctionPrototypeBind(() => {
          this.#options.output.cursorTo(0, 0);
          clearScreenDown(this.#options.output);
          this.rl.prompt();
        }, this),
      }],
    ]);

    this.#initialize();
  }

  defineCommand(name, help, fn) {
    this.replCommands.set(name, {
      __proto__: null,
      help,
      fn,
    });
  }

  #showHelp() {
    const commands = ArrayFrom(this.replCommands.entries(), (entry) => {
      return `.${entry[0]}: ${entry[1].help}`;
    });
    return `Node.js ${process.versions.node} (V8 ${process.versions.v8})\n\n${commands.join('\n')}`;
  }

  #getGlobalNames() {
    return ArrayPrototypeFilter(ObjectGetOwnPropertyNames(this.context), (key) => !StringPrototypeStartsWith(key, '_'));
  }

  evaluate(source) {
    const wrapped = RegExpPrototypeExec(wrappedRegexPos, source) && !RegExpPrototypeExec(wrappedRegexNeg, source) ? `(${source})` : source;
    const contextified = makeContextifyScript(
      wrapped,
      'repl',
      0,
      0,
      undefined,
      false,
      undefined,
      Symbol('repl'),
      importModuleDynamically,
    );

    return ReflectApply(runInContext, contextified, [this.context, {
      displayErrors: false,
    }]);
  }

  complete(line) {
    if (line.length === 0) {
      return {
        fillable: true,
        completions: this.#getGlobalNames(),
      };
    }

    if (line[0] === '.') {
      const commands = ArrayPrototypeFilter(
        ArrayFrom(this.replCommands.keys()),
        (key) => StringPrototypeStartsWith(key, StringPrototypeSlice(line, 1)),
      );
      return {
        fillable: true,
        completions: ArrayPrototypeMap(commands, (c) => StringPrototypeSlice(c, line.length - 1)),
      };
    }

    if (ArrayPrototypeAt(line, -1) === ' ') return undefined;

    if (ArrayPrototypeAt(line, -1) === '.' || StringPrototypeSlice(line, -2, -1) === '[') {
      const lastObject = StringPrototypeSlice(line, -2, -1) === '[' ?
        StringPrototypeSlice(line, 0, -2) : StringPrototypeSlice(line, 0, -1);

      if (!isStaticIdentifier(lastObject)) return undefined;

      let evaluation;
      try {
        evaluation = this.evaluate(lastObject);
      } catch {
        return undefined;
      }

      return {
        fillable: true,
        completions: ObjectGetOwnPropertyNames(evaluation),
      };
    }

    let { node: expression } = walk.findNodeAround(
      acornParse(line, acornParseOptions),
      line.length,
      (type) => type === 'MemberExpression' || type === 'Identifier',
    );

    if (expression.operator === 'void') {
      expression = expression.argument;
    }

    let keys;
    let filter;

    if (expression.type === 'Identifier') {
      keys = this.#getGlobalNames();
      filter = expression.name;

      if (ArrayPrototypeIncludes(keys, filter)) {
        return undefined;
      }
    } else if (expression.type === 'MemberExpression') {
      const expr = StringPrototypeSlice(line, expression.object.start, expression.object.end);
      if (expression.computed && expression.property.type === 'Literal') {
        filter = expression.property.raw;
      } else if (expression.property.type === 'Identifier') {
        // eslint-disable-next-line node-core/non-ascii-character
        if (expression.property.name === '✖') {
          filter = undefined;
        } else {
          filter = expression.property.name;
          if (expression.computed) {
            keys = this.#getGlobalNames();
          }
        }
      } else {
        return undefined;
      }

      if (!keys) {
        let evaluateResult;
        try {
          evaluateResult = this.evaluate(expr, true);
        } catch {
          return undefined;
        }

        keys = ObjectGetOwnPropertyNames(evaluateResult);
        if (keys.length === 0) {
          return undefined;
        }

        if (expression.computed) {
          if (ArrayPrototypeAt(line, -1) === '[') {
            return undefined;
          }

          keys = ArrayPrototypeMap(keys, (key) => {
            let r;
            if (`${+key}` === key) {
              r = key;
            } else {
              r = strEscape(key);
            }
            return `${r}]`;
          });
        } else {
          keys = ArrayPrototypeFilter(keys, isIdentifier);
        }
      }
    }

    if (keys) {
      if (filter) {
        keys = ArrayPrototypeMap(
          ArrayPrototypeFilter(keys, (k) => k !== filter && StringPrototypeStartsWith(k, filter)),
          (k) => StringPrototypeSlice(k, filter.length),
        );
      }
      return { fillable: true, completions: keys };
    }

    return undefined;
  }

  preview(line) {
    try {
      const { node: expression } = walk.findNodeAround(
        acornParse(line, acornParseOptions),
        line.length,
        (type) => type === 'MemberExpression' || type === 'Identifier',
      );

      const expr = StringPrototypeSubstring(line, expression.start, expression.end);
      if (!isStaticIdentifier(expr)) return undefined;

      const result = this.evaluate(expr);
      const inspected = utilInspect(result, {
        colors: false,
        breakLength: Infinity,
        compact: true,
        maxArrayLength: 10,
        depth: 1,
      });

      let noAnsi = stripAnsi(inspected);
      const length = this.#options.output.columns - promptLength(this.promptIndex) - 2;
      if (noAnsi.length > length) {
        noAnsi = StringPrototypeSlice(noAnsi, 0, length - 3) + '...';
      }

      return noAnsi;
    } catch {
      return undefined;
    }
  }

  #countLines(line) {
    let count = 0;
    ArrayPrototypeForEach(RegExpPrototypeSymbolSplit(/\r?\n/, line), (inner) => {
      inner = stripAnsi(inner);
      count += 1;
      if (inner.length > this.#options.output.columns) {
        count += MathFloor(inner.length / this.#options.output.columns);
      }
    });
    return count;
  }

  #initialize() {
    const replModule = new CJSModule('<repl>');
    replModule.paths = CJSModule._resolveLookupPaths('<repl>', module, true);
    ObjectDefineProperty(this.context, 'module', {
      __proto__: null,
      configurable: true,
      writable: true,
      value: replModule,
    });
    ObjectDefineProperty(this.context, 'require', {
      __proto__: null,
      configurable: true,
      writable: true,
      value: makeRequireFunction(replModule),
    });

    addBuiltinLibsToObject(this.context, '<REPL>');

    const globals = vmRunInNewContext('Object.getOwnPropertyNames(globalThis)');

    // eslint-disable-next-line no-restricted-globals
    ArrayPrototypeForEach(ObjectGetOwnPropertyNames(globalThis), (key) => {
      if (!ArrayPrototypeIncludes(globals, key)) {
        ObjectDefineProperty(this.context, key, {
          // eslint-disable-next-line no-restricted-globals
          ...ObjectGetOwnPropertyDescriptor(globalThis, key),
          __proto__: null,
        });
      }
    });

    ArrayPrototypeForEach(['_', '__', '___', '_err'], (prop) => {
      ObjectDefineProperty(this.context, prop, {
        __proto__: null,
        value: undefined,
        writable: true,
        enumerable: false,
        configurable: true,
      });
    });

    process.on('uncaughtException', (e) => {
      this.#options.output.write(`Uncaught ${utilInspect(e)}\n`);
    });

    process.on('unhandledRejection', (reason) => {
      this.#options.output.write(`Unhandled ${utilInspect(reason)}\n`);
    });

    this.context.global = this.context;
  }

  updateInspect(uncaught, line, value) {
    if (uncaught) {
      this.context._err = value;
    } else {
      this.context.___ = this.context.__;
      this.context.__ = this.context._;
      this.context._ = value;
      ObjectDefineProperty(this.context, `_${line}`, {
        __proto__: null,
        value,
        writable: true,
        enumerable: false,
        configurable: true,
      });
    }
    return utilInspect(value, {
      colors: true,
      showProxy: true,
    });
  }

  close() {
    this.rl?.removeAllListeners();
    this.rl?.close();
    this.history?.closeHandle();
    this.emit('exit');
  }

  async onLine(line) {
    if (!this.cache && line[0] === '.') {
      const command = line.slice(1);
      const cmd = this.replCommands.get(command);
      if (cmd) {
        const result = cmd.fn();
        this.#options.output.write(`${result}\n`);
      } else {
        this.#options.output.write(`Unknown command: ${command}\n`);
      }
      this.rl.prompt();
      return;
    }

    if (isScopeOpen(line)) {
      this.cache += line;
      this.rl.setPrompt(StringPrototypeRepeat('.', promptLength(this.promptIndex)));
      this.rl.prompt();
      this.rl.history.unshift(line);
      return;
    } else if (this.cache) {
      line = StringPrototypeTrim(this.cache) + StringPrototypeTrim(line);
      this.cache = '';
      this.rl.history[0] = line;
    }

    this.rl.pause();
    clearScreenDown(this.#options.output);

    let result;
    let uncaught = false;
    try {
      result = this.evaluate(line);
    } catch (error) {
      result = error;
      uncaught = true;
    }

    const inspected = this.updateInspect(uncaught, this.promptIndex, result);

    this.#options.output.write(`${makePromptOut(inspected, this.promptIndex)}${uncaught ? 'Uncaught ' : ''}${inspected}\n\n`);

    this.promptIndex++;
    this.rl.setPrompt(makePrompt(this.promptIndex));

    await this.history.writeHistory(this.rl.history);

    this.rl.resume();
    this.rl.prompt();
  }

  static async start(options = kEmptyObject) {
    const repl = new ExperimentalREPLServer();
    await repl.start(options);
    return repl;
  }

  async start(options = kEmptyObject) {
    this.#options = {
      input: options.input || process.stdin,
      output: options.output || process.stdout,
    };

    this.rl = createInterface({
      input: this.#options.input,
      output: this.#options.output,
      prompt: makePrompt(1),
      completer: (line, cb) => {
        if (this.cache) return cb(null, [[], line]);
        try {
          const completion = this.complete(line);
          if (completion.fillable) {
            cb(null, [ArrayPrototypeMap(completion.completions || [], (l) => line + l), line]);
          } else {
            cb(null, [[], line]);
          }
        } catch {
          cb(null, [[], line]);
        }
      },
      postprocessor: (line) => highlight(line),
    });

    this.rl.pause();

    if (!this.rl.postprocessor) {
      this.rl._insertString = (c) => {
        const beg = StringPrototypeSlice(this.rl.line, 0, this.rl.cursor);
        const end = StringPrototypeSlice(this.rl.line, this.rl.cursor, this.rl.line.length);
        this.rl.line = beg + c + end;
        this.rl.cursor += c.length;
        this.rl._refreshLine();
      };
    }

    this.history = await getHistory();
    this.rl.history = this.history.history;

    let MODE = 'NORMAL';
    this.promptIndex = 1;
    let nextCtrlCKills = false;
    let nextCtrlDKills = false;

    this.rl.on('SIGINT', () => {
      nextCtrlDKills = false;
      if (MODE === 'REVERSE') {
        MODE = 'NORMAL';
        this.#options.output.moveCursor(0, -1);
        this.#options.output.cursorTo(0);
        this.rl._refreshLine();
      } else if (this.rl.line.length) {
        this.rl.line = '';
        this.rl.cursor = 0;
        this.rl._refreshLine();
      } else if (this.cache) {
        this.cache = '';
        this.rl.setPrompt(makePrompt(this.promptIndex));
        this.rl.line = '';
        this.rl.cursor = 0;
        this.rl._refreshLine();
      } else if (nextCtrlCKills) {
        this.close();
        process.exit();
      } else {
        nextCtrlCKills = true;
        this.#options.output.write(`\n(To exit, press ^C again)\n${this.rl.getPrompt()}`);
      }
    });

    let completionCache;
    const ttyWrite = this.rl._ttyWrite.bind(this.rl);

    this.rl._ttyWrite = (d, key) => {

      if (key.name === 'tab') {
        clearScreenDown(this.#options.output);
      }

      if (!(key.ctrl && key.name === 'c')) {
        nextCtrlCKills = false;
      }

      if (key.ctrl && key.name === 'd') {
        if (nextCtrlDKills) process.exit();
        nextCtrlDKills = true;
        this.#options.output.write(`\n(To exit, press ^D again)\n${this.rl.getPrompt()}`);
        return;
      }

      nextCtrlDKills = false;
      if (key.ctrl && key.name === 'r' && MODE === 'NORMAL') {
        MODE = 'REVERSE';
        this.#options.output.write('\n');
        this.rl._refreshLine();
        return;
      }

      if (key.name === 'return' && MODE === 'REVERSE') {
        MODE = 'NORMAL';
        const match = ArrayPrototypeFind(this.rl.history, (h) => ArrayPrototypeIncludes(h, this.rl.line));
        this.#options.output.moveCursor(0, -1);
        this.#options.output.cursorTo(0);
        this.#options.output.clearScreenDown();
        this.rl.cursor = StringPrototypeIndexOf(match, this.rl.line) + this.rl.line.length;
        this.rl.line = match;
        this.rl._refreshLine();
        return;
      }

      ttyWrite(d, key);

      if (key.name === 'right' && this.rl.cursor === this.rl.line.length) {
        if (completionCache) {
          this.rl._insertString(completionCache);
        }
      }
    };

    const refreshLine = FunctionPrototypeBind(this.rl._refreshLine, this.rl);
    this.rl._refreshLine = () => {
      completionCache = undefined;
      const inspectedLine = this.rl.line;

      if (MODE === 'REVERSE') {
        this.#options.output.moveCursor(0, -1);
        this.#options.output.cursorTo(promptLength(this.promptIndex));
        clearScreenDown(this.#options.output);
        let match;
        if (inspectedLine) {
          match = ArrayPrototypeFind(this.rl.history, (h) => ArrayPrototypeIncludes(h, inspectedLine));
        }
        if (match) {
          match = highlight(match);
          match = underlineIgnoreANSI(match, inspectedLine);
        }
        this.#options.output.write(`${match || ''}\n(reverse-i-search): ${inspectedLine}`);
        this.#options.output.cursorTo('(reverse-i-search): '.length + this.rl.cursor);
        return;
      }

      if (this.rl.postprocessor === undefined) {
        this.rl.line = highlight(inspectedLine);
      }
      refreshLine();
      this.rl.line = inspectedLine;

      this.#options.output.cursorTo(promptLength(this.promptIndex) + this.rl.cursor);

      if (inspectedLine !== '') {
        try {
          const completion = this.complete(this.cache + inspectedLine);
          const fullLine = this.cache + inspectedLine + (completion?.completions[0] || '');

          const preview = this.preview(fullLine);

          if (this.rl.line !== inspectedLine) {
            return;
          }

          let rows = 0;
          if (completion && completion.completions.length > 0) {
            if (completion.fillable) {
              completionCache = completion.completions[0];
            }
            this.#options.output.cursorTo(promptLength(this.promptIndex) + this.rl.line.length);
            this.#options.output.write(utilStyleText('grey', completion.completions[0]));
            rows += this.#countLines(completion.completions[0]) - 1;
          }

          if (preview) {
            this.#options.output.write(utilStyleText('grey', `\nOut[${this.promptIndex}]: ${preview}\n`));
            rows += this.#countLines(preview) + 1;
          }

          this.#options.output.cursorTo(promptLength(this.promptIndex) + this.rl.cursor);
          this.#options.output.moveCursor(0, -rows);
        } catch {
          // We ignore the error because we don't care about eval issues.
        }
      }
    };

    emitExperimentalWarning('Experimental REPL');

    this.#options.output.write(
      'Node.js ' + process.versions.node + ' (V8 ' + process.versions.v8 + ')\n' +
      '(Highly) Experimental REPL\n\n',
    );

    this.rl.resume();
    this.rl.prompt();

    this.cache = '';
    this.rl.on('line', FunctionPrototypeBind(this.onLine, this));
  }
}

module.exports = ExperimentalREPLServer;
