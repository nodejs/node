function _typeof(obj) {
  "@babel/helpers - typeof";

  if (typeof Symbol === "function" && typeof Symbol.iterator === "symbol") {
    _typeof = function (obj) {
      return typeof obj;
    };
  } else {
    _typeof = function (obj) {
      return obj && typeof Symbol === "function" && obj.constructor === Symbol && obj !== Symbol.prototype ? "symbol" : typeof obj;
    };
  }

  return _typeof(obj);
}

function _slicedToArray(arr, i) {
  return _arrayWithHoles(arr) || _iterableToArrayLimit(arr, i) || _unsupportedIterableToArray(arr, i) || _nonIterableRest();
}

function _toConsumableArray(arr) {
  return _arrayWithoutHoles(arr) || _iterableToArray(arr) || _unsupportedIterableToArray(arr) || _nonIterableSpread();
}

function _arrayWithoutHoles(arr) {
  if (Array.isArray(arr)) return _arrayLikeToArray(arr);
}

function _arrayWithHoles(arr) {
  if (Array.isArray(arr)) return arr;
}

function _iterableToArray(iter) {
  if (typeof Symbol !== "undefined" && Symbol.iterator in Object(iter)) return Array.from(iter);
}

function _iterableToArrayLimit(arr, i) {
  if (typeof Symbol === "undefined" || !(Symbol.iterator in Object(arr))) return;
  var _arr = [];
  var _n = true;
  var _d = false;
  var _e = undefined;

  try {
    for (var _i = arr[Symbol.iterator](), _s; !(_n = (_s = _i.next()).done); _n = true) {
      _arr.push(_s.value);

      if (i && _arr.length === i) break;
    }
  } catch (err) {
    _d = true;
    _e = err;
  } finally {
    try {
      if (!_n && _i["return"] != null) _i["return"]();
    } finally {
      if (_d) throw _e;
    }
  }

  return _arr;
}

function _unsupportedIterableToArray(o, minLen) {
  if (!o) return;
  if (typeof o === "string") return _arrayLikeToArray(o, minLen);
  var n = Object.prototype.toString.call(o).slice(8, -1);
  if (n === "Object" && o.constructor) n = o.constructor.name;
  if (n === "Map" || n === "Set") return Array.from(n);
  if (n === "Arguments" || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(n)) return _arrayLikeToArray(o, minLen);
}

function _arrayLikeToArray(arr, len) {
  if (len == null || len > arr.length) len = arr.length;

  for (var i = 0, arr2 = new Array(len); i < len; i++) arr2[i] = arr[i];

  return arr2;
}

