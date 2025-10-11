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
  kContextId,
  getREPLResourceName,
  globalBuiltins,
  getReplBuiltinLibs,
  fixReplRequire,
} = require('internal/repl/utils');

const { sendInspectorCommand } = require('internal/util/inspector');

const {
  isProxy,
} = require('internal/util/types');

const CJSModule = require('internal/modules/cjs/loader').Module;

const {
  extensionFormatMap,
} = require('internal/modules/esm/formats');

const path = require('path');
const fs = require('fs');

const {
  constants: {
    ALL_PROPERTIES,
    SKIP_SYMBOLS,
  },
  getOwnNonIndexProperties,
} = internalBinding('util');

const {
  isIdentifierStart,
  isIdentifierChar,
  parse: acornParse,
} = require('internal/deps/acorn/acorn/dist/acorn');
const acornWalk = require('internal/deps/acorn/acorn-walk/dist/walk');

const importRE = /\bimport\s*\(\s*['"`](([\w@./:-]+\/)?(?:[\w@./:-]*))(?![^'"`])$/;
const requireRE = /\brequire\s*\(\s*['"`](([\w@./:-]+\/)?(?:[\w@./:-]*))(?![^'"`])$/;
const fsAutoCompleteRE = /fs(?:\.promises)?\.\s*[a-z][a-zA-Z]+\(\s*["'](.*)/;
const versionedFileNamesRe = /-\d+\.\d+/;

fixReplRequire(module);

const { BuiltinModule } = require('internal/bootstrap/realm');

const nodeSchemeBuiltinLibs = ArrayPrototypeMap(getReplBuiltinLibs(), (lib) => `node:${lib}`);
ArrayPrototypeForEach(
  BuiltinModule.getSchemeOnlyModuleNames(),
  (lib) => ArrayPrototypePush(nodeSchemeBuiltinLibs, `node:${lib}`),
);

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

// Provide a list of completions for the given leading text. This is
// given to the readline interface for handling tab completion.
//
// Example:
//  complete('let foo = util.')
//    -> [['util.print', 'util.debug', 'util.log', 'util.inspect'],
//        'util.' ]
//
// Warning: This evals code like "foo.bar.baz", so it could run property
// getter code. To avoid potential triggering side-effects with getters the completion
// logic is skipped when getters or proxies are involved in the expression.
// (see: https://github.com/nodejs/node/issues/57829).
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
      const extensions = ObjectKeys(CJSModule._extensions);
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
        ArrayPrototypePushApply(paths, module.paths);
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

    ArrayPrototypePush(completionGroups, getReplBuiltinLibs(), nodeSchemeBuiltinLibs);
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
        paths = ArrayPrototypeSlice(module.paths);
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

    ArrayPrototypePush(completionGroups, getReplBuiltinLibs(), nodeSchemeBuiltinLibs);
  } else if ((match = RegExpPrototypeExec(fsAutoCompleteRE, line)) !== null &&
             this.allowBlockingCompletions) {
    ({ 0: completionGroups, 1: completeOn } = completeFSFunctions(match));
  } else if (line.length === 0 ||
             RegExpPrototypeExec(/\w|\.|\$/, line[line.length - 1]) !== null) {
    const completeTarget = line.length === 0 ? line : findExpressionCompleteTarget(line);

    if (line.length !== 0 && !completeTarget) {
      completionGroupsLoaded();
      return;
    }
    let expr = '';
    completeOn = completeTarget;
    if (StringPrototypeEndsWith(line, '.')) {
      expr = StringPrototypeSlice(completeTarget, 0, -1);
    } else if (line.length !== 0) {
      const bits = StringPrototypeSplit(completeTarget, '.');
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

    // If the target ends with a dot (e.g. `obj.foo.`) such code won't be valid for AST parsing
    // so in order to make it correct we add an identifier to its end (e.g. `obj.foo.x`)
    const parsableCompleteTarget = completeTarget.endsWith('.') ? `${completeTarget}x` : completeTarget;

    let completeTargetAst;
    try {
      completeTargetAst = acornParse(
        parsableCompleteTarget, { __proto__: null, sourceType: 'module', ecmaVersion: 'latest' },
      );
    } catch { /* No need to specifically handle parse errors */ }

    if (!completeTargetAst) {
      return completionGroupsLoaded();
    }

    return includesProxiesOrGetters(
      completeTargetAst.body[0].expression,
      parsableCompleteTarget,
      this.eval,
      this.context,
      (includes) => {
        if (includes) {
        // The expression involves proxies or getters, meaning that it
        // can trigger side-effectful behaviors, so bail out
          return completionGroupsLoaded();
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
            filter &&= `${expr}${filter}`;
          }

          completionGroupsLoaded();
        });
      });
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

/**
 * This function tries to extract a target for tab completion from code representing an expression.
 *
 * Such target is basically the last piece of the expression that can be evaluated for the potential
 * tab completion.
 *
 * Some examples:
 * - The complete target for `const a = obj.b` is `obj.b`
 *   (because tab completion will evaluate and check the `obj.b` object)
 * - The complete target for `tru` is `tru`
 *   (since we'd ideally want to complete that to `true`)
 * - The complete target for `{ a: tru` is `tru`
 *   (like the last example, we'd ideally want that to complete to true)
 * - There is no complete target for `{ a: true }`
 *   (there is nothing to complete)
 * @param {string} code the code representing the expression to analyze
 * @returns {string|null} a substring of the code representing the complete target is there was one, `null` otherwise
 */
function findExpressionCompleteTarget(code) {
  if (!code) {
    return null;
  }

  if (code.at(-1) === '.') {
    if (code.at(-2) === '?') {
      // The code ends with the optional chaining operator (`?.`),
      // such code can't generate a valid AST so we need to strip
      // the suffix, run this function's logic and add back the
      // optional chaining operator to the result if present
      const result = findExpressionCompleteTarget(code.slice(0, -2));
      return !result ? result : `${result}?.`;
    }

    // The code ends with a dot, such code can't generate a valid AST
    // so we need to strip the suffix, run this function's logic and
    // add back the dot to the result if present
    const result = findExpressionCompleteTarget(code.slice(0, -1));
    return !result ? result : `${result}.`;
  }

  let ast;
  try {
    ast = acornParse(code, { __proto__: null, sourceType: 'module', ecmaVersion: 'latest' });
  } catch {
    const keywords = code.split(' ');

    if (keywords.length > 1) {
      // Something went wrong with the parsing, however this can be due to incomplete code
      // (that is for example missing a closing bracket, as for example `{ a: obj.te`), in
      // this case we take the last code keyword and try again
      // TODO(dario-piotrowicz): make this more robust, right now we only split by spaces
      //                         but that's not always enough, for example it doesn't handle
      //                         this code: `{ a: obj['hello world'].te`
      return findExpressionCompleteTarget(keywords.at(-1));
    }

    // The ast parsing has legitimately failed so we return null
    return null;
  }

  const lastBodyStatement = ast.body[ast.body.length - 1];

  if (!lastBodyStatement) {
    return null;
  }

  // If the last statement is a block we know there is not going to be a potential
  // completion target (e.g. in `{ a: true }` there is no completion to be done)
  if (lastBodyStatement.type === 'BlockStatement') {
    return null;
  }

  // If the last statement is an expression and it has a right side, that's what we
  // want to potentially complete on, so let's re-run the function's logic on that
  if (lastBodyStatement.type === 'ExpressionStatement' && lastBodyStatement.expression.right) {
    const exprRight = lastBodyStatement.expression.right;
    const exprRightCode = code.slice(exprRight.start, exprRight.end);
    return findExpressionCompleteTarget(exprRightCode);
  }

  // If the last statement is a variable declaration statement the last declaration is
  // what we can potentially complete on, so let's re-run the function's logic on that
  if (lastBodyStatement.type === 'VariableDeclaration') {
    const lastDeclarationInit = lastBodyStatement.declarations.at(-1).init;
    if (!lastDeclarationInit) {
      // If there is no initialization we can simply return
      return null;
    }
    const lastDeclarationInitCode = code.slice(lastDeclarationInit.start, lastDeclarationInit.end);
    return findExpressionCompleteTarget(lastDeclarationInitCode);
  }

  // If the last statement is an expression statement with a unary operator (delete, typeof, etc.)
  // we want to extract the argument for completion (e.g. for `delete obj.prop` we want `obj.prop`)
  if (lastBodyStatement.type === 'ExpressionStatement' &&
      lastBodyStatement.expression.type === 'UnaryExpression' &&
      lastBodyStatement.expression.argument) {
    const argument = lastBodyStatement.expression.argument;
    const argumentCode = code.slice(argument.start, argument.end);
    return findExpressionCompleteTarget(argumentCode);
  }

  // Walk the AST for the current block of code, and check whether it contains any
  // statement or expression type that would potentially have side effects if evaluated.
  let isAllowed = true;
  const disallow = () => isAllowed = false;
  acornWalk.simple(lastBodyStatement, {
    ForInStatement: disallow,
    ForOfStatement: disallow,
    CallExpression: disallow,
    AssignmentExpression: disallow,
    UpdateExpression: disallow,
  });
  if (!isAllowed) {
    return null;
  }

  // If any of the above early returns haven't activated then it means that
  // the potential complete target is the full code (e.g. the code represents
  // a simple partial identifier, a member expression, etc...)
  return code.slice(lastBodyStatement.start, lastBodyStatement.end);
}

/**
 * Utility used to determine if an expression includes object getters or proxies.
 *
 * Example: given `obj.foo`, the function lets you know if `foo` has a getter function
 * associated to it, or if `obj` is a proxy
 * @param {any} expr The expression, in AST format to analyze
 * @param {string} exprStr The string representation of the expression
 * @param {(str: string, ctx: any, resourceName: string, cb: (error, evaled) => void) => void} evalFn
 *   Eval function to use
 * @param {any} ctx The context to use for any code evaluation
 * @param {(includes: boolean) => void} callback Callback that will be called with the result of the operation
 * @returns {void}
 */
function includesProxiesOrGetters(expr, exprStr, evalFn, ctx, callback) {
  if (expr?.type !== 'MemberExpression') {
    // If the expression is not a member one for obvious reasons no getters are involved
    return callback(false);
  }

  if (expr.object.type === 'MemberExpression') {
    // The object itself is a member expression, so we need to recurse (e.g. the expression is `obj.foo.bar`)
    return includesProxiesOrGetters(
      expr.object,
      exprStr.slice(0, expr.object.end),
      evalFn,
      ctx,
      (includes, lastEvaledObj) => {
        if (includes) {
          // If the recurred call found a getter we can also terminate
          return callback(includes);
        }

        if (isProxy(lastEvaledObj)) {
          return callback(true);
        }

        // If a getter/proxy hasn't been found by the recursion call we need to check if maybe a getter/proxy
        // is present here (e.g. in `obj.foo.bar` we found that `obj.foo` doesn't involve any getters so we now
        // need to check if `bar` on `obj.foo` (i.e. `lastEvaledObj`) has a getter or if `obj.foo.bar` is a proxy)
        return hasGetterOrIsProxy(lastEvaledObj, expr.property, (doesHaveGetterOrIsProxy) => {
          return callback(doesHaveGetterOrIsProxy);
        });
      },
    );
  }

  // This is the base of the recursion we have an identifier for the object and an identifier or literal
  // for the property (e.g. we have `obj.foo` or `obj['foo']`, `obj` is the object identifier and `foo`
  // is the property identifier/literal)
  if (expr.object.type === 'Identifier') {
    return evalFn(`try { ${expr.object.name} } catch {}`, ctx, getREPLResourceName(), (err, obj) => {
      if (err) {
        return callback(false);
      }

      if (isProxy(obj)) {
        return callback(true);
      }

      return hasGetterOrIsProxy(obj, expr.property, (doesHaveGetterOrIsProxy) => {
        if (doesHaveGetterOrIsProxy) {
          return callback(true);
        }

        return evalFn(
          `try { ${exprStr} } catch {} `, ctx, getREPLResourceName(), (err, obj) => {
            if (err) {
              return callback(false);
            }
            return callback(false, obj);
          });
      });
    });
  }

  /**
   * Utility to see if a property has a getter associated to it or if
   * the property itself is a proxy object.
   * @returns {void}
   */
  function hasGetterOrIsProxy(obj, astProp, cb) {
    if (!obj || !astProp) {
      return cb(false);
    }

    if (astProp.type === 'Literal') {
      // We have something like `obj['foo'].x` where `x` is the literal

      if (safeIsProxyAccess(obj, astProp.value)) {
        return cb(true);
      }

      const propDescriptor = ObjectGetOwnPropertyDescriptor(
        obj,
        `${astProp.value}`,
      );
      const propHasGetter = typeof propDescriptor?.get === 'function';
      return cb(propHasGetter);
    }

    if (
      astProp.type === 'Identifier' &&
      exprStr.at(astProp.start - 1) === '.'
    ) {
      // We have something like `obj.foo.x` where `foo` is the identifier

      if (safeIsProxyAccess(obj, astProp.name)) {
        return cb(true);
      }

      const propDescriptor = ObjectGetOwnPropertyDescriptor(
        obj,
        `${astProp.name}`,
      );
      const propHasGetter = typeof propDescriptor?.get === 'function';
      return cb(propHasGetter);
    }

    return evalFn(
      // Note: this eval runs the property expression, which might be side-effectful, for example
      //       the user could be running `obj[getKey()].` where `getKey()` has some side effects.
      //       Arguably this behavior should not be too surprising, but if it turns out that it is,
      //       then we can revisit this behavior and add logic to analyze the property expression
      //       and eval it only if we can confidently say that it can't have any side effects
      `try { ${exprStr.slice(astProp.start, astProp.end)} } catch {} `,
      ctx,
      getREPLResourceName(),
      (err, evaledProp) => {
        if (err) {
          return callback(false);
        }

        if (typeof evaledProp === 'string') {
          if (safeIsProxyAccess(obj, evaledProp)) {
            return cb(true);
          }

          const propDescriptor = ObjectGetOwnPropertyDescriptor(
            obj,
            evaledProp,
          );
          const propHasGetter = typeof propDescriptor?.get === 'function';
          return cb(propHasGetter);
        }

        return callback(false);
      },
    );
  }

  function safeIsProxyAccess(obj, prop) {
    // Accessing `prop` may trigger a getter that throws, so we use try-catch to guard against it
    try {
      return isProxy(obj[prop]);
    } catch {
      return false;
    }
  }

  return callback(false);
}

module.exports = {
  complete,
};
