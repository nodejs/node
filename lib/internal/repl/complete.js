'use strict';

const {
  ArrayPrototypeFilter,
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
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
  StringPrototypeCodePointAt,
  StringPrototypeEndsWith,
  StringPrototypeIncludes,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
  StringPrototypeToLocaleLowerCase,
  StringPrototypeTrimStart,
} = primordials;
const {
  constants: {
    ALL_PROPERTIES,
    SKIP_SYMBOLS,
  },
  getOwnNonIndexProperties,
} = internalBinding('util');
const { BuiltinModule } = require('internal/bootstrap/realm');
const { Module: CJSModule } = require('internal/modules/cjs/loader');
const {
  getREPLResourceName,
  kContextId,
  globalBuiltins,
  builtinLibs,
  replFilename,
} = require('internal/repl/utils');
const { extensionFormatMap } = require('internal/modules/esm/formats');
const { isIdentifierStart, isIdentifierChar } = require('internal/deps/acorn/acorn/dist/acorn');
const path = require('path');
const fs = require('fs');
const { sendInspectorCommand } = require('internal/util/inspector');

const requireRE = /\brequire\s*\(\s*['"`](([\w@./:-]+\/)?(?:[\w@./:-]*))(?![^'"`])$/;
const importRE = /\bimport\s*\(\s*['"`](([\w@./:-]+\/)?(?:[\w@./:-]*))(?![^'"`])$/;
const fsAutoCompleteRE = /fs(?:\.promises)?\.\s*[a-z][a-zA-Z]+\(\s*["'](.*)/;
const simpleExpressionRE =
    /(?:[\w$'"`[{(](?:\w|\$|['"`\]})])*\??\.)*[a-zA-Z_$](?:\w|\$)*\??\.?$/;
const versionedFileNamesRe = /-\d+\.\d+/;

const modulePaths = CJSModule._nodeModulePaths(replFilename);
const nodeSchemeBuiltinLibs = ArrayPrototypeMap(builtinLibs, (lib) => `node:${lib}`);
ArrayPrototypeForEach(
  BuiltinModule.getSchemeOnlyModuleNames(),
  (lib) => ArrayPrototypePush(nodeSchemeBuiltinLibs, `node:${lib}`),
);

function completeFSFunctions(match) {
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
    (d) => d.name,
  );

  return [[completions], baseName];
}

function gracefulReaddir(...args) {
  try {
    return ReflectApply(fs.readdirSync, null, args);
  } catch {
    // Continue regardless of error.
  }
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

function getGlobalLexicalScopeNames(contextId) {
  return sendInspectorCommand((session) => {
    let names = [];
    session.post('Runtime.globalLexicalScopeNames', {
      executionContextId: contextId,
    }, (error, result) => {
      if (!error) names = result.names;
    });
    return names;
  }, () => []);
}

function isIdentifier(str) {
  if (str === '') {
    return false;
  }
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
    if (ctorDescriptor && ctorDescriptor.value) {
      const ctorProto = ObjectGetPrototypeOf(ctorDescriptor.value);
      isObjectPrototype = ctorProto && ObjectGetPrototypeOf(ctorProto) === obj;
    }
  }
  const filter = ALL_PROPERTIES | SKIP_SYMBOLS;
  return ArrayPrototypeFilter(
    getOwnNonIndexProperties(obj, filter),
    isObjectPrototype ? isNotLegacyObjectPrototypeMethod : isIdentifier);
}

// Provide a list of completions for the given leading text. This is
// given to the readline interface for handling tab completion.
//
// Example:
//  complete('let foo = util.')
//    -> [['util.print', 'util.debug', 'util.log', 'util.inspect'],
//        'util.' ]
//
// Warning: This eval's code like "foo.bar.baz", so it will run property
// getter code.
function complete(line, callback) {
  // List of completion lists, one for each inheritance "level"
  let completionGroups = [];
  let completeOn, group;

  // Ignore right whitespace. It could change the outcome.
  line = StringPrototypeTrimStart(line);

  let filter = '';

  let match;
  // REPL commands (e.g. ".break").
  if ((match = RegExpPrototypeExec(/^\s*\.(\w*)$/, line)) !== null) {
    ArrayPrototypePush(completionGroups, ObjectKeys(this.commands));
    completeOn = match[1];
    if (completeOn.length) {
      filter = completeOn;
    }
  } else if ((match = RegExpPrototypeExec(requireRE, line)) !== null) {
    // require('...<Tab>')
    completeOn = match[1];
    filter = completeOn;
    if (this.allowBlockingCompletions) {
      const subdir = match[2] || '';
      const extensions = ObjectKeys(this.context.require.extensions);
      const indexes = ArrayPrototypeMap(extensions,
                                        (extension) => `index${extension}`);
      ArrayPrototypePush(indexes, 'package.json', 'index');

      group = [];
      let paths = [];

      if (completeOn === '.') {
        group = ['./', '../'];
      } else if (completeOn === '..') {
        group = ['../'];
      } else if (RegExpPrototypeExec(/^\.\.?\//, completeOn) !== null) {
        paths = [process.cwd()];
      } else {
        paths = [];
        ArrayPrototypePushApply(paths, modulePaths);
        ArrayPrototypePushApply(paths, CJSModule.globalPaths);
      }

      ArrayPrototypeForEach(paths, (dir) => {
        dir = path.resolve(dir, subdir);
        const dirents = gracefulReaddir(dir, { withFileTypes: true }) || [];
        ArrayPrototypeForEach(dirents, (dirent) => {
          if (RegExpPrototypeExec(versionedFileNamesRe, dirent.name) !== null ||
            dirent.name === '.npm') {
            // Exclude versioned names that 'npm' installs.
            return;
          }
          const extension = path.extname(dirent.name);
          const base = StringPrototypeSlice(dirent.name, 0, -extension.length);
          if (!dirent.isDirectory()) {
            if (StringPrototypeIncludes(extensions, extension) &&
              (!subdir || base !== 'index')) {
              ArrayPrototypePush(group, `${subdir}${base}`);
            }
            return;
          }
          ArrayPrototypePush(group, `${subdir}${dirent.name}/`);
          const absolute = path.resolve(dir, dirent.name);
          if (ArrayPrototypeSome(
            gracefulReaddir(absolute) || [],
            (subfile) => ArrayPrototypeIncludes(indexes, subfile),
          )) {
            ArrayPrototypePush(group, `${subdir}${dirent.name}`);
          }
        });
      });
      if (group.length) {
        ArrayPrototypePush(completionGroups, group);
      }
    }

    ArrayPrototypePush(completionGroups, builtinLibs, nodeSchemeBuiltinLibs);
  } else if ((match = RegExpPrototypeExec(importRE, line)) !== null) {
    // import('...<Tab>')
    completeOn = match[1];
    filter = completeOn;
    if (this.allowBlockingCompletions) {
      const subdir = match[2] || '';
      // File extensions that can be imported:
      const extensions = ObjectKeys(extensionFormatMap);

      // Only used when loading bare module specifiers from `node_modules`:
      const indexes = ArrayPrototypeMap(extensions, (ext) => `index${ext}`);
      ArrayPrototypePush(indexes, 'package.json');

      group = [];
      let paths = [];
      if (completeOn === '.') {
        group = ['./', '../'];
      } else if (completeOn === '..') {
        group = ['../'];
      } else if (RegExpPrototypeExec(/^\.\.?\//, completeOn) !== null) {
        paths = [process.cwd()];
      } else {
        paths = ArrayPrototypeSlice(modulePaths);
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
            if (StringPrototypeIncludes(extensions, extension)) {
              ArrayPrototypePush(group, `${subdir}${name}`);
            }
            return;
          }

          ArrayPrototypePush(group, `${subdir}${name}/`);
          if (!subdir && isInNodeModules) {
            const absolute = path.resolve(dir, name);
            const subfiles = gracefulReaddir(absolute) || [];
            if (ArrayPrototypeSome(subfiles, (subfile) => {
              return ArrayPrototypeIncludes(indexes, subfile);
            })) {
              ArrayPrototypePush(group, `${subdir}${name}`);
            }
          }
        });
      });

      if (group.length) {
        ArrayPrototypePush(completionGroups, group);
      }
    }

    ArrayPrototypePush(completionGroups, builtinLibs, nodeSchemeBuiltinLibs);
  } else if ((match = RegExpPrototypeExec(fsAutoCompleteRE, line)) !== null &&
    this.allowBlockingCompletions) {
    ({ 0: completionGroups, 1: completeOn } = completeFSFunctions(match));
    // Handle variable member lookup.
    // We support simple chained expressions like the following (no function
    // calls, etc.). That is for simplicity and also because we *eval* that
    // leading expression so for safety (see WARNING above) don't want to
    // eval function calls.
    //
    //   foo.bar<|>     # completions for 'foo' with filter 'bar'
    //   spam.eggs.<|>  # completions for 'spam.eggs' with filter ''
    //   foo<|>         # all scope vars with filter 'foo'
    //   foo.<|>        # completions for 'foo' with filter ''
  } else if (line.length === 0 ||
    RegExpPrototypeExec(/\w|\.|\$/, line[line.length - 1]) !== null) {
    const { 0: match } = RegExpPrototypeExec(simpleExpressionRE, line) || [''];
    if (line.length !== 0 && !match) {
      completionGroupsLoaded();
      return;
    }
    let expr = '';
    completeOn = match;
    if (StringPrototypeEndsWith(line, '.')) {
      expr = StringPrototypeSlice(match, 0, -1);
    } else if (line.length !== 0) {
      const bits = StringPrototypeSplit(match, '.');
      filter = ArrayPrototypePop(bits);
      expr = ArrayPrototypeJoin(bits, '.');
    }

    // Resolve expr and get its completions.
    if (!expr) {
      // Get global vars synchronously
      ArrayPrototypePush(completionGroups,
                         getGlobalLexicalScopeNames(this[kContextId]));
      let contextProto = this.context;
      while ((contextProto = ObjectGetPrototypeOf(contextProto)) !== null) {
        ArrayPrototypePush(completionGroups,
                           filteredOwnPropertyNames(contextProto));
      }
      const contextOwnNames = filteredOwnPropertyNames(this.context);
      if (!this.useGlobal) {
        // When the context is not `global`, builtins are not own
        // properties of it.
        // `globalBuiltins` is a `SafeSet`, not an Array-like.
        ArrayPrototypePush(contextOwnNames, ...globalBuiltins);
      }
      ArrayPrototypePush(completionGroups, contextOwnNames);
      if (filter !== '') addCommonWords(completionGroups);
      completionGroupsLoaded();
      return;
    }

    let chaining = '.';
    if (StringPrototypeEndsWith(expr, '?')) {
      expr = StringPrototypeSlice(expr, 0, -1);
      chaining = '?.';
    }

    const memberGroups = [];
    const evalExpr = `try { ${expr} } catch {}`;
    this.eval(evalExpr, this.context, getREPLResourceName(), (e, obj) => {
      try {
        let p;
        if ((typeof obj === 'object' && obj !== null) ||
          typeof obj === 'function') {
          ArrayPrototypePush(memberGroups, filteredOwnPropertyNames(obj));
          p = ObjectGetPrototypeOf(obj);
        } else {
          p = obj.constructor ? obj.constructor.prototype : null;
        }
        // Circular refs possible? Let's guard against that.
        let sentinel = 5;
        while (p !== null && sentinel-- !== 0) {
          ArrayPrototypePush(memberGroups, filteredOwnPropertyNames(p));
          p = ObjectGetPrototypeOf(p);
        }
      } catch {
        // Maybe a Proxy object without `getOwnPropertyNames` trap.
        // We simply ignore it here, as we don't want to break the
        // autocompletion. Fixes the bug
        // https://github.com/nodejs/node/issues/2119
      }

      if (memberGroups.length) {
        expr += chaining;
        ArrayPrototypeForEach(memberGroups, (group) => {
          ArrayPrototypePush(completionGroups,
                             ArrayPrototypeMap(group,
                                               (member) => `${expr}${member}`));
        });
        if (filter) {
          filter = `${expr}${filter}`;
        }
      }

      completionGroupsLoaded();
    });
    return;
  }

  return completionGroupsLoaded();

  // Will be called when all completionGroups are in place
  // Useful for async autocompletion
  function completionGroupsLoaded() {
    // Filter, sort (within each group), uniq and merge the completion groups.
    if (completionGroups.length && filter) {
      const newCompletionGroups = [];
      const lowerCaseFilter = StringPrototypeToLocaleLowerCase(filter);
      ArrayPrototypeForEach(completionGroups, (group) => {
        const filteredGroup = ArrayPrototypeFilter(group, (str) => {
          // Filter is always case-insensitive following chromium autocomplete
          // behavior.
          return StringPrototypeStartsWith(
            StringPrototypeToLocaleLowerCase(str),
            lowerCaseFilter,
          );
        });
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
    // Completion group 0 is the "closest" (least far up the inheritance
    // chain) so we put its completions last: to be closest in the REPL.
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

    callback(null, [completions, completeOn]);
  }
}

module.exports = {
  complete,
};