function _nonIterableSpread() {
  throw new TypeError("Invalid attempt to spread non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
}

function _nonIterableRest() {
  throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
}

var commonjsGlobal = typeof globalThis !== 'undefined' ? globalThis : typeof window !== 'undefined' ? window : typeof global !== 'undefined' ? global : typeof self !== 'undefined' ? self : {};

function createCommonjsModule(fn, module) {
	return module = { exports: {} }, fn(module, module.exports), module.exports;
}

var estraverse = createCommonjsModule(function (module, exports) {
  /*
    Copyright (C) 2012-2013 Yusuke Suzuki <utatane.tea@gmail.com>
    Copyright (C) 2012 Ariya Hidayat <ariya.hidayat@gmail.com>
  
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
  
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
  
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  */

  /*jslint vars:false, bitwise:true*/

  /*jshint indent:4*/

  /*global exports:true*/
  (function clone(exports) {

    var Syntax, VisitorOption, VisitorKeys, BREAK, SKIP, REMOVE;

    function deepCopy(obj) {
      var ret = {},
          key,
          val;

      for (key in obj) {
        if (obj.hasOwnProperty(key)) {
          val = obj[key];

          if (typeof val === 'object' && val !== null) {
            ret[key] = deepCopy(val);
          } else {
            ret[key] = val;
          }
        }
      }

      return ret;
    } // based on LLVM libc++ upper_bound / lower_bound
    // MIT License


    function upperBound(array, func) {
      var diff, len, i, current;
      len = array.length;
      i = 0;

      while (len) {
        diff = len >>> 1;
        current = i + diff;

        if (func(array[current])) {
          len = diff;
        } else {
          i = current + 1;
          len -= diff + 1;
        }
      }

      return i;
    }

    Syntax = {
      AssignmentExpression: 'AssignmentExpression',
      AssignmentPattern: 'AssignmentPattern',
      ArrayExpression: 'ArrayExpression',
      ArrayPattern: 'ArrayPattern',
      ArrowFunctionExpression: 'ArrowFunctionExpression',
      AwaitExpression: 'AwaitExpression',
      // CAUTION: It's deferred to ES7.
      BlockStatement: 'BlockStatement',
      BinaryExpression: 'BinaryExpression',
      BreakStatement: 'BreakStatement',
      CallExpression: 'CallExpression',
      CatchClause: 'CatchClause',
      ClassBody: 'ClassBody',
      ClassDeclaration: 'ClassDeclaration',
      ClassExpression: 'ClassExpression',
      ComprehensionBlock: 'ComprehensionBlock',
      // CAUTION: It's deferred to ES7.
      ComprehensionExpression: 'ComprehensionExpression',
      // CAUTION: It's deferred to ES7.
      ConditionalExpression: 'ConditionalExpression',
      ContinueStatement: 'ContinueStatement',
      DebuggerStatement: 'DebuggerStatement',
      DirectiveStatement: 'DirectiveStatement',
      DoWhileStatement: 'DoWhileStatement',
      EmptyStatement: 'EmptyStatement',
      ExportAllDeclaration: 'ExportAllDeclaration',
      ExportDefaultDeclaration: 'ExportDefaultDeclaration',
      ExportNamedDeclaration: 'ExportNamedDeclaration',
      ExportSpecifier: 'ExportSpecifier',
      ExpressionStatement: 'ExpressionStatement',
      ForStatement: 'ForStatement',
      ForInStatement: 'ForInStatement',
      ForOfStatement: 'ForOfStatement',
      FunctionDeclaration: 'FunctionDeclaration',
      FunctionExpression: 'FunctionExpression',
      GeneratorExpression: 'GeneratorExpression',
      // CAUTION: It's deferred to ES7.
      Identifier: 'Identifier',
      IfStatement: 'IfStatement',
      ImportExpression: 'ImportExpression',
      ImportDeclaration: 'ImportDeclaration',
      ImportDefaultSpecifier: 'ImportDefaultSpecifier',
      ImportNamespaceSpecifier: 'ImportNamespaceSpecifier',
      ImportSpecifier: 'ImportSpecifier',
      Literal: 'Literal',
      LabeledStatement: 'LabeledStatement',
      LogicalExpression: 'LogicalExpression',
      MemberExpression: 'MemberExpression',
      MetaProperty: 'MetaProperty',
      MethodDefinition: 'MethodDefinition',
      ModuleSpecifier: 'ModuleSpecifier',
      NewExpression: 'NewExpression',
      ObjectExpression: 'ObjectExpression',
      ObjectPattern: 'ObjectPattern',
      Program: 'Program',
      Property: 'Property',
      RestElement: 'RestElement',
      ReturnStatement: 'ReturnStatement',
      SequenceExpression: 'SequenceExpression',
      SpreadElement: 'SpreadElement',
      Super: 'Super',
      SwitchStatement: 'SwitchStatement',
      SwitchCase: 'SwitchCase',
      TaggedTemplateExpression: 'TaggedTemplateExpression',
      TemplateElement: 'TemplateElement',
      TemplateLiteral: 'TemplateLiteral',
      ThisExpression: 'ThisExpression',
      ThrowStatement: 'ThrowStatement',
      TryStatement: 'TryStatement',
      UnaryExpression: 'UnaryExpression',
      UpdateExpression: 'UpdateExpression',
      VariableDeclaration: 'VariableDeclaration',
      VariableDeclarator: 'VariableDeclarator',
      WhileStatement: 'WhileStatement',
      WithStatement: 'WithStatement',
      YieldExpression: 'YieldExpression'
    };
    VisitorKeys = {
      AssignmentExpression: ['left', 'right'],
      AssignmentPattern: ['left', 'right'],
      ArrayExpression: ['elements'],
      ArrayPattern: ['elements'],
      ArrowFunctionExpression: ['params', 'body'],
      AwaitExpression: ['argument'],
      // CAUTION: It's deferred to ES7.
      BlockStatement: ['body'],
      BinaryExpression: ['left', 'right'],
      BreakStatement: ['label'],
      CallExpression: ['callee', 'arguments'],
      CatchClause: ['param', 'body'],
      ClassBody: ['body'],
      ClassDeclaration: ['id', 'superClass', 'body'],
      ClassExpression: ['id', 'superClass', 'body'],
      ComprehensionBlock: ['left', 'right'],
      // CAUTION: It's deferred to ES7.
      ComprehensionExpression: ['blocks', 'filter', 'body'],
      // CAUTION: It's deferred to ES7.
      ConditionalExpression: ['test', 'consequent', 'alternate'],
      ContinueStatement: ['label'],
      DebuggerStatement: [],
      DirectiveStatement: [],
      DoWhileStatement: ['body', 'test'],
      EmptyStatement: [],
      ExportAllDeclaration: ['source'],
      ExportDefaultDeclaration: ['declaration'],
      ExportNamedDeclaration: ['declaration', 'specifiers', 'source'],
      ExportSpecifier: ['exported', 'local'],
      ExpressionStatement: ['expression'],
      ForStatement: ['init', 'test', 'update', 'body'],
      ForInStatement: ['left', 'right', 'body'],
      ForOfStatement: ['left', 'right', 'body'],
      FunctionDeclaration: ['id', 'params', 'body'],
      FunctionExpression: ['id', 'params', 'body'],
      GeneratorExpression: ['blocks', 'filter', 'body'],
      // CAUTION: It's deferred to ES7.
      Identifier: [],
      IfStatement: ['test', 'consequent', 'alternate'],
      ImportExpression: ['source'],
      ImportDeclaration: ['specifiers', 'source'],
      ImportDefaultSpecifier: ['local'],
      ImportNamespaceSpecifier: ['local'],
      ImportSpecifier: ['imported', 'local'],
      Literal: [],
      LabeledStatement: ['label', 'body'],
      LogicalExpression: ['left', 'right'],
      MemberExpression: ['object', 'property'],
      MetaProperty: ['meta', 'property'],
      MethodDefinition: ['key', 'value'],
      ModuleSpecifier: [],
      NewExpression: ['callee', 'arguments'],
      ObjectExpression: ['properties'],
      ObjectPattern: ['properties'],
      Program: ['body'],
      Property: ['key', 'value'],
      RestElement: ['argument'],
      ReturnStatement: ['argument'],
      SequenceExpression: ['expressions'],
      SpreadElement: ['argument'],
      Super: [],
      SwitchStatement: ['discriminant', 'cases'],
      SwitchCase: ['test', 'consequent'],
      TaggedTemplateExpression: ['tag', 'quasi'],
      TemplateElement: [],
      TemplateLiteral: ['quasis', 'expressions'],
      ThisExpression: [],
      ThrowStatement: ['argument'],
      TryStatement: ['block', 'handler', 'finalizer'],
      UnaryExpression: ['argument'],
      UpdateExpression: ['argument'],
      VariableDeclaration: ['declarations'],
      VariableDeclarator: ['id', 'init'],
      WhileStatement: ['test', 'body'],
      WithStatement: ['object', 'body'],
      YieldExpression: ['argument']
    }; // unique id

    BREAK = {};
    SKIP = {};
    REMOVE = {};
    VisitorOption = {
      Break: BREAK,
      Skip: SKIP,
      Remove: REMOVE
    };

    function Reference(parent, key) {
      this.parent = parent;
      this.key = key;
    }

    Reference.prototype.replace = function replace(node) {
      this.parent[this.key] = node;
    };

    Reference.prototype.remove = function remove() {
      if (Array.isArray(this.parent)) {
        this.parent.splice(this.key, 1);
        return true;
      } else {
        this.replace(null);
        return false;
      }
    };

    function Element(node, path, wrap, ref) {
      this.node = node;
      this.path = path;
      this.wrap = wrap;
      this.ref = ref;
    }

    function Controller() {} // API:
    // return property path array from root to current node


    Controller.prototype.path = function path() {
      var i, iz, j, jz, result, element;

      function addToPath(result, path) {
        if (Array.isArray(path)) {
          for (j = 0, jz = path.length; j < jz; ++j) {
            result.push(path[j]);
          }
        } else {
          result.push(path);
        }
      } // root node


      if (!this.__current.path) {
        return null;
      } // first node is sentinel, second node is root element


      result = [];

      for (i = 2, iz = this.__leavelist.length; i < iz; ++i) {
        element = this.__leavelist[i];
        addToPath(result, element.path);
      }

      addToPath(result, this.__current.path);
      return result;
    }; // API:
    // return type of current node


    Controller.prototype.type = function () {
      var node = this.current();
      return node.type || this.__current.wrap;
    }; // API:
    // return array of parent elements


    Controller.prototype.parents = function parents() {
      var i, iz, result; // first node is sentinel

      result = [];

      for (i = 1, iz = this.__leavelist.length; i < iz; ++i) {
        result.push(this.__leavelist[i].node);
      }

      return result;
    }; // API:
    // return current node


    Controller.prototype.current = function current() {
      return this.__current.node;
    };

    Controller.prototype.__execute = function __execute(callback, element) {
      var previous, result;
      result = undefined;
      previous = this.__current;
      this.__current = element;
      this.__state = null;

      if (callback) {
        result = callback.call(this, element.node, this.__leavelist[this.__leavelist.length - 1].node);
      }

      this.__current = previous;
      return result;
    }; // API:
    // notify control skip / break


    Controller.prototype.notify = function notify(flag) {
      this.__state = flag;
    }; // API:
    // skip child nodes of current node


    Controller.prototype.skip = function () {
      this.notify(SKIP);
    }; // API:
    // break traversals


    Controller.prototype['break'] = function () {
      this.notify(BREAK);
    }; // API:
    // remove node


    Controller.prototype.remove = function () {
      this.notify(REMOVE);
    };

    Controller.prototype.__initialize = function (root, visitor) {
      this.visitor = visitor;
      this.root = root;
      this.__worklist = [];
      this.__leavelist = [];
      this.__current = null;
      this.__state = null;
      this.__fallback = null;

      if (visitor.fallback === 'iteration') {
        this.__fallback = Object.keys;
      } else if (typeof visitor.fallback === 'function') {
        this.__fallback = visitor.fallback;
      }

      this.__keys = VisitorKeys;

      if (visitor.keys) {
        this.__keys = Object.assign(Object.create(this.__keys), visitor.keys);
      }
    };

    function isNode(node) {
      if (node == null) {
        return false;
      }

      return typeof node === 'object' && typeof node.type === 'string';
    }

    function isProperty(nodeType, key) {
      return (nodeType === Syntax.ObjectExpression || nodeType === Syntax.ObjectPattern) && 'properties' === key;
    }

    function candidateExistsInLeaveList(leavelist, candidate) {
      for (var i = leavelist.length - 1; i >= 0; --i) {
        if (leavelist[i].node === candidate) {
          return true;
        }
      }

      return false;
    }

    Controller.prototype.traverse = function traverse(root, visitor) {
      var worklist, leavelist, element, node, nodeType, ret, key, current, current2, candidates, candidate, sentinel;

      this.__initialize(root, visitor);

      sentinel = {}; // reference

      worklist = this.__worklist;
      leavelist = this.__leavelist; // initialize

      worklist.push(new Element(root, null, null, null));
      leavelist.push(new Element(null, null, null, null));

      while (worklist.length) {
        element = worklist.pop();

        if (element === sentinel) {
          element = leavelist.pop();
          ret = this.__execute(visitor.leave, element);

          if (this.__state === BREAK || ret === BREAK) {
            return;
          }

          continue;
        }

        if (element.node) {
          ret = this.__execute(visitor.enter, element);

          if (this.__state === BREAK || ret === BREAK) {
            return;
          }

          worklist.push(sentinel);
          leavelist.push(element);

          if (this.__state === SKIP || ret === SKIP) {
            continue;
          }

          node = element.node;
          nodeType = node.type || element.wrap;
          candidates = this.__keys[nodeType];

          if (!candidates) {
            if (this.__fallback) {
              candidates = this.__fallback(node);
            } else {
              throw new Error('Unknown node type ' + nodeType + '.');
            }
          }

          current = candidates.length;

          while ((current -= 1) >= 0) {
            key = candidates[current];
            candidate = node[key];

            if (!candidate) {
              continue;
            }

            if (Array.isArray(candidate)) {
              current2 = candidate.length;

              while ((current2 -= 1) >= 0) {
                if (!candidate[current2]) {
                  continue;
                }

                if (candidateExistsInLeaveList(leavelist, candidate[current2])) {
                  continue;
                }

                if (isProperty(nodeType, candidates[current])) {
                  element = new Element(candidate[current2], [key, current2], 'Property', null);
                } else if (isNode(candidate[current2])) {
                  element = new Element(candidate[current2], [key, current2], null, null);
                } else {
                  continue;
                }

                worklist.push(element);
              }
            } else if (isNode(candidate)) {
              if (candidateExistsInLeaveList(leavelist, candidate)) {
                continue;
              }

              worklist.push(new Element(candidate, key, null, null));
            }
          }
        }
      }
    };

    Controller.prototype.replace = function replace(root, visitor) {
      var worklist, leavelist, node, nodeType, target, element, current, current2, candidates, candidate, sentinel, outer, key;

      function removeElem(element) {
        var i, key, nextElem, parent;

        if (element.ref.remove()) {
          // When the reference is an element of an array.
          key = element.ref.key;
          parent = element.ref.parent; // If removed from array, then decrease following items' keys.

          i = worklist.length;

          while (i--) {
            nextElem = worklist[i];

            if (nextElem.ref && nextElem.ref.parent === parent) {
              if (nextElem.ref.key < key) {
                break;
              }

              --nextElem.ref.key;
            }
          }
        }
      }

      this.__initialize(root, visitor);

      sentinel = {}; // reference

      worklist = this.__worklist;
      leavelist = this.__leavelist; // initialize

      outer = {
        root: root
      };
      element = new Element(root, null, null, new Reference(outer, 'root'));
      worklist.push(element);
      leavelist.push(element);

      while (worklist.length) {
        element = worklist.pop();

        if (element === sentinel) {
          element = leavelist.pop();
          target = this.__execute(visitor.leave, element); // node may be replaced with null,
          // so distinguish between undefined and null in this place

          if (target !== undefined && target !== BREAK && target !== SKIP && target !== REMOVE) {
            // replace
            element.ref.replace(target);
          }

          if (this.__state === REMOVE || target === REMOVE) {
            removeElem(element);
          }

          if (this.__state === BREAK || target === BREAK) {
            return outer.root;
          }

          continue;
        }

        target = this.__execute(visitor.enter, element); // node may be replaced with null,
        // so distinguish between undefined and null in this place

        if (target !== undefined && target !== BREAK && target !== SKIP && target !== REMOVE) {
          // replace
          element.ref.replace(target);
          element.node = target;
        }

        if (this.__state === REMOVE || target === REMOVE) {
          removeElem(element);
          element.node = null;
        }

        if (this.__state === BREAK || target === BREAK) {
          return outer.root;
        } // node may be null


        node = element.node;

        if (!node) {
          continue;
        }

        worklist.push(sentinel);
        leavelist.push(element);

        if (this.__state === SKIP || target === SKIP) {
          continue;
        }

        nodeType = node.type || element.wrap;
        candidates = this.__keys[nodeType];

        if (!candidates) {
          if (this.__fallback) {
            candidates = this.__fallback(node);
          } else {
            throw new Error('Unknown node type ' + nodeType + '.');
          }
        }

        current = candidates.length;

        while ((current -= 1) >= 0) {
          key = candidates[current];
          candidate = node[key];

          if (!candidate) {
            continue;
          }

          if (Array.isArray(candidate)) {
            current2 = candidate.length;

            while ((current2 -= 1) >= 0) {
              if (!candidate[current2]) {
                continue;
              }

              if (isProperty(nodeType, candidates[current])) {
                element = new Element(candidate[current2], [key, current2], 'Property', new Reference(candidate, current2));
              } else if (isNode(candidate[current2])) {
                element = new Element(candidate[current2], [key, current2], null, new Reference(candidate, current2));
              } else {
                continue;
              }

              worklist.push(element);
            }
          } else if (isNode(candidate)) {
            worklist.push(new Element(candidate, key, null, new Reference(node, key)));
          }
        }
      }

      return outer.root;
    };

    function traverse(root, visitor) {
      var controller = new Controller();
      return controller.traverse(root, visitor);
    }

    function replace(root, visitor) {
      var controller = new Controller();
      return controller.replace(root, visitor);
    }

    function extendCommentRange(comment, tokens) {
      var target;
      target = upperBound(tokens, function search(token) {
        return token.range[0] > comment.range[0];
      });
      comment.extendedRange = [comment.range[0], comment.range[1]];

      if (target !== tokens.length) {
        comment.extendedRange[1] = tokens[target].range[0];
      }

      target -= 1;

      if (target >= 0) {
        comment.extendedRange[0] = tokens[target].range[1];
      }

      return comment;
    }

    function attachComments(tree, providedComments, tokens) {
      // At first, we should calculate extended comment ranges.
      var comments = [],
          comment,
          len,
          i,
          cursor;

      if (!tree.range) {
        throw new Error('attachComments needs range information');
      } // tokens array is empty, we attach comments to tree as 'leadingComments'


      if (!tokens.length) {
        if (providedComments.length) {
          for (i = 0, len = providedComments.length; i < len; i += 1) {
            comment = deepCopy(providedComments[i]);
            comment.extendedRange = [0, tree.range[0]];
            comments.push(comment);
          }

          tree.leadingComments = comments;
        }

        return tree;
      }

      for (i = 0, len = providedComments.length; i < len; i += 1) {
        comments.push(extendCommentRange(deepCopy(providedComments[i]), tokens));
      } // This is based on John Freeman's implementation.


      cursor = 0;
      traverse(tree, {
        enter: function (node) {
          var comment;

          while (cursor < comments.length) {
            comment = comments[cursor];

            if (comment.extendedRange[1] > node.range[0]) {
              break;
            }

            if (comment.extendedRange[1] === node.range[0]) {
              if (!node.leadingComments) {
                node.leadingComments = [];
              }

              node.leadingComments.push(comment);
              comments.splice(cursor, 1);
            } else {
              cursor += 1;
            }
          } // already out of owned node


          if (cursor === comments.length) {
            return VisitorOption.Break;
          }

          if (comments[cursor].extendedRange[0] > node.range[1]) {
            return VisitorOption.Skip;
          }
        }
      });
      cursor = 0;
      traverse(tree, {
        leave: function (node) {
          var comment;

          while (cursor < comments.length) {
            comment = comments[cursor];

            if (node.range[1] < comment.extendedRange[0]) {
              break;
            }

            if (node.range[1] === comment.extendedRange[0]) {
              if (!node.trailingComments) {
                node.trailingComments = [];
              }

              node.trailingComments.push(comment);
              comments.splice(cursor, 1);
            } else {
              cursor += 1;
            }
          } // already out of owned node


          if (cursor === comments.length) {
            return VisitorOption.Break;
          }

          if (comments[cursor].extendedRange[0] > node.range[1]) {
            return VisitorOption.Skip;
          }
        }
      });
      return tree;
    }

    exports.Syntax = Syntax;
    exports.traverse = traverse;
    exports.replace = replace;
    exports.attachComments = attachComments;
    exports.VisitorKeys = VisitorKeys;
    exports.VisitorOption = VisitorOption;
    exports.Controller = Controller;

    exports.cloneEnvironment = function () {
      return clone({});
    };

    return exports;
  })(exports);
  /* vim: set sw=4 ts=4 et tw=80 : */

});

var parser = createCommonjsModule(function (module) {
  /*
   * Generated by PEG.js 0.10.0.
   *
   * http://pegjs.org/
   */
  (function (root, factory) {
    if ( module.exports) {
      module.exports = factory();
    }
  })(commonjsGlobal, function () {

    function peg$subclass(child, parent) {
      function ctor() {
        this.constructor = child;
      }

      ctor.prototype = parent.prototype;
      child.prototype = new ctor();
    }

    function peg$SyntaxError(message, expected, found, location) {
      this.message = message;
      this.expected = expected;
      this.found = found;
      this.location = location;
      this.name = "SyntaxError";

      if (typeof Error.captureStackTrace === "function") {
        Error.captureStackTrace(this, peg$SyntaxError);
      }
    }

    peg$subclass(peg$SyntaxError, Error);

    peg$SyntaxError.buildMessage = function (expected, found) {
      var DESCRIBE_EXPECTATION_FNS = {
        literal: function literal(expectation) {
          return "\"" + literalEscape(expectation.text) + "\"";
        },
        "class": function _class(expectation) {
          var escapedParts = "",
              i;

          for (i = 0; i < expectation.parts.length; i++) {
            escapedParts += expectation.parts[i] instanceof Array ? classEscape(expectation.parts[i][0]) + "-" + classEscape(expectation.parts[i][1]) : classEscape(expectation.parts[i]);
          }

          return "[" + (expectation.inverted ? "^" : "") + escapedParts + "]";
        },
        any: function any(expectation) {
          return "any character";
        },
        end: function end(expectation) {
          return "end of input";
        },
        other: function other(expectation) {
          return expectation.description;
        }
      };

      function hex(ch) {
        return ch.charCodeAt(0).toString(16).toUpperCase();
      }

      function literalEscape(s) {
        return s.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\0/g, '\\0').replace(/\t/g, '\\t').replace(/\n/g, '\\n').replace(/\r/g, '\\r').replace(/[\x00-\x0F]/g, function (ch) {
          return '\\x0' + hex(ch);
        }).replace(/[\x10-\x1F\x7F-\x9F]/g, function (ch) {
          return '\\x' + hex(ch);
        });
      }

      function classEscape(s) {
        return s.replace(/\\/g, '\\\\').replace(/\]/g, '\\]').replace(/\^/g, '\\^').replace(/-/g, '\\-').replace(/\0/g, '\\0').replace(/\t/g, '\\t').replace(/\n/g, '\\n').replace(/\r/g, '\\r').replace(/[\x00-\x0F]/g, function (ch) {
          return '\\x0' + hex(ch);
        }).replace(/[\x10-\x1F\x7F-\x9F]/g, function (ch) {
          return '\\x' + hex(ch);
        });
      }

      function describeExpectation(expectation) {
        return DESCRIBE_EXPECTATION_FNS[expectation.type](expectation);
      }

      function describeExpected(expected) {
        var descriptions = new Array(expected.length),
            i,
            j;

        for (i = 0; i < expected.length; i++) {
          descriptions[i] = describeExpectation(expected[i]);
        }

        descriptions.sort();

        if (descriptions.length > 0) {
          for (i = 1, j = 1; i < descriptions.length; i++) {
            if (descriptions[i - 1] !== descriptions[i]) {
              descriptions[j] = descriptions[i];
              j++;
            }
          }

          descriptions.length = j;
        }

        switch (descriptions.length) {
          case 1:
            return descriptions[0];

          case 2:
            return descriptions[0] + " or " + descriptions[1];

          default:
            return descriptions.slice(0, -1).join(", ") + ", or " + descriptions[descriptions.length - 1];
        }
      }

      function describeFound(found) {
        return found ? "\"" + literalEscape(found) + "\"" : "end of input";
      }

      return "Expected " + describeExpected(expected) + " but " + describeFound(found) + " found.";
    };

    function peg$parse(input, options) {
      options = options !== void 0 ? options : {};

      var peg$FAILED = {},
          peg$startRuleFunctions = {
        start: peg$parsestart
      },
          peg$startRuleFunction = peg$parsestart,
          peg$c0 = function peg$c0(ss) {
        return ss.length === 1 ? ss[0] : {
          type: 'matches',
          selectors: ss
        };
      },
          peg$c1 = function peg$c1() {
        return void 0;
      },
          peg$c2 = " ",
          peg$c3 = peg$literalExpectation(" ", false),
          peg$c4 = /^[^ [\],():#!=><~+.]/,
          peg$c5 = peg$classExpectation([" ", "[", "]", ",", "(", ")", ":", "#", "!", "=", ">", "<", "~", "+", "."], true, false),
          peg$c6 = function peg$c6(i) {
        return i.join('');
      },
          peg$c7 = ">",
          peg$c8 = peg$literalExpectation(">", false),
          peg$c9 = function peg$c9() {
        return 'child';
      },
          peg$c10 = "~",
          peg$c11 = peg$literalExpectation("~", false),
          peg$c12 = function peg$c12() {
        return 'sibling';
      },
          peg$c13 = "+",
          peg$c14 = peg$literalExpectation("+", false),
          peg$c15 = function peg$c15() {
        return 'adjacent';
      },
          peg$c16 = function peg$c16() {
        return 'descendant';
      },
          peg$c17 = ",",
          peg$c18 = peg$literalExpectation(",", false),
          peg$c19 = function peg$c19(s, ss) {
        return [s].concat(ss.map(function (s) {
          return s[3];
        }));
      },
          peg$c20 = function peg$c20(a, ops) {
        return ops.reduce(function (memo, rhs) {
          return {
            type: rhs[0],
            left: memo,
            right: rhs[1]
          };
        }, a);
      },
          peg$c21 = "!",
          peg$c22 = peg$literalExpectation("!", false),
          peg$c23 = function peg$c23(subject, as) {
        var b = as.length === 1 ? as[0] : {
          type: 'compound',
          selectors: as
        };
        if (subject) b.subject = true;
        return b;
      },
          peg$c24 = "*",
          peg$c25 = peg$literalExpectation("*", false),
          peg$c26 = function peg$c26(a) {
        return {
          type: 'wildcard',
          value: a
        };
      },
          peg$c27 = "#",
          peg$c28 = peg$literalExpectation("#", false),
          peg$c29 = function peg$c29(i) {
        return {
          type: 'identifier',
          value: i
        };
      },
          peg$c30 = "[",
          peg$c31 = peg$literalExpectation("[", false),
          peg$c32 = "]",
          peg$c33 = peg$literalExpectation("]", false),
          peg$c34 = function peg$c34(v) {
        return v;
      },
          peg$c35 = /^[><!]/,
          peg$c36 = peg$classExpectation([">", "<", "!"], false, false),
          peg$c37 = "=",
          peg$c38 = peg$literalExpectation("=", false),
          peg$c39 = function peg$c39(a) {
        return (a || '') + '=';
      },
          peg$c40 = /^[><]/,
          peg$c41 = peg$classExpectation([">", "<"], false, false),
          peg$c42 = ".",
          peg$c43 = peg$literalExpectation(".", false),
          peg$c44 = function peg$c44(name, op, value) {
        return {
          type: 'attribute',
          name: name,
          operator: op,
          value: value
        };
      },
          peg$c45 = function peg$c45(name) {
        return {
          type: 'attribute',
          name: name
        };
      },
          peg$c46 = "\"",
          peg$c47 = peg$literalExpectation("\"", false),
          peg$c48 = /^[^\\"]/,
          peg$c49 = peg$classExpectation(["\\", "\""], true, false),
          peg$c50 = "\\",
          peg$c51 = peg$literalExpectation("\\", false),
          peg$c52 = peg$anyExpectation(),
          peg$c53 = function peg$c53(a, b) {
        return a + b;
      },
          peg$c54 = function peg$c54(d) {
        return {
          type: 'literal',
          value: strUnescape(d.join(''))
        };
      },
          peg$c55 = "'",
          peg$c56 = peg$literalExpectation("'", false),
          peg$c57 = /^[^\\']/,
          peg$c58 = peg$classExpectation(["\\", "'"], true, false),
          peg$c59 = /^[0-9]/,
          peg$c60 = peg$classExpectation([["0", "9"]], false, false),
          peg$c61 = function peg$c61(a, b) {
        // Can use `a.flat().join('')` once supported
        var leadingDecimals = a ? [].concat.apply([], a).join('') : '';
        return {
          type: 'literal',
          value: parseFloat(leadingDecimals + b.join(''))
        };
      },
          peg$c62 = function peg$c62(i) {
        return {
          type: 'literal',
          value: i
        };
      },
          peg$c63 = "type(",
          peg$c64 = peg$literalExpectation("type(", false),
          peg$c65 = /^[^ )]/,
          peg$c66 = peg$classExpectation([" ", ")"], true, false),
          peg$c67 = ")",
          peg$c68 = peg$literalExpectation(")", false),
          peg$c69 = function peg$c69(t) {
        return {
          type: 'type',
          value: t.join('')
        };
      },
          peg$c70 = /^[imsu]/,
          peg$c71 = peg$classExpectation(["i", "m", "s", "u"], false, false),
          peg$c72 = "/",
          peg$c73 = peg$literalExpectation("/", false),
          peg$c74 = /^[^\/]/,
          peg$c75 = peg$classExpectation(["/"], true, false),
          peg$c76 = function peg$c76(d, flgs) {
        return {
          type: 'regexp',
          value: new RegExp(d.join(''), flgs ? flgs.join('') : '')
        };
      },
          peg$c77 = function peg$c77(i, is) {
        return {
          type: 'field',
          name: is.reduce(function (memo, p) {
            return memo + p[0] + p[1];
          }, i)
        };
      },
          peg$c78 = ":not(",
          peg$c79 = peg$literalExpectation(":not(", false),
          peg$c80 = function peg$c80(ss) {
        return {
          type: 'not',
          selectors: ss
        };
      },
          peg$c81 = ":matches(",
          peg$c82 = peg$literalExpectation(":matches(", false),
          peg$c83 = function peg$c83(ss) {
        return {
          type: 'matches',
          selectors: ss
        };
      },
          peg$c84 = ":has(",
          peg$c85 = peg$literalExpectation(":has(", false),
          peg$c86 = function peg$c86(ss) {
        return {
          type: 'has',
          selectors: ss
        };
      },
          peg$c87 = ":first-child",
          peg$c88 = peg$literalExpectation(":first-child", false),
          peg$c89 = function peg$c89() {
        return nth(1);
      },
          peg$c90 = ":last-child",
          peg$c91 = peg$literalExpectation(":last-child", false),
          peg$c92 = function peg$c92() {
        return nthLast(1);
      },
          peg$c93 = ":nth-child(",
          peg$c94 = peg$literalExpectation(":nth-child(", false),
          peg$c95 = function peg$c95(n) {
        return nth(parseInt(n.join(''), 10));
      },
          peg$c96 = ":nth-last-child(",
          peg$c97 = peg$literalExpectation(":nth-last-child(", false),
          peg$c98 = function peg$c98(n) {
        return nthLast(parseInt(n.join(''), 10));
      },
          peg$c99 = ":",
          peg$c100 = peg$literalExpectation(":", false),
          peg$c101 = "statement",
          peg$c102 = peg$literalExpectation("statement", true),
          peg$c103 = "expression",
          peg$c104 = peg$literalExpectation("expression", true),
          peg$c105 = "declaration",
          peg$c106 = peg$literalExpectation("declaration", true),
          peg$c107 = "function",
          peg$c108 = peg$literalExpectation("function", true),
          peg$c109 = "pattern",
          peg$c110 = peg$literalExpectation("pattern", true),
          peg$c111 = function peg$c111(c) {
        return {
          type: 'class',
          name: c
        };
      },
          peg$currPos = 0,
          peg$posDetailsCache = [{
        line: 1,
        column: 1
      }],
          peg$maxFailPos = 0,
          peg$maxFailExpected = [],
          peg$resultsCache = {},
          peg$result;

      if ("startRule" in options) {
        if (!(options.startRule in peg$startRuleFunctions)) {
          throw new Error("Can't start parsing from rule \"" + options.startRule + "\".");
        }

        peg$startRuleFunction = peg$startRuleFunctions[options.startRule];
      }

      function peg$literalExpectation(text, ignoreCase) {
        return {
          type: "literal",
          text: text,
          ignoreCase: ignoreCase
        };
      }

      function peg$classExpectation(parts, inverted, ignoreCase) {
        return {
          type: "class",
          parts: parts,
          inverted: inverted,
          ignoreCase: ignoreCase
        };
      }

      function peg$anyExpectation() {
        return {
          type: "any"
        };
      }

      function peg$endExpectation() {
        return {
          type: "end"
        };
      }

      function peg$computePosDetails(pos) {
        var details = peg$posDetailsCache[pos],
            p;

        if (details) {
          return details;
        } else {
          p = pos - 1;

          while (!peg$posDetailsCache[p]) {
            p--;
          }

          details = peg$posDetailsCache[p];
          details = {
            line: details.line,
            column: details.column
          };

          while (p < pos) {
            if (input.charCodeAt(p) === 10) {
              details.line++;
              details.column = 1;
            } else {
              details.column++;
            }

            p++;
          }

          peg$posDetailsCache[pos] = details;
          return details;
        }
      }

      function peg$computeLocation(startPos, endPos) {
        var startPosDetails = peg$computePosDetails(startPos),
            endPosDetails = peg$computePosDetails(endPos);
        return {
          start: {
            offset: startPos,
            line: startPosDetails.line,
            column: startPosDetails.column
          },
          end: {
            offset: endPos,
            line: endPosDetails.line,
            column: endPosDetails.column
          }
        };
      }

      function peg$fail(expected) {
        if (peg$currPos < peg$maxFailPos) {
          return;
        }

        if (peg$currPos > peg$maxFailPos) {
          peg$maxFailPos = peg$currPos;
          peg$maxFailExpected = [];
        }

        peg$maxFailExpected.push(expected);
      }

      function peg$buildStructuredError(expected, found, location) {
        return new peg$SyntaxError(peg$SyntaxError.buildMessage(expected, found), expected, found, location);
      }

      function peg$parsestart() {
        var s0, s1, s2, s3;
        var key = peg$currPos * 30 + 0,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;
        s1 = peg$parse_();

        if (s1 !== peg$FAILED) {
          s2 = peg$parseselectors();

          if (s2 !== peg$FAILED) {
            s3 = peg$parse_();

            if (s3 !== peg$FAILED) {
              s1 = peg$c0(s2);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        if (s0 === peg$FAILED) {
          s0 = peg$currPos;
          s1 = peg$parse_();

          if (s1 !== peg$FAILED) {
            s1 = peg$c1();
          }

          s0 = s1;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parse_() {
        var s0, s1;
        var key = peg$currPos * 30 + 1,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = [];

        if (input.charCodeAt(peg$currPos) === 32) {
          s1 = peg$c2;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c3);
          }
        }

        while (s1 !== peg$FAILED) {
          s0.push(s1);

          if (input.charCodeAt(peg$currPos) === 32) {
            s1 = peg$c2;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c3);
            }
          }
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseidentifierName() {
        var s0, s1, s2;
        var key = peg$currPos * 30 + 2,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;
        s1 = [];

        if (peg$c4.test(input.charAt(peg$currPos))) {
          s2 = input.charAt(peg$currPos);
          peg$currPos++;
        } else {
          s2 = peg$FAILED;

          {
            peg$fail(peg$c5);
          }
        }

        if (s2 !== peg$FAILED) {
          while (s2 !== peg$FAILED) {
            s1.push(s2);

            if (peg$c4.test(input.charAt(peg$currPos))) {
              s2 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s2 = peg$FAILED;

              {
                peg$fail(peg$c5);
              }
            }
          }
        } else {
          s1 = peg$FAILED;
        }

        if (s1 !== peg$FAILED) {
          s1 = peg$c6(s1);
        }

        s0 = s1;
        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsebinaryOp() {
        var s0, s1, s2, s3;
        var key = peg$currPos * 30 + 3,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;
        s1 = peg$parse_();

        if (s1 !== peg$FAILED) {
          if (input.charCodeAt(peg$currPos) === 62) {
            s2 = peg$c7;
            peg$currPos++;
          } else {
            s2 = peg$FAILED;

            {
              peg$fail(peg$c8);
            }
          }

          if (s2 !== peg$FAILED) {
            s3 = peg$parse_();

            if (s3 !== peg$FAILED) {
              s1 = peg$c9();
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        if (s0 === peg$FAILED) {
          s0 = peg$currPos;
          s1 = peg$parse_();

          if (s1 !== peg$FAILED) {
            if (input.charCodeAt(peg$currPos) === 126) {
              s2 = peg$c10;
              peg$currPos++;
            } else {
              s2 = peg$FAILED;

              {
                peg$fail(peg$c11);
              }
            }

            if (s2 !== peg$FAILED) {
              s3 = peg$parse_();

              if (s3 !== peg$FAILED) {
                s1 = peg$c12();
                s0 = s1;
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          if (s0 === peg$FAILED) {
            s0 = peg$currPos;
            s1 = peg$parse_();

            if (s1 !== peg$FAILED) {
              if (input.charCodeAt(peg$currPos) === 43) {
                s2 = peg$c13;
                peg$currPos++;
              } else {
                s2 = peg$FAILED;

                {
                  peg$fail(peg$c14);
                }
              }

              if (s2 !== peg$FAILED) {
                s3 = peg$parse_();

                if (s3 !== peg$FAILED) {
                  s1 = peg$c15();
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }

            if (s0 === peg$FAILED) {
              s0 = peg$currPos;

              if (input.charCodeAt(peg$currPos) === 32) {
                s1 = peg$c2;
                peg$currPos++;
              } else {
                s1 = peg$FAILED;

                {
                  peg$fail(peg$c3);
                }
              }

              if (s1 !== peg$FAILED) {
                s2 = peg$parse_();

                if (s2 !== peg$FAILED) {
                  s1 = peg$c16();
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            }
          }
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseselectors() {
        var s0, s1, s2, s3, s4, s5, s6, s7;
        var key = peg$currPos * 30 + 4,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;
        s1 = peg$parseselector();

        if (s1 !== peg$FAILED) {
          s2 = [];
          s3 = peg$currPos;
          s4 = peg$parse_();

          if (s4 !== peg$FAILED) {
            if (input.charCodeAt(peg$currPos) === 44) {
              s5 = peg$c17;
              peg$currPos++;
            } else {
              s5 = peg$FAILED;

              {
                peg$fail(peg$c18);
              }
            }

            if (s5 !== peg$FAILED) {
              s6 = peg$parse_();

              if (s6 !== peg$FAILED) {
                s7 = peg$parseselector();

                if (s7 !== peg$FAILED) {
                  s4 = [s4, s5, s6, s7];
                  s3 = s4;
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            } else {
              peg$currPos = s3;
              s3 = peg$FAILED;
            }
          } else {
            peg$currPos = s3;
            s3 = peg$FAILED;
          }

          while (s3 !== peg$FAILED) {
            s2.push(s3);
            s3 = peg$currPos;
            s4 = peg$parse_();

            if (s4 !== peg$FAILED) {
              if (input.charCodeAt(peg$currPos) === 44) {
                s5 = peg$c17;
                peg$currPos++;
              } else {
                s5 = peg$FAILED;

                {
                  peg$fail(peg$c18);
                }
              }

              if (s5 !== peg$FAILED) {
                s6 = peg$parse_();

                if (s6 !== peg$FAILED) {
                  s7 = peg$parseselector();

                  if (s7 !== peg$FAILED) {
                    s4 = [s4, s5, s6, s7];
                    s3 = s4;
                  } else {
                    peg$currPos = s3;
                    s3 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            } else {
              peg$currPos = s3;
              s3 = peg$FAILED;
            }
          }

          if (s2 !== peg$FAILED) {
            s1 = peg$c19(s1, s2);
            s0 = s1;
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseselector() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 5,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;
        s1 = peg$parsesequence();

        if (s1 !== peg$FAILED) {
          s2 = [];
          s3 = peg$currPos;
          s4 = peg$parsebinaryOp();

          if (s4 !== peg$FAILED) {
            s5 = peg$parsesequence();

            if (s5 !== peg$FAILED) {
              s4 = [s4, s5];
              s3 = s4;
            } else {
              peg$currPos = s3;
              s3 = peg$FAILED;
            }
          } else {
            peg$currPos = s3;
            s3 = peg$FAILED;
          }

          while (s3 !== peg$FAILED) {
            s2.push(s3);
            s3 = peg$currPos;
            s4 = peg$parsebinaryOp();

            if (s4 !== peg$FAILED) {
              s5 = peg$parsesequence();

              if (s5 !== peg$FAILED) {
                s4 = [s4, s5];
                s3 = s4;
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            } else {
              peg$currPos = s3;
              s3 = peg$FAILED;
            }
          }

          if (s2 !== peg$FAILED) {
            s1 = peg$c20(s1, s2);
            s0 = s1;
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsesequence() {
        var s0, s1, s2, s3;
        var key = peg$currPos * 30 + 6,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.charCodeAt(peg$currPos) === 33) {
          s1 = peg$c21;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c22);
          }
        }

        if (s1 === peg$FAILED) {
          s1 = null;
        }

        if (s1 !== peg$FAILED) {
          s2 = [];
          s3 = peg$parseatom();

          if (s3 !== peg$FAILED) {
            while (s3 !== peg$FAILED) {
              s2.push(s3);
              s3 = peg$parseatom();
            }
          } else {
            s2 = peg$FAILED;
          }

          if (s2 !== peg$FAILED) {
            s1 = peg$c23(s1, s2);
            s0 = s1;
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseatom() {
        var s0;
        var key = peg$currPos * 30 + 7,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$parsewildcard();

        if (s0 === peg$FAILED) {
          s0 = peg$parseidentifier();

          if (s0 === peg$FAILED) {
            s0 = peg$parseattr();

            if (s0 === peg$FAILED) {
              s0 = peg$parsefield();

              if (s0 === peg$FAILED) {
                s0 = peg$parsenegation();

                if (s0 === peg$FAILED) {
                  s0 = peg$parsematches();

                  if (s0 === peg$FAILED) {
                    s0 = peg$parsehas();

                    if (s0 === peg$FAILED) {
                      s0 = peg$parsefirstChild();

                      if (s0 === peg$FAILED) {
                        s0 = peg$parselastChild();

                        if (s0 === peg$FAILED) {
                          s0 = peg$parsenthChild();

                          if (s0 === peg$FAILED) {
                            s0 = peg$parsenthLastChild();

                            if (s0 === peg$FAILED) {
                              s0 = peg$parseclass();
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsewildcard() {
        var s0, s1;
        var key = peg$currPos * 30 + 8,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.charCodeAt(peg$currPos) === 42) {
          s1 = peg$c24;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c25);
          }
        }

        if (s1 !== peg$FAILED) {
          s1 = peg$c26(s1);
        }

        s0 = s1;
        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseidentifier() {
        var s0, s1, s2;
        var key = peg$currPos * 30 + 9,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.charCodeAt(peg$currPos) === 35) {
          s1 = peg$c27;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c28);
          }
        }

        if (s1 === peg$FAILED) {
          s1 = null;
        }

        if (s1 !== peg$FAILED) {
          s2 = peg$parseidentifierName();

          if (s2 !== peg$FAILED) {
            s1 = peg$c29(s2);
            s0 = s1;
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseattr() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 10,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.charCodeAt(peg$currPos) === 91) {
          s1 = peg$c30;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c31);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = peg$parse_();

          if (s2 !== peg$FAILED) {
            s3 = peg$parseattrValue();

            if (s3 !== peg$FAILED) {
              s4 = peg$parse_();

              if (s4 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 93) {
                  s5 = peg$c32;
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c33);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s1 = peg$c34(s3);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseattrOps() {
        var s0, s1, s2;
        var key = peg$currPos * 30 + 11,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (peg$c35.test(input.charAt(peg$currPos))) {
          s1 = input.charAt(peg$currPos);
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c36);
          }
        }

        if (s1 === peg$FAILED) {
          s1 = null;
        }

        if (s1 !== peg$FAILED) {
          if (input.charCodeAt(peg$currPos) === 61) {
            s2 = peg$c37;
            peg$currPos++;
          } else {
            s2 = peg$FAILED;

            {
              peg$fail(peg$c38);
            }
          }

          if (s2 !== peg$FAILED) {
            s1 = peg$c39(s1);
            s0 = s1;
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        if (s0 === peg$FAILED) {
          if (peg$c40.test(input.charAt(peg$currPos))) {
            s0 = input.charAt(peg$currPos);
            peg$currPos++;
          } else {
            s0 = peg$FAILED;

            {
              peg$fail(peg$c41);
            }
          }
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseattrEqOps() {
        var s0, s1, s2;
        var key = peg$currPos * 30 + 12,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.charCodeAt(peg$currPos) === 33) {
          s1 = peg$c21;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c22);
          }
        }

        if (s1 === peg$FAILED) {
          s1 = null;
        }

        if (s1 !== peg$FAILED) {
          if (input.charCodeAt(peg$currPos) === 61) {
            s2 = peg$c37;
            peg$currPos++;
          } else {
            s2 = peg$FAILED;

            {
              peg$fail(peg$c38);
            }
          }

          if (s2 !== peg$FAILED) {
            s1 = peg$c39(s1);
            s0 = s1;
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseattrName() {
        var s0, s1, s2;
        var key = peg$currPos * 30 + 13,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;
        s1 = [];
        s2 = peg$parseidentifierName();

        if (s2 === peg$FAILED) {
          if (input.charCodeAt(peg$currPos) === 46) {
            s2 = peg$c42;
            peg$currPos++;
          } else {
            s2 = peg$FAILED;

            {
              peg$fail(peg$c43);
            }
          }
        }

        if (s2 !== peg$FAILED) {
          while (s2 !== peg$FAILED) {
            s1.push(s2);
            s2 = peg$parseidentifierName();

            if (s2 === peg$FAILED) {
              if (input.charCodeAt(peg$currPos) === 46) {
                s2 = peg$c42;
                peg$currPos++;
              } else {
                s2 = peg$FAILED;

                {
                  peg$fail(peg$c43);
                }
              }
            }
          }
        } else {
          s1 = peg$FAILED;
        }

        if (s1 !== peg$FAILED) {
          s1 = peg$c6(s1);
        }

        s0 = s1;
        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseattrValue() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 14,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;
        s1 = peg$parseattrName();

        if (s1 !== peg$FAILED) {
          s2 = peg$parse_();

          if (s2 !== peg$FAILED) {
            s3 = peg$parseattrEqOps();

            if (s3 !== peg$FAILED) {
              s4 = peg$parse_();

              if (s4 !== peg$FAILED) {
                s5 = peg$parsetype();

                if (s5 === peg$FAILED) {
                  s5 = peg$parseregex();
                }

                if (s5 !== peg$FAILED) {
                  s1 = peg$c44(s1, s3, s5);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        if (s0 === peg$FAILED) {
          s0 = peg$currPos;
          s1 = peg$parseattrName();

          if (s1 !== peg$FAILED) {
            s2 = peg$parse_();

            if (s2 !== peg$FAILED) {
              s3 = peg$parseattrOps();

              if (s3 !== peg$FAILED) {
                s4 = peg$parse_();

                if (s4 !== peg$FAILED) {
                  s5 = peg$parsestring();

                  if (s5 === peg$FAILED) {
                    s5 = peg$parsenumber();

                    if (s5 === peg$FAILED) {
                      s5 = peg$parsepath();
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s1 = peg$c44(s1, s3, s5);
                    s0 = s1;
                  } else {
                    peg$currPos = s0;
                    s0 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }

          if (s0 === peg$FAILED) {
            s0 = peg$currPos;
            s1 = peg$parseattrName();

            if (s1 !== peg$FAILED) {
              s1 = peg$c45(s1);
            }

            s0 = s1;
          }
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsestring() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 15,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.charCodeAt(peg$currPos) === 34) {
          s1 = peg$c46;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c47);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = [];

          if (peg$c48.test(input.charAt(peg$currPos))) {
            s3 = input.charAt(peg$currPos);
            peg$currPos++;
          } else {
            s3 = peg$FAILED;

            {
              peg$fail(peg$c49);
            }
          }

          if (s3 === peg$FAILED) {
            s3 = peg$currPos;

            if (input.charCodeAt(peg$currPos) === 92) {
              s4 = peg$c50;
              peg$currPos++;
            } else {
              s4 = peg$FAILED;

              {
                peg$fail(peg$c51);
              }
            }

            if (s4 !== peg$FAILED) {
              if (input.length > peg$currPos) {
                s5 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s5 = peg$FAILED;

                {
                  peg$fail(peg$c52);
                }
              }

              if (s5 !== peg$FAILED) {
                s4 = peg$c53(s4, s5);
                s3 = s4;
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            } else {
              peg$currPos = s3;
              s3 = peg$FAILED;
            }
          }

          while (s3 !== peg$FAILED) {
            s2.push(s3);

            if (peg$c48.test(input.charAt(peg$currPos))) {
              s3 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s3 = peg$FAILED;

              {
                peg$fail(peg$c49);
              }
            }

            if (s3 === peg$FAILED) {
              s3 = peg$currPos;

              if (input.charCodeAt(peg$currPos) === 92) {
                s4 = peg$c50;
                peg$currPos++;
              } else {
                s4 = peg$FAILED;

                {
                  peg$fail(peg$c51);
                }
              }

              if (s4 !== peg$FAILED) {
                if (input.length > peg$currPos) {
                  s5 = input.charAt(peg$currPos);
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c52);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s4 = peg$c53(s4, s5);
                  s3 = s4;
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            }
          }

          if (s2 !== peg$FAILED) {
            if (input.charCodeAt(peg$currPos) === 34) {
              s3 = peg$c46;
              peg$currPos++;
            } else {
              s3 = peg$FAILED;

              {
                peg$fail(peg$c47);
              }
            }

            if (s3 !== peg$FAILED) {
              s1 = peg$c54(s2);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        if (s0 === peg$FAILED) {
          s0 = peg$currPos;

          if (input.charCodeAt(peg$currPos) === 39) {
            s1 = peg$c55;
            peg$currPos++;
          } else {
            s1 = peg$FAILED;

            {
              peg$fail(peg$c56);
            }
          }

          if (s1 !== peg$FAILED) {
            s2 = [];

            if (peg$c57.test(input.charAt(peg$currPos))) {
              s3 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s3 = peg$FAILED;

              {
                peg$fail(peg$c58);
              }
            }

            if (s3 === peg$FAILED) {
              s3 = peg$currPos;

              if (input.charCodeAt(peg$currPos) === 92) {
                s4 = peg$c50;
                peg$currPos++;
              } else {
                s4 = peg$FAILED;

                {
                  peg$fail(peg$c51);
                }
              }

              if (s4 !== peg$FAILED) {
                if (input.length > peg$currPos) {
                  s5 = input.charAt(peg$currPos);
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c52);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s4 = peg$c53(s4, s5);
                  s3 = s4;
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              } else {
                peg$currPos = s3;
                s3 = peg$FAILED;
              }
            }

            while (s3 !== peg$FAILED) {
              s2.push(s3);

              if (peg$c57.test(input.charAt(peg$currPos))) {
                s3 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s3 = peg$FAILED;

                {
                  peg$fail(peg$c58);
                }
              }

              if (s3 === peg$FAILED) {
                s3 = peg$currPos;

                if (input.charCodeAt(peg$currPos) === 92) {
                  s4 = peg$c50;
                  peg$currPos++;
                } else {
                  s4 = peg$FAILED;

                  {
                    peg$fail(peg$c51);
                  }
                }

                if (s4 !== peg$FAILED) {
                  if (input.length > peg$currPos) {
                    s5 = input.charAt(peg$currPos);
                    peg$currPos++;
                  } else {
                    s5 = peg$FAILED;

                    {
                      peg$fail(peg$c52);
                    }
                  }

                  if (s5 !== peg$FAILED) {
                    s4 = peg$c53(s4, s5);
                    s3 = s4;
                  } else {
                    peg$currPos = s3;
                    s3 = peg$FAILED;
                  }
                } else {
                  peg$currPos = s3;
                  s3 = peg$FAILED;
                }
              }
            }

            if (s2 !== peg$FAILED) {
              if (input.charCodeAt(peg$currPos) === 39) {
                s3 = peg$c55;
                peg$currPos++;
              } else {
                s3 = peg$FAILED;

                {
                  peg$fail(peg$c56);
                }
              }

              if (s3 !== peg$FAILED) {
                s1 = peg$c54(s2);
                s0 = s1;
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsenumber() {
        var s0, s1, s2, s3;
        var key = peg$currPos * 30 + 16,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;
        s1 = peg$currPos;
        s2 = [];

        if (peg$c59.test(input.charAt(peg$currPos))) {
          s3 = input.charAt(peg$currPos);
          peg$currPos++;
        } else {
          s3 = peg$FAILED;

          {
            peg$fail(peg$c60);
          }
        }

        while (s3 !== peg$FAILED) {
          s2.push(s3);

          if (peg$c59.test(input.charAt(peg$currPos))) {
            s3 = input.charAt(peg$currPos);
            peg$currPos++;
          } else {
            s3 = peg$FAILED;

            {
              peg$fail(peg$c60);
            }
          }
        }

        if (s2 !== peg$FAILED) {
          if (input.charCodeAt(peg$currPos) === 46) {
            s3 = peg$c42;
            peg$currPos++;
          } else {
            s3 = peg$FAILED;

            {
              peg$fail(peg$c43);
            }
          }

          if (s3 !== peg$FAILED) {
            s2 = [s2, s3];
            s1 = s2;
          } else {
            peg$currPos = s1;
            s1 = peg$FAILED;
          }
        } else {
          peg$currPos = s1;
          s1 = peg$FAILED;
        }

        if (s1 === peg$FAILED) {
          s1 = null;
        }

        if (s1 !== peg$FAILED) {
          s2 = [];

          if (peg$c59.test(input.charAt(peg$currPos))) {
            s3 = input.charAt(peg$currPos);
            peg$currPos++;
          } else {
            s3 = peg$FAILED;

            {
              peg$fail(peg$c60);
            }
          }

          if (s3 !== peg$FAILED) {
            while (s3 !== peg$FAILED) {
              s2.push(s3);

              if (peg$c59.test(input.charAt(peg$currPos))) {
                s3 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s3 = peg$FAILED;

                {
                  peg$fail(peg$c60);
                }
              }
            }
          } else {
            s2 = peg$FAILED;
          }

          if (s2 !== peg$FAILED) {
            s1 = peg$c61(s1, s2);
            s0 = s1;
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsepath() {
        var s0, s1;
        var key = peg$currPos * 30 + 17,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;
        s1 = peg$parseidentifierName();

        if (s1 !== peg$FAILED) {
          s1 = peg$c62(s1);
        }

        s0 = s1;
        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsetype() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 18,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.substr(peg$currPos, 5) === peg$c63) {
          s1 = peg$c63;
          peg$currPos += 5;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c64);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = peg$parse_();

          if (s2 !== peg$FAILED) {
            s3 = [];

            if (peg$c65.test(input.charAt(peg$currPos))) {
              s4 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s4 = peg$FAILED;

              {
                peg$fail(peg$c66);
              }
            }

            if (s4 !== peg$FAILED) {
              while (s4 !== peg$FAILED) {
                s3.push(s4);

                if (peg$c65.test(input.charAt(peg$currPos))) {
                  s4 = input.charAt(peg$currPos);
                  peg$currPos++;
                } else {
                  s4 = peg$FAILED;

                  {
                    peg$fail(peg$c66);
                  }
                }
              }
            } else {
              s3 = peg$FAILED;
            }

            if (s3 !== peg$FAILED) {
              s4 = peg$parse_();

              if (s4 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 41) {
                  s5 = peg$c67;
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c68);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s1 = peg$c69(s3);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseflags() {
        var s0, s1;
        var key = peg$currPos * 30 + 19,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = [];

        if (peg$c70.test(input.charAt(peg$currPos))) {
          s1 = input.charAt(peg$currPos);
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c71);
          }
        }

        if (s1 !== peg$FAILED) {
          while (s1 !== peg$FAILED) {
            s0.push(s1);

            if (peg$c70.test(input.charAt(peg$currPos))) {
              s1 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s1 = peg$FAILED;

              {
                peg$fail(peg$c71);
              }
            }
          }
        } else {
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseregex() {
        var s0, s1, s2, s3, s4;
        var key = peg$currPos * 30 + 20,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.charCodeAt(peg$currPos) === 47) {
          s1 = peg$c72;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c73);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = [];

          if (peg$c74.test(input.charAt(peg$currPos))) {
            s3 = input.charAt(peg$currPos);
            peg$currPos++;
          } else {
            s3 = peg$FAILED;

            {
              peg$fail(peg$c75);
            }
          }

          if (s3 !== peg$FAILED) {
            while (s3 !== peg$FAILED) {
              s2.push(s3);

              if (peg$c74.test(input.charAt(peg$currPos))) {
                s3 = input.charAt(peg$currPos);
                peg$currPos++;
              } else {
                s3 = peg$FAILED;

                {
                  peg$fail(peg$c75);
                }
              }
            }
          } else {
            s2 = peg$FAILED;
          }

          if (s2 !== peg$FAILED) {
            if (input.charCodeAt(peg$currPos) === 47) {
              s3 = peg$c72;
              peg$currPos++;
            } else {
              s3 = peg$FAILED;

              {
                peg$fail(peg$c73);
              }
            }

            if (s3 !== peg$FAILED) {
              s4 = peg$parseflags();

              if (s4 === peg$FAILED) {
                s4 = null;
              }

              if (s4 !== peg$FAILED) {
                s1 = peg$c76(s2, s4);
                s0 = s1;
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsefield() {
        var s0, s1, s2, s3, s4, s5, s6;
        var key = peg$currPos * 30 + 21,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.charCodeAt(peg$currPos) === 46) {
          s1 = peg$c42;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c43);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = peg$parseidentifierName();

          if (s2 !== peg$FAILED) {
            s3 = [];
            s4 = peg$currPos;

            if (input.charCodeAt(peg$currPos) === 46) {
              s5 = peg$c42;
              peg$currPos++;
            } else {
              s5 = peg$FAILED;

              {
                peg$fail(peg$c43);
              }
            }

            if (s5 !== peg$FAILED) {
              s6 = peg$parseidentifierName();

              if (s6 !== peg$FAILED) {
                s5 = [s5, s6];
                s4 = s5;
              } else {
                peg$currPos = s4;
                s4 = peg$FAILED;
              }
            } else {
              peg$currPos = s4;
              s4 = peg$FAILED;
            }

            while (s4 !== peg$FAILED) {
              s3.push(s4);
              s4 = peg$currPos;

              if (input.charCodeAt(peg$currPos) === 46) {
                s5 = peg$c42;
                peg$currPos++;
              } else {
                s5 = peg$FAILED;

                {
                  peg$fail(peg$c43);
                }
              }

              if (s5 !== peg$FAILED) {
                s6 = peg$parseidentifierName();

                if (s6 !== peg$FAILED) {
                  s5 = [s5, s6];
                  s4 = s5;
                } else {
                  peg$currPos = s4;
                  s4 = peg$FAILED;
                }
              } else {
                peg$currPos = s4;
                s4 = peg$FAILED;
              }
            }

            if (s3 !== peg$FAILED) {
              s1 = peg$c77(s2, s3);
              s0 = s1;
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsenegation() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 22,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.substr(peg$currPos, 5) === peg$c78) {
          s1 = peg$c78;
          peg$currPos += 5;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c79);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = peg$parse_();

          if (s2 !== peg$FAILED) {
            s3 = peg$parseselectors();

            if (s3 !== peg$FAILED) {
              s4 = peg$parse_();

              if (s4 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 41) {
                  s5 = peg$c67;
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c68);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s1 = peg$c80(s3);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsematches() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 23,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.substr(peg$currPos, 9) === peg$c81) {
          s1 = peg$c81;
          peg$currPos += 9;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c82);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = peg$parse_();

          if (s2 !== peg$FAILED) {
            s3 = peg$parseselectors();

            if (s3 !== peg$FAILED) {
              s4 = peg$parse_();

              if (s4 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 41) {
                  s5 = peg$c67;
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c68);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s1 = peg$c83(s3);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsehas() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 24,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.substr(peg$currPos, 5) === peg$c84) {
          s1 = peg$c84;
          peg$currPos += 5;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c85);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = peg$parse_();

          if (s2 !== peg$FAILED) {
            s3 = peg$parseselectors();

            if (s3 !== peg$FAILED) {
              s4 = peg$parse_();

              if (s4 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 41) {
                  s5 = peg$c67;
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c68);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s1 = peg$c86(s3);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsefirstChild() {
        var s0, s1;
        var key = peg$currPos * 30 + 25,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.substr(peg$currPos, 12) === peg$c87) {
          s1 = peg$c87;
          peg$currPos += 12;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c88);
          }
        }

        if (s1 !== peg$FAILED) {
          s1 = peg$c89();
        }

        s0 = s1;
        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parselastChild() {
        var s0, s1;
        var key = peg$currPos * 30 + 26,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.substr(peg$currPos, 11) === peg$c90) {
          s1 = peg$c90;
          peg$currPos += 11;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c91);
          }
        }

        if (s1 !== peg$FAILED) {
          s1 = peg$c92();
        }

        s0 = s1;
        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsenthChild() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 27,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.substr(peg$currPos, 11) === peg$c93) {
          s1 = peg$c93;
          peg$currPos += 11;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c94);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = peg$parse_();

          if (s2 !== peg$FAILED) {
            s3 = [];

            if (peg$c59.test(input.charAt(peg$currPos))) {
              s4 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s4 = peg$FAILED;

              {
                peg$fail(peg$c60);
              }
            }

            if (s4 !== peg$FAILED) {
              while (s4 !== peg$FAILED) {
                s3.push(s4);

                if (peg$c59.test(input.charAt(peg$currPos))) {
                  s4 = input.charAt(peg$currPos);
                  peg$currPos++;
                } else {
                  s4 = peg$FAILED;

                  {
                    peg$fail(peg$c60);
                  }
                }
              }
            } else {
              s3 = peg$FAILED;
            }

            if (s3 !== peg$FAILED) {
              s4 = peg$parse_();

              if (s4 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 41) {
                  s5 = peg$c67;
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c68);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s1 = peg$c95(s3);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parsenthLastChild() {
        var s0, s1, s2, s3, s4, s5;
        var key = peg$currPos * 30 + 28,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.substr(peg$currPos, 16) === peg$c96) {
          s1 = peg$c96;
          peg$currPos += 16;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c97);
          }
        }

        if (s1 !== peg$FAILED) {
          s2 = peg$parse_();

          if (s2 !== peg$FAILED) {
            s3 = [];

            if (peg$c59.test(input.charAt(peg$currPos))) {
              s4 = input.charAt(peg$currPos);
              peg$currPos++;
            } else {
              s4 = peg$FAILED;

              {
                peg$fail(peg$c60);
              }
            }

            if (s4 !== peg$FAILED) {
              while (s4 !== peg$FAILED) {
                s3.push(s4);

                if (peg$c59.test(input.charAt(peg$currPos))) {
                  s4 = input.charAt(peg$currPos);
                  peg$currPos++;
                } else {
                  s4 = peg$FAILED;

                  {
                    peg$fail(peg$c60);
                  }
                }
              }
            } else {
              s3 = peg$FAILED;
            }

            if (s3 !== peg$FAILED) {
              s4 = peg$parse_();

              if (s4 !== peg$FAILED) {
                if (input.charCodeAt(peg$currPos) === 41) {
                  s5 = peg$c67;
                  peg$currPos++;
                } else {
                  s5 = peg$FAILED;

                  {
                    peg$fail(peg$c68);
                  }
                }

                if (s5 !== peg$FAILED) {
                  s1 = peg$c98(s3);
                  s0 = s1;
                } else {
                  peg$currPos = s0;
                  s0 = peg$FAILED;
                }
              } else {
                peg$currPos = s0;
                s0 = peg$FAILED;
              }
            } else {
              peg$currPos = s0;
              s0 = peg$FAILED;
            }
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function peg$parseclass() {
        var s0, s1, s2;
        var key = peg$currPos * 30 + 29,
            cached = peg$resultsCache[key];

        if (cached) {
          peg$currPos = cached.nextPos;
          return cached.result;
        }

        s0 = peg$currPos;

        if (input.charCodeAt(peg$currPos) === 58) {
          s1 = peg$c99;
          peg$currPos++;
        } else {
          s1 = peg$FAILED;

          {
            peg$fail(peg$c100);
          }
        }

        if (s1 !== peg$FAILED) {
          if (input.substr(peg$currPos, 9).toLowerCase() === peg$c101) {
            s2 = input.substr(peg$currPos, 9);
            peg$currPos += 9;
          } else {
            s2 = peg$FAILED;

            {
              peg$fail(peg$c102);
            }
          }

          if (s2 === peg$FAILED) {
            if (input.substr(peg$currPos, 10).toLowerCase() === peg$c103) {
              s2 = input.substr(peg$currPos, 10);
              peg$currPos += 10;
            } else {
              s2 = peg$FAILED;

              {
                peg$fail(peg$c104);
              }
            }

            if (s2 === peg$FAILED) {
              if (input.substr(peg$currPos, 11).toLowerCase() === peg$c105) {
                s2 = input.substr(peg$currPos, 11);
                peg$currPos += 11;
              } else {
                s2 = peg$FAILED;

                {
                  peg$fail(peg$c106);
                }
              }

              if (s2 === peg$FAILED) {
                if (input.substr(peg$currPos, 8).toLowerCase() === peg$c107) {
                  s2 = input.substr(peg$currPos, 8);
                  peg$currPos += 8;
                } else {
                  s2 = peg$FAILED;

                  {
                    peg$fail(peg$c108);
                  }
                }

                if (s2 === peg$FAILED) {
                  if (input.substr(peg$currPos, 7).toLowerCase() === peg$c109) {
                    s2 = input.substr(peg$currPos, 7);
                    peg$currPos += 7;
                  } else {
                    s2 = peg$FAILED;

                    {
                      peg$fail(peg$c110);
                    }
                  }
                }
              }
            }
          }

          if (s2 !== peg$FAILED) {
            s1 = peg$c111(s2);
            s0 = s1;
          } else {
            peg$currPos = s0;
            s0 = peg$FAILED;
          }
        } else {
          peg$currPos = s0;
          s0 = peg$FAILED;
        }

        peg$resultsCache[key] = {
          nextPos: peg$currPos,
          result: s0
        };
        return s0;
      }

      function nth(n) {
        return {
          type: 'nth-child',
          index: {
            type: 'literal',
            value: n
          }
        };
      }

      function nthLast(n) {
        return {
          type: 'nth-last-child',
          index: {
            type: 'literal',
            value: n
          }
        };
      }

      function strUnescape(s) {
        return s.replace(/\\(.)/g, function (match, ch) {
          switch (ch) {
            case 'b':
              return '\b';

            case 'f':
              return '\f';

            case 'n':
              return '\n';

            case 'r':
              return '\r';

            case 't':
              return '\t';

            case 'v':
              return '\v';

            default:
              return ch;
          }
        });
      }

      peg$result = peg$startRuleFunction();

      if (peg$result !== peg$FAILED && peg$currPos === input.length) {
        return peg$result;
      } else {
        if (peg$result !== peg$FAILED && peg$currPos < input.length) {
          peg$fail(peg$endExpectation());
        }

        throw peg$buildStructuredError(peg$maxFailExpected, peg$maxFailPos < input.length ? input.charAt(peg$maxFailPos) : null, peg$maxFailPos < input.length ? peg$computeLocation(peg$maxFailPos, peg$maxFailPos + 1) : peg$computeLocation(peg$maxFailPos, peg$maxFailPos));
      }
    }

    return {
      SyntaxError: peg$SyntaxError,
      parse: peg$parse
    };
  });
});

function _objectEntries(obj) {
  var entries = [];
  var keys = Object.keys(obj);

  for (var k = 0; k < keys.length; k++) entries.push([keys[k], obj[keys[k]]]);

  return entries;
}
/**
* @typedef {"LEFT_SIDE"|"RIGHT_SIDE"} Side
*/

var LEFT_SIDE = 'LEFT_SIDE';
var RIGHT_SIDE = 'RIGHT_SIDE';
/**
 * @external AST
 * @see https://esprima.readthedocs.io/en/latest/syntax-tree-format.html
 */

/**
 * One of the rules of `grammar.pegjs`
 * @typedef {PlainObject} SelectorAST
 * @see grammar.pegjs
*/

/**
 * The `sequence` production of `grammar.pegjs`
 * @typedef {PlainObject} SelectorSequenceAST
*/

/**
 * Get the value of a property which may be multiple levels down
 * in the object.
 * @param {?PlainObject} obj
 * @param {string} key
 * @returns {undefined|boolean|string|number|external:AST}
 */

function getPath(obj, key) {
  var keys = key.split('.');

  for (var i = 0; i < keys.length; i++) {
    if (obj == null) {
      return obj;
    }

    obj = obj[keys[i]];
  }

  return obj;
}
/**
 * Determine whether `node` can be reached by following `path`,
 * starting at `ancestor`.
 * @param {?external:AST} node
 * @param {?external:AST} ancestor
 * @param {string[]} path
 * @returns {boolean}
 */


function inPath(node, ancestor, path) {
  if (path.length === 0) {
    return node === ancestor;
  }

  if (ancestor == null) {
    return false;
  }

  var field = ancestor[path[0]];
  var remainingPath = path.slice(1);

  if (Array.isArray(field)) {
    for (var i = 0, l = field.length; i < l; ++i) {
      if (inPath(node, field[i], remainingPath)) {
        return true;
      }
    }

    return false;
  } else {
    return inPath(node, field, remainingPath);
  }
}
/**
 * Given a `node` and its ancestors, determine if `node` is matched
 * by `selector`.
 * @param {?external:AST} node
 * @param {?SelectorAST} selector
 * @param {external:AST[]} [ancestry=[]]
 * @throws {Error} Unknowns (operator, class name, selector type, or
 * selector value type)
 * @returns {boolean}
 */


function matches(node, selector, ancestry) {
  if (!selector) {
    return true;
  }

  if (!node) {
    return false;
  }

  if (!ancestry) {
    ancestry = [];
  }

  switch (selector.type) {
    case 'wildcard':
      return true;

    case 'identifier':
      return selector.value.toLowerCase() === node.type.toLowerCase();

    case 'field':
      {
        var path = selector.name.split('.');
        var ancestor = ancestry[path.length - 1];
        return inPath(node, ancestor, path);
      }

    case 'matches':
      for (var i = 0, l = selector.selectors.length; i < l; ++i) {
        if (matches(node, selector.selectors[i], ancestry)) {
          return true;
        }
      }

      return false;

    case 'compound':
      for (var _i = 0, _l = selector.selectors.length; _i < _l; ++_i) {
        if (!matches(node, selector.selectors[_i], ancestry)) {
          return false;
        }
      }

      return true;

    case 'not':
      for (var _i2 = 0, _l2 = selector.selectors.length; _i2 < _l2; ++_i2) {
        if (matches(node, selector.selectors[_i2], ancestry)) {
          return false;
        }
      }

      return true;

    case 'has':
      {
        var _ret = function () {
          var collector = [];

          var _loop = function _loop(_i3, _l3) {
            var a = [];
            estraverse.traverse(node, {
              enter: function enter(node, parent) {
                if (parent != null) {
                  a.unshift(parent);
                }

                if (matches(node, selector.selectors[_i3], a)) {
                  collector.push(node);
                }
              },
              leave: function leave() {
                a.shift();
              },
              fallback: 'iteration'
            });
          };

          for (var _i3 = 0, _l3 = selector.selectors.length; _i3 < _l3; ++_i3) {
            _loop(_i3);
          }

          return {
            v: collector.length !== 0
          };
        }();

        if (_typeof(_ret) === "object") return _ret.v;
      }

    case 'child':
      if (matches(node, selector.right, ancestry)) {
        return matches(ancestry[0], selector.left, ancestry.slice(1));
      }

      return false;

    case 'descendant':
      if (matches(node, selector.right, ancestry)) {
        for (var _i4 = 0, _l4 = ancestry.length; _i4 < _l4; ++_i4) {
          if (matches(ancestry[_i4], selector.left, ancestry.slice(_i4 + 1))) {
            return true;
          }
        }
      }

      return false;

    case 'attribute':
      {
        var p = getPath(node, selector.name);

        switch (selector.operator) {
          case void 0:
            return p != null;

          case '=':
            switch (selector.value.type) {
              case 'regexp':
                return typeof p === 'string' && selector.value.value.test(p);

              case 'literal':
                return "".concat(selector.value.value) === "".concat(p);

              case 'type':
                return selector.value.value === _typeof(p);
            }

            throw new Error("Unknown selector value type: ".concat(selector.value.type));

          case '!=':
            switch (selector.value.type) {
              case 'regexp':
                return !selector.value.value.test(p);

              case 'literal':
                return "".concat(selector.value.value) !== "".concat(p);

              case 'type':
                return selector.value.value !== _typeof(p);
            }

            throw new Error("Unknown selector value type: ".concat(selector.value.type));

          case '<=':
            return p <= selector.value.value;

          case '<':
            return p < selector.value.value;

          case '>':
            return p > selector.value.value;

          case '>=':
            return p >= selector.value.value;
        }

        throw new Error("Unknown operator: ".concat(selector.operator));
      }

    case 'sibling':
      return matches(node, selector.right, ancestry) && sibling(node, selector.left, ancestry, LEFT_SIDE) || selector.left.subject && matches(node, selector.left, ancestry) && sibling(node, selector.right, ancestry, RIGHT_SIDE);

    case 'adjacent':
      return matches(node, selector.right, ancestry) && adjacent(node, selector.left, ancestry, LEFT_SIDE) || selector.right.subject && matches(node, selector.left, ancestry) && adjacent(node, selector.right, ancestry, RIGHT_SIDE);

    case 'nth-child':
      return matches(node, selector.right, ancestry) && nthChild(node, ancestry, function () {
        return selector.index.value - 1;
      });

    case 'nth-last-child':
      return matches(node, selector.right, ancestry) && nthChild(node, ancestry, function (length) {
        return length - selector.index.value;
      });

    case 'class':
      switch (selector.name.toLowerCase()) {
        case 'statement':
          if (node.type.slice(-9) === 'Statement') return true;
        // fallthrough: interface Declaration <: Statement { }

        case 'declaration':
          return node.type.slice(-11) === 'Declaration';

        case 'pattern':
          if (node.type.slice(-7) === 'Pattern') return true;
        // fallthrough: interface Expression <: Node, Pattern { }

        case 'expression':
          return node.type.slice(-10) === 'Expression' || node.type.slice(-7) === 'Literal' || node.type === 'Identifier' && (ancestry.length === 0 || ancestry[0].type !== 'MetaProperty') || node.type === 'MetaProperty';

        case 'function':
          return node.type === 'FunctionDeclaration' || node.type === 'FunctionExpression' || node.type === 'ArrowFunctionExpression';
      }

      throw new Error("Unknown class name: ".concat(selector.name));
  }

  throw new Error("Unknown selector type: ".concat(selector.type));
}
/**
 * Determines if the given node has a sibling that matches the
 * given selector.
 * @param {external:AST} node
 * @param {SelectorSequenceAST} selector
 * @param {external:AST[]} ancestry
 * @param {Side} side
 * @returns {boolean}
 */


function sibling(node, selector, ancestry, side) {
  var _ancestry = _slicedToArray(ancestry, 1),
      parent = _ancestry[0];

  if (!parent) {
    return false;
  }

  var keys = estraverse.VisitorKeys[parent.type];

  for (var i = 0, l = keys.length; i < l; ++i) {
    var listProp = parent[keys[i]];

    if (Array.isArray(listProp)) {
      var startIndex = listProp.indexOf(node);

      if (startIndex < 0) {
        continue;
      }

      var lowerBound = void 0,
          upperBound = void 0;

      if (side === LEFT_SIDE) {
        lowerBound = 0;
        upperBound = startIndex;
      } else {
        lowerBound = startIndex + 1;
        upperBound = listProp.length;
      }

      for (var k = lowerBound; k < upperBound; ++k) {
        if (matches(listProp[k], selector, ancestry)) {
          return true;
        }
      }
    }
  }

  return false;
}
/**
 * Determines if the given node has an adjacent sibling that matches
 * the given selector.
 * @param {external:AST} node
 * @param {SelectorSequenceAST} selector
 * @param {external:AST[]} ancestry
 * @param {Side} side
 * @returns {boolean}
 */


function adjacent(node, selector, ancestry, side) {
  var _ancestry2 = _slicedToArray(ancestry, 1),
      parent = _ancestry2[0];

  if (!parent) {
    return false;
  }

  var keys = estraverse.VisitorKeys[parent.type];

  for (var i = 0, l = keys.length; i < l; ++i) {
    var listProp = parent[keys[i]];

    if (Array.isArray(listProp)) {
      var idx = listProp.indexOf(node);

      if (idx < 0) {
        continue;
      }

      if (side === LEFT_SIDE && idx > 0 && matches(listProp[idx - 1], selector, ancestry)) {
        return true;
      }

      if (side === RIGHT_SIDE && idx < listProp.length - 1 && matches(listProp[idx + 1], selector, ancestry)) {
        return true;
      }
    }
  }

  return false;
}
/**
* @callback IndexFunction
* @param {Integer} len Containing list's length
* @returns {Integer}
*/

/**
 * Determines if the given node is the nth child, determined by
 * `idxFn`, which is given the containing list's length.
 * @param {external:AST} node
 * @param {external:AST[]} ancestry
 * @param {IndexFunction} idxFn
 * @returns {boolean}
 */


function nthChild(node, ancestry, idxFn) {
  var _ancestry3 = _slicedToArray(ancestry, 1),
      parent = _ancestry3[0];

  if (!parent) {
    return false;
  }

  var keys = estraverse.VisitorKeys[parent.type];

  for (var i = 0, l = keys.length; i < l; ++i) {
    var listProp = parent[keys[i]];

    if (Array.isArray(listProp)) {
      var idx = listProp.indexOf(node);

      if (idx >= 0 && idx === idxFn(listProp.length)) {
        return true;
      }
    }
  }

  return false;
}
/**
 * For each selector node marked as a subject, find the portion of the
 * selector that the subject must match.
 * @param {SelectorAST} selector
 * @param {SelectorAST} [ancestor] Defaults to `selector`
 * @returns {SelectorAST[]}
 */


function subjects(selector, ancestor) {
  if (selector == null || _typeof(selector) != 'object') {
    return [];
  }

  if (ancestor == null) {
    ancestor = selector;
  }

  var results = selector.subject ? [ancestor] : [];

  for (var _i5 = 0, _Object$entries = _objectEntries(selector); _i5 < _Object$entries.length; _i5++) {
    var _Object$entries$_i = _slicedToArray(_Object$entries[_i5], 2),
        p = _Object$entries$_i[0],
        sel = _Object$entries$_i[1];

    results.push.apply(results, _toConsumableArray(subjects(sel, p === 'left' ? sel : ancestor)));
  }

  return results;
}
/**
* @callback TraverseVisitor
* @param {?external:AST} node
* @param {?external:AST} parent
* @param {external:AST[]} ancestry
*/

/**
 * From a JS AST and a selector AST, collect all JS AST nodes that
 * match the selector.
 * @param {external:AST} ast
 * @param {?SelectorAST} selector
 * @param {TraverseVisitor} visitor
 * @returns {external:AST[]}
 */


function traverse(ast, selector, visitor) {
  if (!selector) {
    return;
  }

  var ancestry = [];
  var altSubjects = subjects(selector);
  estraverse.traverse(ast, {
    enter: function enter(node, parent) {
      if (parent != null) {
        ancestry.unshift(parent);
      }

      if (matches(node, selector, ancestry)) {
        if (altSubjects.length) {
          for (var i = 0, l = altSubjects.length; i < l; ++i) {
            if (matches(node, altSubjects[i], ancestry)) {
              visitor(node, parent, ancestry);
            }

            for (var k = 0, m = ancestry.length; k < m; ++k) {
              var succeedingAncestry = ancestry.slice(k + 1);

              if (matches(ancestry[k], altSubjects[i], succeedingAncestry)) {
                visitor(ancestry[k], parent, succeedingAncestry);
              }
            }
          }
        } else {
          visitor(node, parent, ancestry);
        }
      }
    },
    leave: function leave() {
      ancestry.shift();
    },
    fallback: 'iteration'
  });
}
/**
 * From a JS AST and a selector AST, collect all JS AST nodes that
 * match the selector.
 * @param {external:AST} ast
 * @param {?SelectorAST} selector
 * @returns {external:AST[]}
 */


function match(ast, selector) {
  var results = [];
  traverse(ast, selector, function (node) {
    results.push(node);
  });
  return results;
}
/**
 * Parse a selector string and return its AST.
 * @param {string} selector
 * @returns {SelectorAST}
 */


function parse(selector) {
  return parser.parse(selector);
}
/**
 * Query the code AST using the selector string.
 * @param {external:AST} ast
 * @param {string} selector
 * @returns {external:AST[]}
 */


function query(ast, selector) {
  return match(ast, parse(selector));
}

query.parse = parse;
query.match = match;
query.traverse = traverse;
query.matches = matches;
query.query = query;

export default query;
