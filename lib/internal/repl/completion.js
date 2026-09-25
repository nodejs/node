'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypeMap,
  ArrayPrototypePop,
  ArrayPrototypePush,
  ArrayPrototypePushApply,
  ArrayPrototypeShift,
  ArrayPrototypeSlice,
  ArrayPrototypeSome,
  ArrayPrototypeSort,
  ArrayPrototypeUnshift,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ReflectApply,
  RegExpPrototypeExec,
  SafeSet,
  StringPrototypeCharCodeAt,
  StringPrototypeCodePointAt,
  StringPrototypeEndsWith,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  StringPrototypeToLocaleLowerCase,
  StringPrototypeTrimStart,
} = primordials;

const {
  kContextId,
  getGlobalBuiltins,
  getReplBuiltinLibs,
  fixReplRequire,
} = require('internal/repl/utils');

const { getReplInspector } = require('internal/repl/inspector');

const CJSModule = require('internal/modules/cjs/loader').Module;

const {
  extensionFormatMap,
} = require('internal/modules/esm/get_format');

const path = require('path');
const fs = require('fs');

const {
  constants: {
    ALL_PROPERTIES,
    SKIP_SYMBOLS,
  },
  getOwnNonIndexProperties,
} = internalBinding('util');

const replCommandRE = /^\s*\.(\w*)$/;
const importRE = /\bimport\s*(?:\(\s*|[^'"`]*?\bfrom\s*)?['"`](([\w@./:-]+\/)?(?:[\w@./:-]*))(?![^'"`])$/;
const requireRE = /\brequire\s*\(\s*['"`](([\w@./:-]+\/)?(?:[\w@./:-]*))(?![^'"`])$/;
const fsAutoCompleteRE = /fs(?:\.promises)?\.\s*[a-z][a-zA-Z]+\(\s*["'](.*)/;
const versionedFileNamesRe = /-\d+\.\d+/;
const relativePathRE = /^\.\.?\//;
const completableLastCharRE = /\w|\.|\$/;
const declarationKeywordRE = /(?:^|[^\w$])(?:var|let|const|class|function)\s+$/;

const kCompletionObjectGroup = 'node-repl-completion';

fixReplRequire(module);

const { BuiltinModule } = require('internal/bootstrap/realm');

const nodeSchemeBuiltinLibs = ArrayPrototypeMap(getReplBuiltinLibs(), (lib) => `node:${lib}`);
ArrayPrototypeForEach(
  BuiltinModule.getSchemeOnlyModuleNames(),
  (lib) => ArrayPrototypePush(nodeSchemeBuiltinLibs, `node:${lib}`),
);

async function complete(line, repl) {
  // Ignore leading whitespace; it could change the outcome (e.g. a lone space
  // should still offer the global scope rather than nothing).
  line = StringPrototypeTrimStart(line);

  let match;
  let result;
  if ((match = RegExpPrototypeExec(replCommandRE, line)) !== null) {
    result = completeReplCommand(repl, match);
  } else if ((match = RegExpPrototypeExec(requireRE, line)) !== null) {
    result = completeRequireSpecifier(repl, match);
  } else if ((match = RegExpPrototypeExec(importRE, line)) !== null) {
    result = completeImportSpecifier(repl, match);
  } else if ((match = RegExpPrototypeExec(fsAutoCompleteRE, line)) !== null &&
             repl.allowBlockingCompletions) {
    result = completeFsPath(match);
  } else if (line.length === 0 ||
             RegExpPrototypeExec(completableLastCharRE, line[line.length - 1]) !== null) {
    result = await completeExpression(repl, line);
  } else {
    result = emptyCompletion();
  }

  return finalizeCompletions(result.completionGroups, result.filter, result.completeOn);
}

/**
 * Filters, sorts, de-duplicates and merges the completion groups into the
 * `[completions, completeOn]` tuple readline expects.
 * @returns {[string[], string]} The completions and the string to complete on.
 */
function finalizeCompletions(completionGroups, filter, completeOn) {
  if (completionGroups.length && filter) {
    const lowerCaseFilter = StringPrototypeToLocaleLowerCase(filter);
    const newCompletionGroups = [];
    ArrayPrototypeForEach(completionGroups, (group) => {
      const filteredGroup = ArrayPrototypeFilter(group, (str) =>
        // Filtering is always case-insensitive, following chromium autocomplete
        // behavior.
        StringPrototypeStartsWith(
          StringPrototypeToLocaleLowerCase(str), lowerCaseFilter),
      );
      if (filteredGroup.length) {
        ArrayPrototypePush(newCompletionGroups, filteredGroup);
      }
    });
    completionGroups = newCompletionGroups;
  }

  const completions = [];
  // Unique completions across all groups.
  const uniqueSet = new SafeSet();
  uniqueSet.add('');
  ArrayPrototypeForEach(completionGroups, (group) => {
    ArrayPrototypeSort(group, (a, b) => (b > a ? 1 : -1));
    const setSize = uniqueSet.size;
    ArrayPrototypeForEach(group, (entry) => {
      if (!uniqueSet.has(entry)) {
        ArrayPrototypeUnshift(completions, entry);
        uniqueSet.add(entry);
      }
    });
    // Add a separator between groups.
    if (uniqueSet.size !== setSize) {
      ArrayPrototypeUnshift(completions, '');
    }
  });

  // Remove obsolete group entry, if present.
  if (completions[0] === '') {
    ArrayPrototypeShift(completions);
  }

  return [completions, completeOn];
}

// A result with nothing to complete (also the shape every handler returns).
function emptyCompletion() {
  return { __proto__: null, completionGroups: [], completeOn: undefined, filter: '' };
}

// Completes REPL dot-commands, e.g. `.bre` -> `.break`.
function completeReplCommand(repl, match) {
  const completeOn = match[1];
  return {
    __proto__: null,
    completionGroups: [ObjectKeys(repl.commands)],
    completeOn,
    filter: completeOn,
  };
}

// Completes the specifier inside `require('...')` with the builtin module names
// and, when blocking completions are allowed, on-disk files and directories.
function completeRequireSpecifier(repl, match) {
  return completeModuleSpecifier(repl, match, {
    __proto__: null,
    // `CJSModule._extensions` can be augmented at runtime (e.g. by loaders), so
    // it is read on every completion rather than cached at module load.
    extensions: ObjectKeys(CJSModule._extensions),
    extraIndexes: ['package.json', 'index'],
    getBasePaths() {
      const paths = [];
      ArrayPrototypePushApply(paths, module.paths);
      ArrayPrototypePushApply(paths, CJSModule.globalPaths);
      return paths;
    },
    // `require('foo')` resolves without the extension, and `require('foo/index')`
    // is redundant with `require('foo')`.
    formatFile(subdir, name, extension) {
      const base = StringPrototypeSlice(name, 0, name.length - extension.length);
      if (subdir && base === 'index') {
        return null;
      }
      return `${subdir}${base}`;
    },
    // CommonJS resolution treats any directory holding an index/`package.json`
    // as requirable, not only those under `node_modules`.
    listsDirectoryIndex() {
      return true;
    },
  });
}

// Completes the specifier inside `import('...')` with the builtin module names
// and, when blocking completions are allowed, importable on-disk files and
// directories.
function completeImportSpecifier(repl, match) {
  return completeModuleSpecifier(repl, match, {
    __proto__: null,
    extensions: ObjectKeys(extensionFormatMap),
    extraIndexes: ['package.json'],
    getBasePaths() {
      return ArrayPrototypeSlice(module.paths);
    },
    // `import()` needs the explicit file extension.
    formatFile(subdir, name) {
      return `${subdir}${name}`;
    },
    // Only bare specifiers resolved from `node_modules` honor a directory's
    // index/`package.json`; relative imports must be spelled out in full.
    listsDirectoryIndex(subdir, isInNodeModules) {
      return !subdir && isInNodeModules;
    },
  });
}

// Shared driver for `require('...')` and `import('...')` specifier completion.
// Both offer the builtin module names plus, when blocking completions are
// allowed, the files and directories reachable from a set of base paths; the
// `options` object captures the few places their resolution rules differ.
function completeModuleSpecifier(repl, match, options) {
  const completeOn = match[1];
  const completionGroups = [];

  if (repl.allowBlockingCompletions) {
    const { extensions, getBasePaths, formatFile, listsDirectoryIndex } = options;
    const subdir = match[2] || '';
    // Filenames that mark a directory as loadable without a trailing slash.
    const indexes = ArrayPrototypeMap(extensions, (ext) => `index${ext}`);
    ArrayPrototypePushApply(indexes, options.extraIndexes);

    const group = [];
    let paths = [];
    if (completeOn === '.') {
      ArrayPrototypePush(group, './', '../');
    } else if (completeOn === '..') {
      ArrayPrototypePush(group, '../');
    } else if (RegExpPrototypeExec(relativePathRE, completeOn) !== null) {
      paths = [process.cwd()];
    } else {
      paths = getBasePaths();
    }

    ArrayPrototypeForEach(paths, (dir) => {
      dir = path.resolve(dir, subdir);
      const isInNodeModules = path.basename(dir) === 'node_modules';
      const dirents = gracefulReaddir(dir, { withFileTypes: true }) || [];
      ArrayPrototypeForEach(dirents, (dirent) => {
        const { name } = dirent;
        if (RegExpPrototypeExec(versionedFileNamesRe, name) !== null ||
            name === '.npm') {
          // Exclude versioned names that 'npm' installs.
          return;
        }

        if (!dirent.isDirectory()) {
          const extension = path.extname(name);
          if (ArrayPrototypeIncludes(extensions, extension)) {
            const completion = formatFile(subdir, name, extension);
            if (completion !== null) {
              ArrayPrototypePush(group, completion);
            }
          }
          return;
        }

        ArrayPrototypePush(group, `${subdir}${name}/`);
        if (listsDirectoryIndex(subdir, isInNodeModules) &&
            hasIndexFile(path.resolve(dir, name), indexes)) {
          ArrayPrototypePush(group, `${subdir}${name}`);
        }
      });
    });
    if (group.length) {
      ArrayPrototypePush(completionGroups, group);
    }
  }

  ArrayPrototypePush(completionGroups, getReplBuiltinLibs(), nodeSchemeBuiltinLibs);
  return { __proto__: null, completionGroups, completeOn, filter: completeOn };
}

// Whether `dir` contains any of the `indexes` files, which makes it loadable by
// its bare name (e.g. `require('foo')` resolving to `foo/index.js`).
function hasIndexFile(dir, indexes) {
  return ArrayPrototypeSome(
    gracefulReaddir(dir) || [],
    (subfile) => ArrayPrototypeIncludes(indexes, subfile),
  );
}

// Completes a filesystem path passed to an `fs` function, e.g.
// `fs.readFile('./<Tab>')`.
function completeFsPath(match) {
  let baseName = '';
  let filePath = match[1];
  let fileList = gracefulReaddir(filePath, { withFileTypes: true });

  if (!fileList) {
    baseName = path.basename(filePath);
    filePath = path.dirname(filePath);
    fileList = gracefulReaddir(filePath, { withFileTypes: true }) || [];
  }

  const completions = ArrayPrototypeMap(
    ArrayPrototypeFilter(
      fileList,
      (dirent) => StringPrototypeStartsWith(dirent.name, baseName),
    ),
    (dirent) => dirent.name,
  );

  return {
    __proto__: null,
    completionGroups: [completions],
    completeOn: baseName,
    filter: '',
  };
}

/**
 * Completes a trailing expression: either a member access (`obj.foo`) or a bare
 * identifier against the global/lexical scope (`Strin`).
 * @returns {Promise<{completionGroups: string[][], completeOn: string, filter: string}>}
 */
async function completeExpression(repl, line) {
  const analysis = analyzeCompletion(line);
  if (analysis === null) {
    return emptyCompletion();
  }

  const { receiver, chaining, completeOn } = analysis;
  // Completions are emitted fully qualified (e.g. `console.log`), so filtering
  // happens against the whole text being completed.
  const filter = completeOn;

  const completionGroups = receiver === null ?
    await completeGlobalScope(repl, filter) :
    await completeMemberExpression(
      getReplInspector(repl), receiver, chaining, repl[kContextId]);

  return { __proto__: null, completionGroups, completeOn, filter };
}

/**
 * Lists completion groups for a bare identifier
 * @returns {Promise<string[][]>} The completion groups
 */
async function completeGlobalScope(repl, filter) {
  const completionGroups = [];

  const lexicalNames =
    await getReplInspector(repl).globalLexicalScopeNames(repl[kContextId]);
  ArrayPrototypePush(completionGroups, lexicalNames);

  let contextProto = repl.context;
  while ((contextProto = ObjectGetPrototypeOf(contextProto)) !== null) {
    ArrayPrototypePush(completionGroups, filteredOwnPropertyNames(contextProto));
  }

  const contextOwnNames = filteredOwnPropertyNames(repl.context);
  if (!repl.useGlobal) {
    // When the context is not `global`, builtins are not own properties of it.
    // `getGlobalBuiltins()` is a `SafeSet`, not an Array-like.
    ArrayPrototypePush(contextOwnNames, ...getGlobalBuiltins());
  }
  ArrayPrototypePush(completionGroups, contextOwnNames);

  if (filter !== '') {
    addCommonWords(completionGroups);
  }

  return completionGroups;
}

/**
 * Lists member-completion groups for `receiver` using the inspector.
 * @returns {Promise<string[][]>} The completion groups
 */
async function completeMemberExpression(inspector, receiver, chaining, contextId) {
  const evaluate = (expression, throwOnSideEffect) => {
    const params = {
      __proto__: null,
      expression,
      throwOnSideEffect,
      objectGroup: kCompletionObjectGroup,
      silent: true,
    };
    if (contextId !== undefined) {
      params.contextId = contextId;
    }
    return inspector.post('Runtime.evaluate', params);
  };

  try {
    let response;
    try {
      response = await evaluate(receiver, true);
    } catch {
      return [];
    }
    // A side effect (getter/call/proxy trap) or any error (e.g. a ReferenceError
    // for an undeclared receiver) means there is nothing safe to complete.
    if (response.exceptionDetails) {
      return [];
    }

    let { objectId } = response.result;
    if (objectId === undefined) {
      const { type, subtype } = response.result;
      if (type === 'undefined' || subtype === 'null') {
        // `null`/`undefined` have no properties to offer.
        return [];
      }
      // A primitive: box it so its wrapper and prototype properties (e.g. a
      // string's `length` and the `String.prototype` methods) are reachable.
      // The receiver was just proven side-effect-free, so re-evaluating it as
      // the argument to `Object(...)` is safe.
      let boxed;
      try {
        boxed = await evaluate(`Object(${receiver})`, false);
      } catch {
        return [];
      }
      if (boxed.exceptionDetails) {
        return [];
      }
      objectId = boxed.result.objectId;
    }

    let properties;
    try {
      properties = await inspector.post('Runtime.getProperties', {
        __proto__: null,
        objectId,
        // `false` walks the whole prototype chain in one call; each descriptor's
        // `isOwn` flag still lets us keep own properties in their own group.
        ownProperties: false,
        // Indexed elements are never useful for property-name completion and
        // asking the inspector to materialize them makes large arrays and typed
        // arrays needlessly expensive.
        nonIndexedPropertiesOnly: true,
        generatePreview: false,
      });
    } catch {
      // e.g. a revoked proxy. Nothing safe to complete.
      return [];
    }

    // Own properties form the first ("closest") group; inherited ones follow.
    const prefix = `${receiver}${chaining}`;
    const own = [];
    const inherited = [];
    ArrayPrototypeForEach(properties.result || [], (descriptor) => {
      if (isNotLegacyObjectPrototypeMethod(descriptor.name)) {
        ArrayPrototypePush(
          descriptor.isOwn ? own : inherited, `${prefix}${descriptor.name}`);
      }
    });

    const groups = [];
    if (own.length) {
      ArrayPrototypePush(groups, own);
    }
    if (inherited.length) {
      ArrayPrototypePush(groups, inherited);
    }
    return groups;
  } finally {
    try {
      await inspector.post('Runtime.releaseObjectGroup', {
        __proto__: null,
        objectGroup: kCompletionObjectGroup,
      });
    } catch {
      // Best-effort cleanup; ignore failures.
    }
  }
}

// Determines what kind of completion `line` is requesting using a backward scan
function analyzeCompletion(line) {
  if (line.length === 0) {
    return { __proto__: null, receiver: null, chaining: '', completeOn: '' };
  }

  if (endsInsideStringLiteral(line)) {
    return null;
  }

  const start = scanReceiverStart(line);
  if (start < 0) {
    // The completion point is inside an unterminated string literal.
    return null;
  }
  const target = StringPrototypeSlice(line, start);
  if (target === '') {
    return null;
  }

  let rest = target;
  if (!StringPrototypeEndsWith(target, '.')) {
    // Strip the trailing partial identifier (e.g. the `lo` in `console.lo`).
    let k = target.length;
    while (k > 0 &&
           isScannerIdentifierPart(StringPrototypeCharCodeAt(target, k - 1))) {
      k--;
    }
    rest = StringPrototypeSlice(target, 0, k);
  }

  if (StringPrototypeEndsWith(rest, '?.')) {
    const receiver = StringPrototypeSlice(rest, 0, -2);
    return receiver === '' ?
      null :
      { __proto__: null, receiver, chaining: '?.', completeOn: target };
  }
  if (StringPrototypeEndsWith(rest, '.')) {
    const receiver = StringPrototypeSlice(rest, 0, -1);
    return receiver === '' ?
      null :
      { __proto__: null, receiver, chaining: '.', completeOn: target };
  }
  if (rest !== '') {
    // A receiver with no trailing member operator (e.g. ending in `]`). There is
    // nothing to scope a member completion to, so bail conservatively.
    return null;
  }

  // A bare identifier. Skip it when it is the name being introduced by a
  // declaration (e.g. `let a`), since there is nothing to complete there.
  if (RegExpPrototypeExec(declarationKeywordRE,
                          StringPrototypeSlice(line, 0, start)) !== null) {
    return null;
  }
  return { __proto__: null, receiver: null, chaining: '', completeOn: target };
}

// Determines whether the completion point sits inside the text
// of an unterminated string or template literal.
function endsInsideStringLiteral(code) {
  const stack = [];
  for (let i = 0; i < code.length; i++) {
    const top = stack.length ? stack[stack.length - 1] : '';
    const ch = code[i];
    if (top === "'" || top === '"') {
      // Inside a single/double-quoted string.
      if (ch === '\\') {
        i++; // Skip the escaped character.
      } else if (ch === top) {
        ArrayPrototypePop(stack);
      }
    } else if (top === '`') {
      // Inside the text portion of a template literal.
      if (ch === '\\') {
        i++;
      } else if (ch === '`') {
        ArrayPrototypePop(stack);
      } else if (ch === '$' && code[i + 1] === '{') {
        ArrayPrototypePush(stack, '{'); // Enter a `${ ... }` substitution.
        i++;
      }
    } else if (ch === "'" || ch === '"' || ch === '`') {
      // Code context: a quote opens a new string or template literal.
      ArrayPrototypePush(stack, ch);
    } else if (ch === '{') {
      ArrayPrototypePush(stack, '{');
    } else if (ch === '}' && top === '{') {
      ArrayPrototypePop(stack);
    }
  }
  const top = stack.length ? stack[stack.length - 1] : '';
  return top === "'" || top === '"' || top === '`';
}

// Scans backward from the end of `code` over a trailing member-access
// expression, returning the index where it starts.
function scanReceiverStart(code) {
  let i = code.length;
  while (i > 0) {
    const ch = code[i - 1];
    if (isScannerIdentifierPart(StringPrototypeCharCodeAt(code, i - 1))) {
      i--;
    } else if (ch === '.') {
      i--;
    } else if (ch === '?' && code[i] === '.') {
      // The optional-chaining operator `?.`.
      i--;
    } else if (ch === ')' || ch === ']') {
      const open = matchBracketStart(code, i - 1);
      if (open < 0) {
        break;
      }
      i = open;
    } else if (ch === '"' || ch === "'" || ch === '`') {
      const open = matchStringStart(code, i - 1, ch);
      if (open < 0) {
        // No matching opening quote before this one means the quote opens an
        // unterminated string literal that the completion point sits inside
        // (e.g. `"n`). There is nothing to complete there.
        return -1;
      }
      i = open;
    } else {
      break;
    }
  }
  return i;
}

// Given the index of a closing `)` or `]`, returns the index of the matching
// opener
function matchBracketStart(code, closeIndex) {
  let depth = 0;
  for (let i = closeIndex; i >= 0; i--) {
    const ch = code[i];
    if (ch === '"' || ch === "'" || ch === '`') {
      i = matchStringStart(code, i, ch);
      if (i < 0) {
        return -1;
      }
      continue;
    }
    if (ch === ')' || ch === ']' || ch === '}') {
      depth++;
    } else if (ch === '(' || ch === '[' || ch === '{') {
      if (--depth === 0) {
        return i;
      }
    }
  }
  return -1;
}

// Given the index of a closing quote, returns the index of the matching opening
// quote
function matchStringStart(code, closeIndex, quote) {
  for (let i = closeIndex - 1; i >= 0; i--) {
    if (code[i] !== quote) {
      continue;
    }
    let backslashes = 0;
    for (let k = i - 1; k >= 0 && code[k] === '\\'; k--) {
      backslashes++;
    }
    if (backslashes % 2 === 0) {
      return i;
    }
  }
  return -1;
}

function isScannerIdentifierPart(code) {
  return (code >= 48 && code <= 57) ||  // 0-9
         (code >= 65 && code <= 90) ||  // A-Z
         (code >= 97 && code <= 122) || // a-z
         code === 36 ||                 // $
         code === 95 ||                 // _
         code > 127;                    // non-ASCII
}

function isIdentifier(str) {
  if (str === '') {
    return false;
  }
  const { isIdentifierStart, isIdentifierChar } =
    require('internal/deps/acorn/acorn/dist/acorn');
  const first = StringPrototypeCodePointAt(str, 0);
  if (!isIdentifierStart(first)) {
    return false;
  }
  const firstLen = first > 0xffff ? 2 : 1;
  for (let i = firstLen; i < str.length; i += 1) {
    const cp = StringPrototypeCodePointAt(str, i);
    if (!isIdentifierChar(cp)) {
      return false;
    }
    if (cp > 0xffff) {
      i += 1;
    }
  }
  return true;
}

function isNotLegacyObjectPrototypeMethod(str) {
  return isIdentifier(str) &&
    str !== '__defineGetter__' &&
    str !== '__defineSetter__' &&
    str !== '__lookupGetter__' &&
    str !== '__lookupSetter__';
}

function filteredOwnPropertyNames(obj) {
  if (!obj) return [];
  // `Object.prototype` is the only non-contrived object that fulfills
  // `Object.getPrototypeOf(X) === null &&
  //  Object.getPrototypeOf(Object.getPrototypeOf(X.constructor)) === X`.
  let isObjectPrototype = false;
  if (ObjectGetPrototypeOf(obj) === null) {
    const ctorDescriptor = ObjectGetOwnPropertyDescriptor(obj, 'constructor');
    if (ctorDescriptor?.value) {
      const ctorProto = ObjectGetPrototypeOf(ctorDescriptor.value);
      isObjectPrototype = ctorProto && ObjectGetPrototypeOf(ctorProto) === obj;
    }
  }
  const filter = ALL_PROPERTIES | SKIP_SYMBOLS;
  return ArrayPrototypeFilter(
    getOwnNonIndexProperties(obj, filter),
    isObjectPrototype ? isNotLegacyObjectPrototypeMethod : isIdentifier);
}

function addCommonWords(completionGroups) {
  // Only words which do not yet exist as global property should be added to
  // this list.
  ArrayPrototypePush(completionGroups, [
    'async', 'await', 'break', 'case', 'catch', 'const', 'continue',
    'debugger', 'default', 'delete', 'do', 'else', 'export', 'false',
    'finally', 'for', 'function', 'if', 'import', 'in', 'instanceof', 'let',
    'new', 'null', 'return', 'switch', 'this', 'throw', 'true', 'try',
    'typeof', 'var', 'void', 'while', 'with', 'yield',
  ]);
}

function gracefulReaddir(...args) {
  try {
    return ReflectApply(fs.readdirSync, null, args);
  } catch {
    // Continue regardless of error.
  }
}

module.exports = {
  complete,
};
