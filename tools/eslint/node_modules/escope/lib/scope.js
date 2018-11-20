"use strict";

var _interopRequire = function (obj) { return obj && obj.__esModule ? obj["default"] : obj; };

var _get = function get(object, property, receiver) { var desc = Object.getOwnPropertyDescriptor(object, property); if (desc === undefined) { var parent = Object.getPrototypeOf(object); if (parent === null) { return undefined; } else { return get(parent, property, receiver); } } else if ("value" in desc && desc.writable) { return desc.value; } else { var getter = desc.get; if (getter === undefined) { return undefined; } return getter.call(receiver); } };

var _inherits = function (subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) subClass.__proto__ = superClass; };

var _createClass = (function () { function defineProperties(target, props) { for (var key in props) { var prop = props[key]; prop.configurable = true; if (prop.value) prop.writable = true; } Object.defineProperties(target, props); } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; })();

var _classCallCheck = function (instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } };

Object.defineProperty(exports, "__esModule", {
    value: true
});
/*
  Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>

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

var Syntax = require("estraverse").Syntax;

var Map = _interopRequire(require("es6-map"));

var Reference = _interopRequire(require("./reference"));

var Variable = _interopRequire(require("./variable"));

var Definition = _interopRequire(require("./definition"));

var assert = _interopRequire(require("assert"));

function isStrictScope(scope, block, isMethodDefinition, useDirective) {
    var body, i, iz, stmt, expr;

    // When upper scope is exists and strict, inner scope is also strict.
    if (scope.upper && scope.upper.isStrict) {
        return true;
    }

    // ArrowFunctionExpression's scope is always strict scope.
    if (block.type === Syntax.ArrowFunctionExpression) {
        return true;
    }

    if (isMethodDefinition) {
        return true;
    }

    if (scope.type === "class" || scope.type === "module") {
        return true;
    }

    if (scope.type === "block" || scope.type === "switch") {
        return false;
    }

    if (scope.type === "function") {
        if (block.type === "Program") {
            body = block;
        } else {
            body = block.body;
        }
    } else if (scope.type === "global") {
        body = block;
    } else {
        return false;
    }

    // Search 'use strict' directive.
    if (useDirective) {
        for (i = 0, iz = body.body.length; i < iz; ++i) {
            stmt = body.body[i];
            if (stmt.type !== "DirectiveStatement") {
                break;
            }
            if (stmt.raw === "\"use strict\"" || stmt.raw === "'use strict'") {
                return true;
            }
        }
    } else {
        for (i = 0, iz = body.body.length; i < iz; ++i) {
            stmt = body.body[i];
            if (stmt.type !== Syntax.ExpressionStatement) {
                break;
            }
            expr = stmt.expression;
            if (expr.type !== Syntax.Literal || typeof expr.value !== "string") {
                break;
            }
            if (expr.raw != null) {
                if (expr.raw === "\"use strict\"" || expr.raw === "'use strict'") {
                    return true;
                }
            } else {
                if (expr.value === "use strict") {
                    return true;
                }
            }
        }
    }
    return false;
}

function registerScope(scopeManager, scope) {
    var scopes;

    scopeManager.scopes.push(scope);

    scopes = scopeManager.__nodeToScope.get(scope.block);
    if (scopes) {
        scopes.push(scope);
    } else {
        scopeManager.__nodeToScope.set(scope.block, [scope]);
    }
}

/**
 * @class Scope
 */

var Scope = (function () {
    function Scope(scopeManager, type, upperScope, block, isMethodDefinition) {
        _classCallCheck(this, Scope);

        /**
         * One of 'TDZ', 'module', 'block', 'switch', 'function', 'catch', 'with', 'function', 'class', 'global'.
         * @member {String} Scope#type
         */
        this.type = type;
        /**
        * The scoped {@link Variable}s of this scope, as <code>{ Variable.name
        * : Variable }</code>.
        * @member {Map} Scope#set
        */
        this.set = new Map();
        /**
         * The tainted variables of this scope, as <code>{ Variable.name :
         * boolean }</code>.
         * @member {Map} Scope#taints */
        this.taints = new Map();
        /**
         * Generally, through the lexical scoping of JS you can always know
         * which variable an identifier in the source code refers to. There are
         * a few exceptions to this rule. With 'global' and 'with' scopes you
         * can only decide at runtime which variable a reference refers to.
         * Moreover, if 'eval()' is used in a scope, it might introduce new
         * bindings in this or its prarent scopes.
         * All those scopes are considered 'dynamic'.
         * @member {boolean} Scope#dynamic
         */
        this.dynamic = this.type === "global" || this.type === "with";
        /**
         * A reference to the scope-defining syntax node.
         * @member {esprima.Node} Scope#block
         */
        this.block = block;
        /**
        * The {@link Reference|references} that are not resolved with this scope.
        * @member {Reference[]} Scope#through
        */
        this.through = [];
        /**
        * The scoped {@link Variable}s of this scope. In the case of a
        * 'function' scope this includes the automatic argument <em>arguments</em> as
        * its first element, as well as all further formal arguments.
        * @member {Variable[]} Scope#variables
        */
        this.variables = [];
        /**
        * Any variable {@link Reference|reference} found in this scope. This
        * includes occurrences of local variables as well as variables from
        * parent scopes (including the global scope). For local variables
        * this also includes defining occurrences (like in a 'var' statement).
        * In a 'function' scope this does not include the occurrences of the
        * formal parameter in the parameter list.
        * @member {Reference[]} Scope#references
        */
        this.references = [];

        /**
        * For 'global' and 'function' scopes, this is a self-reference. For
        * other scope types this is the <em>variableScope</em> value of the
        * parent scope.
        * @member {Scope} Scope#variableScope
        */
        this.variableScope = this.type === "global" || this.type === "function" || this.type === "module" ? this : upperScope.variableScope;
        /**
        * Whether this scope is created by a FunctionExpression.
        * @member {boolean} Scope#functionExpressionScope
        */
        this.functionExpressionScope = false;
        /**
        * Whether this is a scope that contains an 'eval()' invocation.
        * @member {boolean} Scope#directCallToEvalScope
        */
        this.directCallToEvalScope = false;
        /**
        * @member {boolean} Scope#thisFound
        */
        this.thisFound = false;

        this.__left = [];

        /**
        * Reference to the parent {@link Scope|scope}.
        * @member {Scope} Scope#upper
        */
        this.upper = upperScope;
        /**
        * Whether 'use strict' is in effect in this scope.
        * @member {boolean} Scope#isStrict
        */
        this.isStrict = isStrictScope(this, block, isMethodDefinition, scopeManager.__useDirective());

        /**
        * List of nested {@link Scope}s.
        * @member {Scope[]} Scope#childScopes
        */
        this.childScopes = [];
        if (this.upper) {
            this.upper.childScopes.push(this);
        }

        registerScope(scopeManager, this);
    }

    _createClass(Scope, {
        __shouldStaticallyClose: {
            value: function __shouldStaticallyClose(scopeManager) {
                return !this.dynamic || scopeManager.__isOptimistic();
            }
        },
        __staticClose: {
            value: function __staticClose(scopeManager) {
                // static resolve
                for (var i = 0, iz = this.__left.length; i < iz; ++i) {
                    var ref = this.__left[i];
                    if (!this.__resolve(ref)) {
                        this.__delegateToUpperScope(ref);
                    }
                }
            }
        },
        __dynamicClose: {
            value: function __dynamicClose(scopeManager) {
                // This path is for "global" and "function with eval" environment.
                for (var i = 0, iz = this.__left.length; i < iz; ++i) {
                    // notify all names are through to global
                    var ref = this.__left[i];
                    var current = this;
                    do {
                        current.through.push(ref);
                        current = current.upper;
                    } while (current);
                }
            }
        },
        __close: {
            value: function __close(scopeManager) {
                if (this.__shouldStaticallyClose(scopeManager)) {
                    this.__staticClose();
                } else {
                    this.__dynamicClose();
                }

                this.__left = null;
                return this.upper;
            }
        },
        __resolve: {
            value: function __resolve(ref) {
                var variable, name;
                name = ref.identifier.name;
                if (this.set.has(name)) {
                    variable = this.set.get(name);
                    variable.references.push(ref);
                    variable.stack = variable.stack && ref.from.variableScope === this.variableScope;
                    if (ref.tainted) {
                        variable.tainted = true;
                        this.taints.set(variable.name, true);
                    }
                    ref.resolved = variable;
                    return true;
                }
                return false;
            }
        },
        __delegateToUpperScope: {
            value: function __delegateToUpperScope(ref) {
                if (this.upper) {
                    this.upper.__left.push(ref);
                }
                this.through.push(ref);
            }
        },
        __defineGeneric: {
            value: function __defineGeneric(name, set, variables, node, def) {
                var variable;

                variable = set.get(name);
                if (!variable) {
                    variable = new Variable(name, this);
                    set.set(name, variable);
                    variables.push(variable);
                }

                if (def) {
                    variable.defs.push(def);
                }
                if (node) {
                    variable.identifiers.push(node);
                }
            }
        },
        __define: {
            value: function __define(node, def) {
                if (node && node.type === Syntax.Identifier) {
                    this.__defineGeneric(node.name, this.set, this.variables, node, def);
                }
            }
        },
        __referencing: {
            value: function __referencing(node, assign, writeExpr, maybeImplicitGlobal, partial) {
                // because Array element may be null
                if (!node || node.type !== Syntax.Identifier) {
                    return;
                }

                // Specially handle like `this`.
                if (node.name === "super") {
                    return;
                }

                var ref = new Reference(node, this, assign || Reference.READ, writeExpr, maybeImplicitGlobal, !!partial);
                this.references.push(ref);
                this.__left.push(ref);
            }
        },
        __detectEval: {
            value: function __detectEval() {
                var current;
                current = this;
                this.directCallToEvalScope = true;
                do {
                    current.dynamic = true;
                    current = current.upper;
                } while (current);
            }
        },
        __detectThis: {
            value: function __detectThis() {
                this.thisFound = true;
            }
        },
        __isClosed: {
            value: function __isClosed() {
                return this.__left === null;
            }
        },
        resolve: {

            /**
             * returns resolved {Reference}
             * @method Scope#resolve
             * @param {Esprima.Identifier} ident - identifier to be resolved.
             * @return {Reference}
             */

            value: function resolve(ident) {
                var ref, i, iz;
                assert(this.__isClosed(), "Scope should be closed.");
                assert(ident.type === Syntax.Identifier, "Target should be identifier.");
                for (i = 0, iz = this.references.length; i < iz; ++i) {
                    ref = this.references[i];
                    if (ref.identifier === ident) {
                        return ref;
                    }
                }
                return null;
            }
        },
        isStatic: {

            /**
             * returns this scope is static
             * @method Scope#isStatic
             * @return {boolean}
             */

            value: function isStatic() {
                return !this.dynamic;
            }
        },
        isArgumentsMaterialized: {

            /**
             * returns this scope has materialized arguments
             * @method Scope#isArgumentsMaterialized
             * @return {boolean}
             */

            value: function isArgumentsMaterialized() {
                return true;
            }
        },
        isThisMaterialized: {

            /**
             * returns this scope has materialized `this` reference
             * @method Scope#isThisMaterialized
             * @return {boolean}
             */

            value: function isThisMaterialized() {
                return true;
            }
        },
        isUsedName: {
            value: function isUsedName(name) {
                if (this.set.has(name)) {
                    return true;
                }
                for (var i = 0, iz = this.through.length; i < iz; ++i) {
                    if (this.through[i].identifier.name === name) {
                        return true;
                    }
                }
                return false;
            }
        }
    });

    return Scope;
})();

exports["default"] = Scope;

var GlobalScope = exports.GlobalScope = (function (_Scope) {
    function GlobalScope(scopeManager, block) {
        _classCallCheck(this, GlobalScope);

        _get(Object.getPrototypeOf(GlobalScope.prototype), "constructor", this).call(this, scopeManager, "global", null, block, false);
        this.implicit = {
            set: new Map(),
            variables: [],
            /**
            * List of {@link Reference}s that are left to be resolved (i.e. which
            * need to be linked to the variable they refer to).
            * @member {Reference[]} Scope#implicit#left
            */
            left: []
        };
    }

    _inherits(GlobalScope, _Scope);

    _createClass(GlobalScope, {
        __close: {
            value: function __close(scopeManager) {
                var implicit = [];
                for (var i = 0, iz = this.__left.length; i < iz; ++i) {
                    var ref = this.__left[i];
                    if (ref.__maybeImplicitGlobal && !this.set.has(ref.identifier.name)) {
                        implicit.push(ref.__maybeImplicitGlobal);
                    }
                }

                // create an implicit global variable from assignment expression
                for (var i = 0, iz = implicit.length; i < iz; ++i) {
                    var info = implicit[i];
                    this.__defineImplicit(info.pattern, new Definition(Variable.ImplicitGlobalVariable, info.pattern, info.node, null, null, null));
                }

                this.implicit.left = this.__left;

                return _get(Object.getPrototypeOf(GlobalScope.prototype), "__close", this).call(this, scopeManager);
            }
        },
        __defineImplicit: {
            value: function __defineImplicit(node, def) {
                if (node && node.type === Syntax.Identifier) {
                    this.__defineGeneric(node.name, this.implicit.set, this.implicit.variables, node, def);
                }
            }
        }
    });

    return GlobalScope;
})(Scope);

var ModuleScope = exports.ModuleScope = (function (_Scope2) {
    function ModuleScope(scopeManager, upperScope, block) {
        _classCallCheck(this, ModuleScope);

        _get(Object.getPrototypeOf(ModuleScope.prototype), "constructor", this).call(this, scopeManager, "module", upperScope, block, false);
    }

    _inherits(ModuleScope, _Scope2);

    return ModuleScope;
})(Scope);

var FunctionExpressionNameScope = exports.FunctionExpressionNameScope = (function (_Scope3) {
    function FunctionExpressionNameScope(scopeManager, upperScope, block) {
        _classCallCheck(this, FunctionExpressionNameScope);

        _get(Object.getPrototypeOf(FunctionExpressionNameScope.prototype), "constructor", this).call(this, scopeManager, "function-expression-name", upperScope, block, false);
        this.__define(block.id, new Definition(Variable.FunctionName, block.id, block, null, null, null));
        this.functionExpressionScope = true;
    }

    _inherits(FunctionExpressionNameScope, _Scope3);

    return FunctionExpressionNameScope;
})(Scope);

var CatchScope = exports.CatchScope = (function (_Scope4) {
    function CatchScope(scopeManager, upperScope, block) {
        _classCallCheck(this, CatchScope);

        _get(Object.getPrototypeOf(CatchScope.prototype), "constructor", this).call(this, scopeManager, "catch", upperScope, block, false);
    }

    _inherits(CatchScope, _Scope4);

    return CatchScope;
})(Scope);

var WithScope = exports.WithScope = (function (_Scope5) {
    function WithScope(scopeManager, upperScope, block) {
        _classCallCheck(this, WithScope);

        _get(Object.getPrototypeOf(WithScope.prototype), "constructor", this).call(this, scopeManager, "with", upperScope, block, false);
    }

    _inherits(WithScope, _Scope5);

    _createClass(WithScope, {
        __close: {
            value: function __close(scopeManager) {
                if (this.__shouldStaticallyClose(scopeManager)) {
                    return _get(Object.getPrototypeOf(WithScope.prototype), "__close", this).call(this, scopeManager);
                }

                for (var i = 0, iz = this.__left.length; i < iz; ++i) {
                    var ref = this.__left[i];
                    ref.tainted = true;
                    this.__delegateToUpperScope(ref);
                }
                this.__left = null;

                return this.upper;
            }
        }
    });

    return WithScope;
})(Scope);

var TDZScope = exports.TDZScope = (function (_Scope6) {
    function TDZScope(scopeManager, upperScope, block) {
        _classCallCheck(this, TDZScope);

        _get(Object.getPrototypeOf(TDZScope.prototype), "constructor", this).call(this, scopeManager, "TDZ", upperScope, block, false);
    }

    _inherits(TDZScope, _Scope6);

    return TDZScope;
})(Scope);

var BlockScope = exports.BlockScope = (function (_Scope7) {
    function BlockScope(scopeManager, upperScope, block) {
        _classCallCheck(this, BlockScope);

        _get(Object.getPrototypeOf(BlockScope.prototype), "constructor", this).call(this, scopeManager, "block", upperScope, block, false);
    }

    _inherits(BlockScope, _Scope7);

    return BlockScope;
})(Scope);

var SwitchScope = exports.SwitchScope = (function (_Scope8) {
    function SwitchScope(scopeManager, upperScope, block) {
        _classCallCheck(this, SwitchScope);

        _get(Object.getPrototypeOf(SwitchScope.prototype), "constructor", this).call(this, scopeManager, "switch", upperScope, block, false);
    }

    _inherits(SwitchScope, _Scope8);

    return SwitchScope;
})(Scope);

var FunctionScope = exports.FunctionScope = (function (_Scope9) {
    function FunctionScope(scopeManager, upperScope, block, isMethodDefinition) {
        _classCallCheck(this, FunctionScope);

        _get(Object.getPrototypeOf(FunctionScope.prototype), "constructor", this).call(this, scopeManager, "function", upperScope, block, isMethodDefinition);

        // section 9.2.13, FunctionDeclarationInstantiation.
        // NOTE Arrow functions never have an arguments objects.
        if (this.block.type !== Syntax.ArrowFunctionExpression) {
            this.__defineArguments();
        }
    }

    _inherits(FunctionScope, _Scope9);

    _createClass(FunctionScope, {
        isArgumentsMaterialized: {
            value: function isArgumentsMaterialized() {
                // TODO(Constellation)
                // We can more aggressive on this condition like this.
                //
                // function t() {
                //     // arguments of t is always hidden.
                //     function arguments() {
                //     }
                // }
                if (this.block.type === Syntax.ArrowFunctionExpression) {
                    return false;
                }

                if (!this.isStatic()) {
                    return true;
                }

                var variable = this.set.get("arguments");
                assert(variable, "Always have arguments variable.");
                return variable.tainted || variable.references.length !== 0;
            }
        },
        isThisMaterialized: {
            value: function isThisMaterialized() {
                if (!this.isStatic()) {
                    return true;
                }
                return this.thisFound;
            }
        },
        __defineArguments: {
            value: function __defineArguments() {
                this.__defineGeneric("arguments", this.set, this.variables, null, null);
                this.taints.set("arguments", true);
            }
        }
    });

    return FunctionScope;
})(Scope);

var ForScope = exports.ForScope = (function (_Scope10) {
    function ForScope(scopeManager, upperScope, block) {
        _classCallCheck(this, ForScope);

        _get(Object.getPrototypeOf(ForScope.prototype), "constructor", this).call(this, scopeManager, "for", upperScope, block, false);
    }

    _inherits(ForScope, _Scope10);

    return ForScope;
})(Scope);

var ClassScope = exports.ClassScope = (function (_Scope11) {
    function ClassScope(scopeManager, upperScope, block) {
        _classCallCheck(this, ClassScope);

        _get(Object.getPrototypeOf(ClassScope.prototype), "constructor", this).call(this, scopeManager, "class", upperScope, block, false);
    }

    _inherits(ClassScope, _Scope11);

    return ClassScope;
})(Scope);

/* vim: set sw=4 ts=4 et tw=80 : */
//# sourceMappingURL=data:application/json;base64,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RVAsWUFBWSxFQUFFLEtBQUssRUFBRSxVQUFVLEVBQUUsS0FBSyxFQUFFLEtBQUssRUFBRTtLQUN4RDs7Y0FIUSxRQUFROztXQUFSLFFBQVE7R0FBUyxLQUFLOztJQU10QixVQUFVLFdBQVYsVUFBVTtBQUNSLGFBREYsVUFBVSxDQUNQLFlBQVksRUFBRSxVQUFVLEVBQUUsS0FBSyxFQUFFOzhCQURwQyxVQUFVOztBQUVmLG1DQUZLLFVBQVUsNkNBRVQsWUFBWSxFQUFFLE9BQU8sRUFBRSxVQUFVLEVBQUUsS0FBSyxFQUFFLEtBQUssRUFBRTtLQUMxRDs7Y0FIUSxVQUFVOztXQUFWLFVBQVU7R0FBUyxLQUFLIiwiZmlsZSI6InNjb3BlLmpzIiwic291cmNlc0NvbnRlbnQiOlsiLypcbiAgQ29weXJpZ2h0IChDKSAyMDE1IFl1c3VrZSBTdXp1a2kgPHV0YXRhbmUudGVhQGdtYWlsLmNvbT5cblxuICBSZWRpc3RyaWJ1dGlvbiBhbmQgdXNlIGluIHNvdXJjZSBhbmQgYmluYXJ5IGZvcm1zLCB3aXRoIG9yIHdpdGhvdXRcbiAgbW9kaWZpY2F0aW9uLCBhcmUgcGVybWl0dGVkIHByb3ZpZGVkIHRoYXQgdGhlIGZvbGxvd2luZyBjb25kaXRpb25zIGFyZSBtZXQ6XG5cbiAgICAqIFJlZGlzdHJpYnV0aW9ucyBvZiBzb3VyY2UgY29kZSBtdXN0IHJldGFpbiB0aGUgYWJvdmUgY29weXJpZ2h0XG4gICAgICBub3RpY2UsIHRoaXMgbGlzdCBvZiBjb25kaXRpb25zIGFuZCB0aGUgZm9sbG93aW5nIGRpc2NsYWltZXIuXG4gICAgKiBSZWRpc3RyaWJ1dGlvbnMgaW4gYmluYXJ5IGZvcm0gbXVzdCByZXByb2R1Y2UgdGhlIGFib3ZlIGNvcHlyaWdodFxuICAgICAgbm90aWNlLCB0aGlzIGxpc3Qgb2YgY29uZGl0aW9ucyBhbmQgdGhlIGZvbGxvd2luZyBkaXNjbGFpbWVyIGluIHRoZVxuICAgICAgZG9jdW1lbnRhdGlvbiBhbmQvb3Igb3RoZXIgbWF0ZXJpYWxzIHByb3ZpZGVkIHdpdGggdGhlIGRpc3RyaWJ1dGlvbi5cblxuICBUSElTIFNPRlRXQVJFIElTIFBST1ZJREVEIEJZIFRIRSBDT1BZUklHSFQgSE9MREVSUyBBTkQgQ09OVFJJQlVUT1JTIFwiQVMgSVNcIlxuICBBTkQgQU5ZIEVYUFJFU1MgT1IgSU1QTElFRCBXQVJSQU5USUVTLCBJTkNMVURJTkcsIEJVVCBOT1QgTElNSVRFRCBUTywgVEhFXG4gIElNUExJRUQgV0FSUkFOVElFUyBPRiBNRVJDSEFOVEFCSUxJVFkgQU5EIEZJVE5FU1MgRk9SIEEgUEFSVElDVUxBUiBQVVJQT1NFXG4gIEFSRSBESVNDTEFJTUVELiBJTiBOTyBFVkVOVCBTSEFMTCA8Q09QWVJJR0hUIEhPTERFUj4gQkUgTElBQkxFIEZPUiBBTllcbiAgRElSRUNULCBJTkRJUkVDVCwgSU5DSURFTlRBTCwgU1BFQ0lBTCwgRVhFTVBMQVJZLCBPUiBDT05TRVFVRU5USUFMIERBTUFHRVNcbiAgKElOQ0xVRElORywgQlVUIE5PVCBMSU1JVEVEIFRPLCBQUk9DVVJFTUVOVCBPRiBTVUJTVElUVVRFIEdPT0RTIE9SIFNFUlZJQ0VTO1xuICBMT1NTIE9GIFVTRSwgREFUQSwgT1IgUFJPRklUUzsgT1IgQlVTSU5FU1MgSU5URVJSVVBUSU9OKSBIT1dFVkVSIENBVVNFRCBBTkRcbiAgT04gQU5ZIFRIRU9SWSBPRiBMSUFCSUxJVFksIFdIRVRIRVIgSU4gQ09OVFJBQ1QsIFNUUklDVCBMSUFCSUxJVFksIE9SIFRPUlRcbiAgKElOQ0xVRElORyBORUdMSUdFTkNFIE9SIE9USEVSV0lTRSkgQVJJU0lORyBJTiBBTlkgV0FZIE9VVCBPRiBUSEUgVVNFIE9GXG4gIFRISVMgU09GVFdBUkUsIEVWRU4gSUYgQURWSVNFRCBPRiBUSEUgUE9TU0lCSUxJVFkgT0YgU1VDSCBEQU1BR0UuXG4qL1xuXG5pbXBvcnQgeyBTeW50YXggfSBmcm9tICdlc3RyYXZlcnNlJztcbmltcG9ydCBNYXAgZnJvbSAnZXM2LW1hcCc7XG5cbmltcG9ydCBSZWZlcmVuY2UgZnJvbSAnLi9yZWZlcmVuY2UnO1xuaW1wb3J0IFZhcmlhYmxlIGZyb20gJy4vdmFyaWFibGUnO1xuaW1wb3J0IERlZmluaXRpb24gZnJvbSAnLi9kZWZpbml0aW9uJztcbmltcG9ydCBhc3NlcnQgZnJvbSAnYXNzZXJ0JztcblxuZnVuY3Rpb24gaXNTdHJpY3RTY29wZShzY29wZSwgYmxvY2ssIGlzTWV0aG9kRGVmaW5pdGlvbiwgdXNlRGlyZWN0aXZlKSB7XG4gICAgdmFyIGJvZHksIGksIGl6LCBzdG10LCBleHByO1xuXG4gICAgLy8gV2hlbiB1cHBlciBzY29wZSBpcyBleGlzdHMgYW5kIHN0cmljdCwgaW5uZXIgc2NvcGUgaXMgYWxzbyBzdHJpY3QuXG4gICAgaWYgKHNjb3BlLnVwcGVyICYmIHNjb3BlLnVwcGVyLmlzU3RyaWN0KSB7XG4gICAgICAgIHJldHVybiB0cnVlO1xuICAgIH1cblxuICAgIC8vIEFycm93RnVuY3Rpb25FeHByZXNzaW9uJ3Mgc2NvcGUgaXMgYWx3YXlzIHN0cmljdCBzY29wZS5cbiAgICBpZiAoYmxvY2sudHlwZSA9PT0gU3ludGF4LkFycm93RnVuY3Rpb25FeHByZXNzaW9uKSB7XG4gICAgICAgIHJldHVybiB0cnVlO1xuICAgIH1cblxuICAgIGlmIChpc01ldGhvZERlZmluaXRpb24pIHtcbiAgICAgICAgcmV0dXJuIHRydWU7XG4gICAgfVxuXG4gICAgaWYgKHNjb3BlLnR5cGUgPT09ICdjbGFzcycgfHwgc2NvcGUudHlwZSA9PT0gJ21vZHVsZScpIHtcbiAgICAgICAgcmV0dXJuIHRydWU7XG4gICAgfVxuXG4gICAgaWYgKHNjb3BlLnR5cGUgPT09ICdibG9jaycgfHwgc2NvcGUudHlwZSA9PT0gJ3N3aXRjaCcpIHtcbiAgICAgICAgcmV0dXJuIGZhbHNlO1xuICAgIH1cblxuICAgIGlmIChzY29wZS50eXBlID09PSAnZnVuY3Rpb24nKSB7XG4gICAgICAgIGlmIChibG9jay50eXBlID09PSAnUHJvZ3JhbScpIHtcbiAgICAgICAgICAgIGJvZHkgPSBibG9jaztcbiAgICAgICAgfSBlbHNlIHtcbiAgICAgICAgICAgIGJvZHkgPSBibG9jay5ib2R5O1xuICAgICAgICB9XG4gICAgfSBlbHNlIGlmIChzY29wZS50eXBlID09PSAnZ2xvYmFsJykge1xuICAgICAgICBib2R5ID0gYmxvY2s7XG4gICAgfSBlbHNlIHtcbiAgICAgICAgcmV0dXJuIGZhbHNlO1xuICAgIH1cblxuICAgIC8vIFNlYXJjaCAndXNlIHN0cmljdCcgZGlyZWN0aXZlLlxuICAgIGlmICh1c2VEaXJlY3RpdmUpIHtcbiAgICAgICAgZm9yIChpID0gMCwgaXogPSBib2R5LmJvZHkubGVuZ3RoOyBpIDwgaXo7ICsraSkge1xuICAgICAgICAgICAgc3RtdCA9IGJvZHkuYm9keVtpXTtcbiAgICAgICAgICAgIGlmIChzdG10LnR5cGUgIT09ICdEaXJlY3RpdmVTdGF0ZW1lbnQnKSB7XG4gICAgICAgICAgICAgICAgYnJlYWs7XG4gICAgICAgICAgICB9XG4gICAgICAgICAgICBpZiAoc3RtdC5yYXcgPT09ICdcInVzZSBzdHJpY3RcIicgfHwgc3RtdC5yYXcgPT09ICdcXCd1c2Ugc3RyaWN0XFwnJykge1xuICAgICAgICAgICAgICAgIHJldHVybiB0cnVlO1xuICAgICAgICAgICAgfVxuICAgICAgICB9XG4gICAgfSBlbHNlIHtcbiAgICAgICAgZm9yIChpID0gMCwgaXogPSBib2R5LmJvZHkubGVuZ3RoOyBpIDwgaXo7ICsraSkge1xuICAgICAgICAgICAgc3RtdCA9IGJvZHkuYm9keVtpXTtcbiAgICAgICAgICAgIGlmIChzdG10LnR5cGUgIT09IFN5bnRheC5FeHByZXNzaW9uU3RhdGVtZW50KSB7XG4gICAgICAgICAgICAgICAgYnJlYWs7XG4gICAgICAgICAgICB9XG4gICAgICAgICAgICBleHByID0gc3RtdC5leHByZXNzaW9uO1xuICAgICAgICAgICAgaWYgKGV4cHIudHlwZSAhPT0gU3ludGF4LkxpdGVyYWwgfHwgdHlwZW9mIGV4cHIudmFsdWUgIT09ICdzdHJpbmcnKSB7XG4gICAgICAgICAgICAgICAgYnJlYWs7XG4gICAgICAgICAgICB9XG4gICAgICAgICAgICBpZiAoZXhwci5yYXcgIT0gbnVsbCkge1xuICAgICAgICAgICAgICAgIGlmIChleHByLnJhdyA9PT0gJ1widXNlIHN0cmljdFwiJyB8fCBleHByLnJhdyA9PT0gJ1xcJ3VzZSBzdHJpY3RcXCcnKSB7XG4gICAgICAgICAgICAgICAgICAgIHJldHVybiB0cnVlO1xuICAgICAgICAgICAgICAgIH1cbiAgICAgICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgICAgICAgaWYgKGV4cHIudmFsdWUgPT09ICd1c2Ugc3RyaWN0Jykge1xuICAgICAgICAgICAgICAgICAgICByZXR1cm4gdHJ1ZTtcbiAgICAgICAgICAgICAgICB9XG4gICAgICAgICAgICB9XG4gICAgICAgIH1cbiAgICB9XG4gICAgcmV0dXJuIGZhbHNlO1xufVxuXG5mdW5jdGlvbiByZWdpc3RlclNjb3BlKHNjb3BlTWFuYWdlciwgc2NvcGUpIHtcbiAgICB2YXIgc2NvcGVzO1xuXG4gICAgc2NvcGVNYW5hZ2VyLnNjb3Blcy5wdXNoKHNjb3BlKTtcblxuICAgIHNjb3BlcyA9IHNjb3BlTWFuYWdlci5fX25vZGVUb1Njb3BlLmdldChzY29wZS5ibG9jayk7XG4gICAgaWYgKHNjb3Blcykge1xuICAgICAgICBzY29wZXMucHVzaChzY29wZSk7XG4gICAgfSBlbHNlIHtcbiAgICAgICAgc2NvcGVNYW5hZ2VyLl9fbm9kZVRvU2NvcGUuc2V0KHNjb3BlLmJsb2NrLCBbIHNjb3BlIF0pO1xuICAgIH1cbn1cblxuLyoqXG4gKiBAY2xhc3MgU2NvcGVcbiAqL1xuZXhwb3J0IGRlZmF1bHQgY2xhc3MgU2NvcGUge1xuICAgIGNvbnN0cnVjdG9yKHNjb3BlTWFuYWdlciwgdHlwZSwgdXBwZXJTY29wZSwgYmxvY2ssIGlzTWV0aG9kRGVmaW5pdGlvbikge1xuICAgICAgICAvKipcbiAgICAgICAgICogT25lIG9mICdURFonLCAnbW9kdWxlJywgJ2Jsb2NrJywgJ3N3aXRjaCcsICdmdW5jdGlvbicsICdjYXRjaCcsICd3aXRoJywgJ2Z1bmN0aW9uJywgJ2NsYXNzJywgJ2dsb2JhbCcuXG4gICAgICAgICAqIEBtZW1iZXIge1N0cmluZ30gU2NvcGUjdHlwZVxuICAgICAgICAgKi9cbiAgICAgICAgdGhpcy50eXBlID0gdHlwZTtcbiAgICAgICAgIC8qKlxuICAgICAgICAgKiBUaGUgc2NvcGVkIHtAbGluayBWYXJpYWJsZX1zIG9mIHRoaXMgc2NvcGUsIGFzIDxjb2RlPnsgVmFyaWFibGUubmFtZVxuICAgICAgICAgKiA6IFZhcmlhYmxlIH08L2NvZGU+LlxuICAgICAgICAgKiBAbWVtYmVyIHtNYXB9IFNjb3BlI3NldFxuICAgICAgICAgKi9cbiAgICAgICAgdGhpcy5zZXQgPSBuZXcgTWFwKCk7XG4gICAgICAgIC8qKlxuICAgICAgICAgKiBUaGUgdGFpbnRlZCB2YXJpYWJsZXMgb2YgdGhpcyBzY29wZSwgYXMgPGNvZGU+eyBWYXJpYWJsZS5uYW1lIDpcbiAgICAgICAgICogYm9vbGVhbiB9PC9jb2RlPi5cbiAgICAgICAgICogQG1lbWJlciB7TWFwfSBTY29wZSN0YWludHMgKi9cbiAgICAgICAgdGhpcy50YWludHMgPSBuZXcgTWFwKCk7XG4gICAgICAgIC8qKlxuICAgICAgICAgKiBHZW5lcmFsbHksIHRocm91Z2ggdGhlIGxleGljYWwgc2NvcGluZyBvZiBKUyB5b3UgY2FuIGFsd2F5cyBrbm93XG4gICAgICAgICAqIHdoaWNoIHZhcmlhYmxlIGFuIGlkZW50aWZpZXIgaW4gdGhlIHNvdXJjZSBjb2RlIHJlZmVycyB0by4gVGhlcmUgYXJlXG4gICAgICAgICAqIGEgZmV3IGV4Y2VwdGlvbnMgdG8gdGhpcyBydWxlLiBXaXRoICdnbG9iYWwnIGFuZCAnd2l0aCcgc2NvcGVzIHlvdVxuICAgICAgICAgKiBjYW4gb25seSBkZWNpZGUgYXQgcnVudGltZSB3aGljaCB2YXJpYWJsZSBhIHJlZmVyZW5jZSByZWZlcnMgdG8uXG4gICAgICAgICAqIE1vcmVvdmVyLCBpZiAnZXZhbCgpJyBpcyB1c2VkIGluIGEgc2NvcGUsIGl0IG1pZ2h0IGludHJvZHVjZSBuZXdcbiAgICAgICAgICogYmluZGluZ3MgaW4gdGhpcyBvciBpdHMgcHJhcmVudCBzY29wZXMuXG4gICAgICAgICAqIEFsbCB0aG9zZSBzY29wZXMgYXJlIGNvbnNpZGVyZWQgJ2R5bmFtaWMnLlxuICAgICAgICAgKiBAbWVtYmVyIHtib29sZWFufSBTY29wZSNkeW5hbWljXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLmR5bmFtaWMgPSB0aGlzLnR5cGUgPT09ICdnbG9iYWwnIHx8IHRoaXMudHlwZSA9PT0gJ3dpdGgnO1xuICAgICAgICAvKipcbiAgICAgICAgICogQSByZWZlcmVuY2UgdG8gdGhlIHNjb3BlLWRlZmluaW5nIHN5bnRheCBub2RlLlxuICAgICAgICAgKiBAbWVtYmVyIHtlc3ByaW1hLk5vZGV9IFNjb3BlI2Jsb2NrXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLmJsb2NrID0gYmxvY2s7XG4gICAgICAgICAvKipcbiAgICAgICAgICogVGhlIHtAbGluayBSZWZlcmVuY2V8cmVmZXJlbmNlc30gdGhhdCBhcmUgbm90IHJlc29sdmVkIHdpdGggdGhpcyBzY29wZS5cbiAgICAgICAgICogQG1lbWJlciB7UmVmZXJlbmNlW119IFNjb3BlI3Rocm91Z2hcbiAgICAgICAgICovXG4gICAgICAgIHRoaXMudGhyb3VnaCA9IFtdO1xuICAgICAgICAgLyoqXG4gICAgICAgICAqIFRoZSBzY29wZWQge0BsaW5rIFZhcmlhYmxlfXMgb2YgdGhpcyBzY29wZS4gSW4gdGhlIGNhc2Ugb2YgYVxuICAgICAgICAgKiAnZnVuY3Rpb24nIHNjb3BlIHRoaXMgaW5jbHVkZXMgdGhlIGF1dG9tYXRpYyBhcmd1bWVudCA8ZW0+YXJndW1lbnRzPC9lbT4gYXNcbiAgICAgICAgICogaXRzIGZpcnN0IGVsZW1lbnQsIGFzIHdlbGwgYXMgYWxsIGZ1cnRoZXIgZm9ybWFsIGFyZ3VtZW50cy5cbiAgICAgICAgICogQG1lbWJlciB7VmFyaWFibGVbXX0gU2NvcGUjdmFyaWFibGVzXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLnZhcmlhYmxlcyA9IFtdO1xuICAgICAgICAgLyoqXG4gICAgICAgICAqIEFueSB2YXJpYWJsZSB7QGxpbmsgUmVmZXJlbmNlfHJlZmVyZW5jZX0gZm91bmQgaW4gdGhpcyBzY29wZS4gVGhpc1xuICAgICAgICAgKiBpbmNsdWRlcyBvY2N1cnJlbmNlcyBvZiBsb2NhbCB2YXJpYWJsZXMgYXMgd2VsbCBhcyB2YXJpYWJsZXMgZnJvbVxuICAgICAgICAgKiBwYXJlbnQgc2NvcGVzIChpbmNsdWRpbmcgdGhlIGdsb2JhbCBzY29wZSkuIEZvciBsb2NhbCB2YXJpYWJsZXNcbiAgICAgICAgICogdGhpcyBhbHNvIGluY2x1ZGVzIGRlZmluaW5nIG9jY3VycmVuY2VzIChsaWtlIGluIGEgJ3Zhcicgc3RhdGVtZW50KS5cbiAgICAgICAgICogSW4gYSAnZnVuY3Rpb24nIHNjb3BlIHRoaXMgZG9lcyBub3QgaW5jbHVkZSB0aGUgb2NjdXJyZW5jZXMgb2YgdGhlXG4gICAgICAgICAqIGZvcm1hbCBwYXJhbWV0ZXIgaW4gdGhlIHBhcmFtZXRlciBsaXN0LlxuICAgICAgICAgKiBAbWVtYmVyIHtSZWZlcmVuY2VbXX0gU2NvcGUjcmVmZXJlbmNlc1xuICAgICAgICAgKi9cbiAgICAgICAgdGhpcy5yZWZlcmVuY2VzID0gW107XG5cbiAgICAgICAgIC8qKlxuICAgICAgICAgKiBGb3IgJ2dsb2JhbCcgYW5kICdmdW5jdGlvbicgc2NvcGVzLCB0aGlzIGlzIGEgc2VsZi1yZWZlcmVuY2UuIEZvclxuICAgICAgICAgKiBvdGhlciBzY29wZSB0eXBlcyB0aGlzIGlzIHRoZSA8ZW0+dmFyaWFibGVTY29wZTwvZW0+IHZhbHVlIG9mIHRoZVxuICAgICAgICAgKiBwYXJlbnQgc2NvcGUuXG4gICAgICAgICAqIEBtZW1iZXIge1Njb3BlfSBTY29wZSN2YXJpYWJsZVNjb3BlXG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLnZhcmlhYmxlU2NvcGUgPVxuICAgICAgICAgICAgKHRoaXMudHlwZSA9PT0gJ2dsb2JhbCcgfHwgdGhpcy50eXBlID09PSAnZnVuY3Rpb24nIHx8IHRoaXMudHlwZSA9PT0gJ21vZHVsZScpID8gdGhpcyA6IHVwcGVyU2NvcGUudmFyaWFibGVTY29wZTtcbiAgICAgICAgIC8qKlxuICAgICAgICAgKiBXaGV0aGVyIHRoaXMgc2NvcGUgaXMgY3JlYXRlZCBieSBhIEZ1bmN0aW9uRXhwcmVzc2lvbi5cbiAgICAgICAgICogQG1lbWJlciB7Ym9vbGVhbn0gU2NvcGUjZnVuY3Rpb25FeHByZXNzaW9uU2NvcGVcbiAgICAgICAgICovXG4gICAgICAgIHRoaXMuZnVuY3Rpb25FeHByZXNzaW9uU2NvcGUgPSBmYWxzZTtcbiAgICAgICAgIC8qKlxuICAgICAgICAgKiBXaGV0aGVyIHRoaXMgaXMgYSBzY29wZSB0aGF0IGNvbnRhaW5zIGFuICdldmFsKCknIGludm9jYXRpb24uXG4gICAgICAgICAqIEBtZW1iZXIge2Jvb2xlYW59IFNjb3BlI2RpcmVjdENhbGxUb0V2YWxTY29wZVxuICAgICAgICAgKi9cbiAgICAgICAgdGhpcy5kaXJlY3RDYWxsVG9FdmFsU2NvcGUgPSBmYWxzZTtcbiAgICAgICAgIC8qKlxuICAgICAgICAgKiBAbWVtYmVyIHtib29sZWFufSBTY29wZSN0aGlzRm91bmRcbiAgICAgICAgICovXG4gICAgICAgIHRoaXMudGhpc0ZvdW5kID0gZmFsc2U7XG5cbiAgICAgICAgdGhpcy5fX2xlZnQgPSBbXTtcblxuICAgICAgICAgLyoqXG4gICAgICAgICAqIFJlZmVyZW5jZSB0byB0aGUgcGFyZW50IHtAbGluayBTY29wZXxzY29wZX0uXG4gICAgICAgICAqIEBtZW1iZXIge1Njb3BlfSBTY29wZSN1cHBlclxuICAgICAgICAgKi9cbiAgICAgICAgdGhpcy51cHBlciA9IHVwcGVyU2NvcGU7XG4gICAgICAgICAvKipcbiAgICAgICAgICogV2hldGhlciAndXNlIHN0cmljdCcgaXMgaW4gZWZmZWN0IGluIHRoaXMgc2NvcGUuXG4gICAgICAgICAqIEBtZW1iZXIge2Jvb2xlYW59IFNjb3BlI2lzU3RyaWN0XG4gICAgICAgICAqL1xuICAgICAgICB0aGlzLmlzU3RyaWN0ID0gaXNTdHJpY3RTY29wZSh0aGlzLCBibG9jaywgaXNNZXRob2REZWZpbml0aW9uLCBzY29wZU1hbmFnZXIuX191c2VEaXJlY3RpdmUoKSk7XG5cbiAgICAgICAgIC8qKlxuICAgICAgICAgKiBMaXN0IG9mIG5lc3RlZCB7QGxpbmsgU2NvcGV9cy5cbiAgICAgICAgICogQG1lbWJlciB7U2NvcGVbXX0gU2NvcGUjY2hpbGRTY29wZXNcbiAgICAgICAgICovXG4gICAgICAgIHRoaXMuY2hpbGRTY29wZXMgPSBbXTtcbiAgICAgICAgaWYgKHRoaXMudXBwZXIpIHtcbiAgICAgICAgICAgIHRoaXMudXBwZXIuY2hpbGRTY29wZXMucHVzaCh0aGlzKTtcbiAgICAgICAgfVxuXG4gICAgICAgIHJlZ2lzdGVyU2NvcGUoc2NvcGVNYW5hZ2VyLCB0aGlzKTtcbiAgICB9XG5cbiAgICBfX3Nob3VsZFN0YXRpY2FsbHlDbG9zZShzY29wZU1hbmFnZXIpIHtcbiAgICAgICAgcmV0dXJuICghdGhpcy5keW5hbWljIHx8IHNjb3BlTWFuYWdlci5fX2lzT3B0aW1pc3RpYygpKTtcbiAgICB9XG5cbiAgICBfX3N0YXRpY0Nsb3NlKHNjb3BlTWFuYWdlcikge1xuICAgICAgICAvLyBzdGF0aWMgcmVzb2x2ZVxuICAgICAgICBmb3IgKGxldCBpID0gMCwgaXogPSB0aGlzLl9fbGVmdC5sZW5ndGg7IGkgPCBpejsgKytpKSB7XG4gICAgICAgICAgICBsZXQgcmVmID0gdGhpcy5fX2xlZnRbaV07XG4gICAgICAgICAgICBpZiAoIXRoaXMuX19yZXNvbHZlKHJlZikpIHtcbiAgICAgICAgICAgICAgICB0aGlzLl9fZGVsZWdhdGVUb1VwcGVyU2NvcGUocmVmKTtcbiAgICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgIH1cblxuICAgIF9fZHluYW1pY0Nsb3NlKHNjb3BlTWFuYWdlcikge1xuICAgICAgICAvLyBUaGlzIHBhdGggaXMgZm9yIFwiZ2xvYmFsXCIgYW5kIFwiZnVuY3Rpb24gd2l0aCBldmFsXCIgZW52aXJvbm1lbnQuXG4gICAgICAgIGZvciAobGV0IGkgPSAwLCBpeiA9IHRoaXMuX19sZWZ0Lmxlbmd0aDsgaSA8IGl6OyArK2kpIHtcbiAgICAgICAgICAgIC8vIG5vdGlmeSBhbGwgbmFtZXMgYXJlIHRocm91Z2ggdG8gZ2xvYmFsXG4gICAgICAgICAgICBsZXQgcmVmID0gdGhpcy5fX2xlZnRbaV07XG4gICAgICAgICAgICBsZXQgY3VycmVudCA9IHRoaXM7XG4gICAgICAgICAgICBkbyB7XG4gICAgICAgICAgICAgICAgY3VycmVudC50aHJvdWdoLnB1c2gocmVmKTtcbiAgICAgICAgICAgICAgICBjdXJyZW50ID0gY3VycmVudC51cHBlcjtcbiAgICAgICAgICAgIH0gd2hpbGUgKGN1cnJlbnQpO1xuICAgICAgICB9XG4gICAgfVxuXG4gICAgX19jbG9zZShzY29wZU1hbmFnZXIpIHtcbiAgICAgICAgaWYgKHRoaXMuX19zaG91bGRTdGF0aWNhbGx5Q2xvc2Uoc2NvcGVNYW5hZ2VyKSkge1xuICAgICAgICAgICAgdGhpcy5fX3N0YXRpY0Nsb3NlKCk7XG4gICAgICAgIH0gZWxzZSB7XG4gICAgICAgICAgICB0aGlzLl9fZHluYW1pY0Nsb3NlKCk7XG4gICAgICAgIH1cblxuICAgICAgICB0aGlzLl9fbGVmdCA9IG51bGw7XG4gICAgICAgIHJldHVybiB0aGlzLnVwcGVyO1xuICAgIH1cblxuICAgIF9fcmVzb2x2ZShyZWYpIHtcbiAgICAgICAgdmFyIHZhcmlhYmxlLCBuYW1lO1xuICAgICAgICBuYW1lID0gcmVmLmlkZW50aWZpZXIubmFtZTtcbiAgICAgICAgaWYgKHRoaXMuc2V0LmhhcyhuYW1lKSkge1xuICAgICAgICAgICAgdmFyaWFibGUgPSB0aGlzLnNldC5nZXQobmFtZSk7XG4gICAgICAgICAgICB2YXJpYWJsZS5yZWZlcmVuY2VzLnB1c2gocmVmKTtcbiAgICAgICAgICAgIHZhcmlhYmxlLnN0YWNrID0gdmFyaWFibGUuc3RhY2sgJiYgcmVmLmZyb20udmFyaWFibGVTY29wZSA9PT0gdGhpcy52YXJpYWJsZVNjb3BlO1xuICAgICAgICAgICAgaWYgKHJlZi50YWludGVkKSB7XG4gICAgICAgICAgICAgICAgdmFyaWFibGUudGFpbnRlZCA9IHRydWU7XG4gICAgICAgICAgICAgICAgdGhpcy50YWludHMuc2V0KHZhcmlhYmxlLm5hbWUsIHRydWUpO1xuICAgICAgICAgICAgfVxuICAgICAgICAgICAgcmVmLnJlc29sdmVkID0gdmFyaWFibGU7XG4gICAgICAgICAgICByZXR1cm4gdHJ1ZTtcbiAgICAgICAgfVxuICAgICAgICByZXR1cm4gZmFsc2U7XG4gICAgfVxuXG4gICAgX19kZWxlZ2F0ZVRvVXBwZXJTY29wZShyZWYpIHtcbiAgICAgICAgaWYgKHRoaXMudXBwZXIpIHtcbiAgICAgICAgICAgIHRoaXMudXBwZXIuX19sZWZ0LnB1c2gocmVmKTtcbiAgICAgICAgfVxuICAgICAgICB0aGlzLnRocm91Z2gucHVzaChyZWYpO1xuICAgIH1cblxuICAgIF9fZGVmaW5lR2VuZXJpYyhuYW1lLCBzZXQsIHZhcmlhYmxlcywgbm9kZSwgZGVmKSB7XG4gICAgICAgIHZhciB2YXJpYWJsZTtcblxuICAgICAgICB2YXJpYWJsZSA9IHNldC5nZXQobmFtZSk7XG4gICAgICAgIGlmICghdmFyaWFibGUpIHtcbiAgICAgICAgICAgIHZhcmlhYmxlID0gbmV3IFZhcmlhYmxlKG5hbWUsIHRoaXMpO1xuICAgICAgICAgICAgc2V0LnNldChuYW1lLCB2YXJpYWJsZSk7XG4gICAgICAgICAgICB2YXJpYWJsZXMucHVzaCh2YXJpYWJsZSk7XG4gICAgICAgIH1cblxuICAgICAgICBpZiAoZGVmKSB7XG4gICAgICAgICAgICB2YXJpYWJsZS5kZWZzLnB1c2goZGVmKTtcbiAgICAgICAgfVxuICAgICAgICBpZiAobm9kZSkge1xuICAgICAgICAgICAgdmFyaWFibGUuaWRlbnRpZmllcnMucHVzaChub2RlKTtcbiAgICAgICAgfVxuICAgIH1cblxuICAgIF9fZGVmaW5lKG5vZGUsIGRlZikge1xuICAgICAgICBpZiAobm9kZSAmJiBub2RlLnR5cGUgPT09IFN5bnRheC5JZGVudGlmaWVyKSB7XG4gICAgICAgICAgICB0aGlzLl9fZGVmaW5lR2VuZXJpYyhcbiAgICAgICAgICAgICAgICAgICAgbm9kZS5uYW1lLFxuICAgICAgICAgICAgICAgICAgICB0aGlzLnNldCxcbiAgICAgICAgICAgICAgICAgICAgdGhpcy52YXJpYWJsZXMsXG4gICAgICAgICAgICAgICAgICAgIG5vZGUsXG4gICAgICAgICAgICAgICAgICAgIGRlZik7XG4gICAgICAgIH1cbiAgICB9XG5cbiAgICBfX3JlZmVyZW5jaW5nKG5vZGUsIGFzc2lnbiwgd3JpdGVFeHByLCBtYXliZUltcGxpY2l0R2xvYmFsLCBwYXJ0aWFsKSB7XG4gICAgICAgIC8vIGJlY2F1c2UgQXJyYXkgZWxlbWVudCBtYXkgYmUgbnVsbFxuICAgICAgICBpZiAoIW5vZGUgfHwgbm9kZS50eXBlICE9PSBTeW50YXguSWRlbnRpZmllcikge1xuICAgICAgICAgICAgcmV0dXJuO1xuICAgICAgICB9XG5cbiAgICAgICAgLy8gU3BlY2lhbGx5IGhhbmRsZSBsaWtlIGB0aGlzYC5cbiAgICAgICAgaWYgKG5vZGUubmFtZSA9PT0gJ3N1cGVyJykge1xuICAgICAgICAgICAgcmV0dXJuO1xuICAgICAgICB9XG5cbiAgICAgICAgbGV0IHJlZiA9IG5ldyBSZWZlcmVuY2Uobm9kZSwgdGhpcywgYXNzaWduIHx8IFJlZmVyZW5jZS5SRUFELCB3cml0ZUV4cHIsIG1heWJlSW1wbGljaXRHbG9iYWwsICEhcGFydGlhbCk7XG4gICAgICAgIHRoaXMucmVmZXJlbmNlcy5wdXNoKHJlZik7XG4gICAgICAgIHRoaXMuX19sZWZ0LnB1c2gocmVmKTtcbiAgICB9XG5cbiAgICBfX2RldGVjdEV2YWwoKSB7XG4gICAgICAgIHZhciBjdXJyZW50O1xuICAgICAgICBjdXJyZW50ID0gdGhpcztcbiAgICAgICAgdGhpcy5kaXJlY3RDYWxsVG9FdmFsU2NvcGUgPSB0cnVlO1xuICAgICAgICBkbyB7XG4gICAgICAgICAgICBjdXJyZW50LmR5bmFtaWMgPSB0cnVlO1xuICAgICAgICAgICAgY3VycmVudCA9IGN1cnJlbnQudXBwZXI7XG4gICAgICAgIH0gd2hpbGUgKGN1cnJlbnQpO1xuICAgIH1cblxuICAgIF9fZGV0ZWN0VGhpcygpIHtcbiAgICAgICAgdGhpcy50aGlzRm91bmQgPSB0cnVlO1xuICAgIH1cblxuICAgIF9faXNDbG9zZWQoKSB7XG4gICAgICAgIHJldHVybiB0aGlzLl9fbGVmdCA9PT0gbnVsbDtcbiAgICB9XG5cbiAgICAvKipcbiAgICAgKiByZXR1cm5zIHJlc29sdmVkIHtSZWZlcmVuY2V9XG4gICAgICogQG1ldGhvZCBTY29wZSNyZXNvbHZlXG4gICAgICogQHBhcmFtIHtFc3ByaW1hLklkZW50aWZpZXJ9IGlkZW50IC0gaWRlbnRpZmllciB0byBiZSByZXNvbHZlZC5cbiAgICAgKiBAcmV0dXJuIHtSZWZlcmVuY2V9XG4gICAgICovXG4gICAgcmVzb2x2ZShpZGVudCkge1xuICAgICAgICB2YXIgcmVmLCBpLCBpejtcbiAgICAgICAgYXNzZXJ0KHRoaXMuX19pc0Nsb3NlZCgpLCAnU2NvcGUgc2hvdWxkIGJlIGNsb3NlZC4nKTtcbiAgICAgICAgYXNzZXJ0KGlkZW50LnR5cGUgPT09IFN5bnRheC5JZGVudGlmaWVyLCAnVGFyZ2V0IHNob3VsZCBiZSBpZGVudGlmaWVyLicpO1xuICAgICAgICBmb3IgKGkgPSAwLCBpeiA9IHRoaXMucmVmZXJlbmNlcy5sZW5ndGg7IGkgPCBpejsgKytpKSB7XG4gICAgICAgICAgICByZWYgPSB0aGlzLnJlZmVyZW5jZXNbaV07XG4gICAgICAgICAgICBpZiAocmVmLmlkZW50aWZpZXIgPT09IGlkZW50KSB7XG4gICAgICAgICAgICAgICAgcmV0dXJuIHJlZjtcbiAgICAgICAgICAgIH1cbiAgICAgICAgfVxuICAgICAgICByZXR1cm4gbnVsbDtcbiAgICB9XG5cbiAgICAvKipcbiAgICAgKiByZXR1cm5zIHRoaXMgc2NvcGUgaXMgc3RhdGljXG4gICAgICogQG1ldGhvZCBTY29wZSNpc1N0YXRpY1xuICAgICAqIEByZXR1cm4ge2Jvb2xlYW59XG4gICAgICovXG4gICAgaXNTdGF0aWMoKSB7XG4gICAgICAgIHJldHVybiAhdGhpcy5keW5hbWljO1xuICAgIH1cblxuICAgIC8qKlxuICAgICAqIHJldHVybnMgdGhpcyBzY29wZSBoYXMgbWF0ZXJpYWxpemVkIGFyZ3VtZW50c1xuICAgICAqIEBtZXRob2QgU2NvcGUjaXNBcmd1bWVudHNNYXRlcmlhbGl6ZWRcbiAgICAgKiBAcmV0dXJuIHtib29sZWFufVxuICAgICAqL1xuICAgIGlzQXJndW1lbnRzTWF0ZXJpYWxpemVkKCkge1xuICAgICAgICByZXR1cm4gdHJ1ZTtcbiAgICB9XG5cbiAgICAvKipcbiAgICAgKiByZXR1cm5zIHRoaXMgc2NvcGUgaGFzIG1hdGVyaWFsaXplZCBgdGhpc2AgcmVmZXJlbmNlXG4gICAgICogQG1ldGhvZCBTY29wZSNpc1RoaXNNYXRlcmlhbGl6ZWRcbiAgICAgKiBAcmV0dXJuIHtib29sZWFufVxuICAgICAqL1xuICAgIGlzVGhpc01hdGVyaWFsaXplZCgpIHtcbiAgICAgICAgcmV0dXJuIHRydWU7XG4gICAgfVxuXG4gICAgaXNVc2VkTmFtZShuYW1lKSB7XG4gICAgICAgIGlmICh0aGlzLnNldC5oYXMobmFtZSkpIHtcbiAgICAgICAgICAgIHJldHVybiB0cnVlO1xuICAgICAgICB9XG4gICAgICAgIGZvciAodmFyIGkgPSAwLCBpeiA9IHRoaXMudGhyb3VnaC5sZW5ndGg7IGkgPCBpejsgKytpKSB7XG4gICAgICAgICAgICBpZiAodGhpcy50aHJvdWdoW2ldLmlkZW50aWZpZXIubmFtZSA9PT0gbmFtZSkge1xuICAgICAgICAgICAgICAgIHJldHVybiB0cnVlO1xuICAgICAgICAgICAgfVxuICAgICAgICB9XG4gICAgICAgIHJldHVybiBmYWxzZTtcbiAgICB9XG59XG5cbmV4cG9ydCBjbGFzcyBHbG9iYWxTY29wZSBleHRlbmRzIFNjb3BlIHtcbiAgICBjb25zdHJ1Y3RvcihzY29wZU1hbmFnZXIsIGJsb2NrKSB7XG4gICAgICAgIHN1cGVyKHNjb3BlTWFuYWdlciwgJ2dsb2JhbCcsIG51bGwsIGJsb2NrLCBmYWxzZSk7XG4gICAgICAgIHRoaXMuaW1wbGljaXQgPSB7XG4gICAgICAgICAgICBzZXQ6IG5ldyBNYXAoKSxcbiAgICAgICAgICAgIHZhcmlhYmxlczogW10sXG4gICAgICAgICAgICAvKipcbiAgICAgICAgICAgICogTGlzdCBvZiB7QGxpbmsgUmVmZXJlbmNlfXMgdGhhdCBhcmUgbGVmdCB0byBiZSByZXNvbHZlZCAoaS5lLiB3aGljaFxuICAgICAgICAgICAgKiBuZWVkIHRvIGJlIGxpbmtlZCB0byB0aGUgdmFyaWFibGUgdGhleSByZWZlciB0bykuXG4gICAgICAgICAgICAqIEBtZW1iZXIge1JlZmVyZW5jZVtdfSBTY29wZSNpbXBsaWNpdCNsZWZ0XG4gICAgICAgICAgICAqL1xuICAgICAgICAgICAgbGVmdDogW11cbiAgICAgICAgfTtcbiAgICB9XG5cbiAgICBfX2Nsb3NlKHNjb3BlTWFuYWdlcikge1xuICAgICAgICBsZXQgaW1wbGljaXQgPSBbXTtcbiAgICAgICAgZm9yIChsZXQgaSA9IDAsIGl6ID0gdGhpcy5fX2xlZnQubGVuZ3RoOyBpIDwgaXo7ICsraSkge1xuICAgICAgICAgICAgbGV0IHJlZiA9IHRoaXMuX19sZWZ0W2ldO1xuICAgICAgICAgICAgaWYgKHJlZi5fX21heWJlSW1wbGljaXRHbG9iYWwgJiYgIXRoaXMuc2V0LmhhcyhyZWYuaWRlbnRpZmllci5uYW1lKSkge1xuICAgICAgICAgICAgICAgIGltcGxpY2l0LnB1c2gocmVmLl9fbWF5YmVJbXBsaWNpdEdsb2JhbCk7XG4gICAgICAgICAgICB9XG4gICAgICAgIH1cblxuICAgICAgICAvLyBjcmVhdGUgYW4gaW1wbGljaXQgZ2xvYmFsIHZhcmlhYmxlIGZyb20gYXNzaWdubWVudCBleHByZXNzaW9uXG4gICAgICAgIGZvciAobGV0IGkgPSAwLCBpeiA9IGltcGxpY2l0Lmxlbmd0aDsgaSA8IGl6OyArK2kpIHtcbiAgICAgICAgICAgIGxldCBpbmZvID0gaW1wbGljaXRbaV07XG4gICAgICAgICAgICB0aGlzLl9fZGVmaW5lSW1wbGljaXQoaW5mby5wYXR0ZXJuLFxuICAgICAgICAgICAgICAgICAgICBuZXcgRGVmaW5pdGlvbihcbiAgICAgICAgICAgICAgICAgICAgICAgIFZhcmlhYmxlLkltcGxpY2l0R2xvYmFsVmFyaWFibGUsXG4gICAgICAgICAgICAgICAgICAgICAgICBpbmZvLnBhdHRlcm4sXG4gICAgICAgICAgICAgICAgICAgICAgICBpbmZvLm5vZGUsXG4gICAgICAgICAgICAgICAgICAgICAgICBudWxsLFxuICAgICAgICAgICAgICAgICAgICAgICAgbnVsbCxcbiAgICAgICAgICAgICAgICAgICAgICAgIG51bGxcbiAgICAgICAgICAgICAgICAgICAgKSk7XG5cbiAgICAgICAgfVxuXG4gICAgICAgIHRoaXMuaW1wbGljaXQubGVmdCA9IHRoaXMuX19sZWZ0O1xuXG4gICAgICAgIHJldHVybiBzdXBlci5fX2Nsb3NlKHNjb3BlTWFuYWdlcik7XG4gICAgfVxuXG4gICAgX19kZWZpbmVJbXBsaWNpdChub2RlLCBkZWYpIHtcbiAgICAgICAgaWYgKG5vZGUgJiYgbm9kZS50eXBlID09PSBTeW50YXguSWRlbnRpZmllcikge1xuICAgICAgICAgICAgdGhpcy5fX2RlZmluZUdlbmVyaWMoXG4gICAgICAgICAgICAgICAgICAgIG5vZGUubmFtZSxcbiAgICAgICAgICAgICAgICAgICAgdGhpcy5pbXBsaWNpdC5zZXQsXG4gICAgICAgICAgICAgICAgICAgIHRoaXMuaW1wbGljaXQudmFyaWFibGVzLFxuICAgICAgICAgICAgICAgICAgICBub2RlLFxuICAgICAgICAgICAgICAgICAgICBkZWYpO1xuICAgICAgICB9XG4gICAgfVxufVxuXG5leHBvcnQgY2xhc3MgTW9kdWxlU2NvcGUgZXh0ZW5kcyBTY29wZSB7XG4gICAgY29uc3RydWN0b3Ioc2NvcGVNYW5hZ2VyLCB1cHBlclNjb3BlLCBibG9jaykge1xuICAgICAgICBzdXBlcihzY29wZU1hbmFnZXIsICdtb2R1bGUnLCB1cHBlclNjb3BlLCBibG9jaywgZmFsc2UpO1xuICAgIH1cbn1cblxuZXhwb3J0IGNsYXNzIEZ1bmN0aW9uRXhwcmVzc2lvbk5hbWVTY29wZSBleHRlbmRzIFNjb3BlIHtcbiAgICBjb25zdHJ1Y3RvcihzY29wZU1hbmFnZXIsIHVwcGVyU2NvcGUsIGJsb2NrKSB7XG4gICAgICAgIHN1cGVyKHNjb3BlTWFuYWdlciwgJ2Z1bmN0aW9uLWV4cHJlc3Npb24tbmFtZScsIHVwcGVyU2NvcGUsIGJsb2NrLCBmYWxzZSk7XG4gICAgICAgIHRoaXMuX19kZWZpbmUoYmxvY2suaWQsXG4gICAgICAgICAgICAgICAgbmV3IERlZmluaXRpb24oXG4gICAgICAgICAgICAgICAgICAgIFZhcmlhYmxlLkZ1bmN0aW9uTmFtZSxcbiAgICAgICAgICAgICAgICAgICAgYmxvY2suaWQsXG4gICAgICAgICAgICAgICAgICAgIGJsb2NrLFxuICAgICAgICAgICAgICAgICAgICBudWxsLFxuICAgICAgICAgICAgICAgICAgICBudWxsLFxuICAgICAgICAgICAgICAgICAgICBudWxsXG4gICAgICAgICAgICAgICAgKSk7XG4gICAgICAgIHRoaXMuZnVuY3Rpb25FeHByZXNzaW9uU2NvcGUgPSB0cnVlO1xuICAgIH1cbn1cblxuZXhwb3J0IGNsYXNzIENhdGNoU2NvcGUgZXh0ZW5kcyBTY29wZSB7XG4gICAgY29uc3RydWN0b3Ioc2NvcGVNYW5hZ2VyLCB1cHBlclNjb3BlLCBibG9jaykge1xuICAgICAgICBzdXBlcihzY29wZU1hbmFnZXIsICdjYXRjaCcsIHVwcGVyU2NvcGUsIGJsb2NrLCBmYWxzZSk7XG4gICAgfVxufVxuXG5leHBvcnQgY2xhc3MgV2l0aFNjb3BlIGV4dGVuZHMgU2NvcGUge1xuICAgIGNvbnN0cnVjdG9yKHNjb3BlTWFuYWdlciwgdXBwZXJTY29wZSwgYmxvY2spIHtcbiAgICAgICAgc3VwZXIoc2NvcGVNYW5hZ2VyLCAnd2l0aCcsIHVwcGVyU2NvcGUsIGJsb2NrLCBmYWxzZSk7XG4gICAgfVxuXG4gICAgX19jbG9zZShzY29wZU1hbmFnZXIpIHtcbiAgICAgICAgaWYgKHRoaXMuX19zaG91bGRTdGF0aWNhbGx5Q2xvc2Uoc2NvcGVNYW5hZ2VyKSkge1xuICAgICAgICAgICAgcmV0dXJuIHN1cGVyLl9fY2xvc2Uoc2NvcGVNYW5hZ2VyKTtcbiAgICAgICAgfVxuXG4gICAgICAgIGZvciAobGV0IGkgPSAwLCBpeiA9IHRoaXMuX19sZWZ0Lmxlbmd0aDsgaSA8IGl6OyArK2kpIHtcbiAgICAgICAgICAgIGxldCByZWYgPSB0aGlzLl9fbGVmdFtpXTtcbiAgICAgICAgICAgIHJlZi50YWludGVkID0gdHJ1ZTtcbiAgICAgICAgICAgIHRoaXMuX19kZWxlZ2F0ZVRvVXBwZXJTY29wZShyZWYpO1xuICAgICAgICB9XG4gICAgICAgIHRoaXMuX19sZWZ0ID0gbnVsbDtcblxuICAgICAgICByZXR1cm4gdGhpcy51cHBlcjtcbiAgICB9XG59XG5cbmV4cG9ydCBjbGFzcyBURFpTY29wZSBleHRlbmRzIFNjb3BlIHtcbiAgICBjb25zdHJ1Y3RvcihzY29wZU1hbmFnZXIsIHVwcGVyU2NvcGUsIGJsb2NrKSB7XG4gICAgICAgIHN1cGVyKHNjb3BlTWFuYWdlciwgJ1REWicsIHVwcGVyU2NvcGUsIGJsb2NrLCBmYWxzZSk7XG4gICAgfVxufVxuXG5leHBvcnQgY2xhc3MgQmxvY2tTY29wZSBleHRlbmRzIFNjb3BlIHtcbiAgICBjb25zdHJ1Y3RvcihzY29wZU1hbmFnZXIsIHVwcGVyU2NvcGUsIGJsb2NrKSB7XG4gICAgICAgIHN1cGVyKHNjb3BlTWFuYWdlciwgJ2Jsb2NrJywgdXBwZXJTY29wZSwgYmxvY2ssIGZhbHNlKTtcbiAgICB9XG59XG5cbmV4cG9ydCBjbGFzcyBTd2l0Y2hTY29wZSBleHRlbmRzIFNjb3BlIHtcbiAgICBjb25zdHJ1Y3RvcihzY29wZU1hbmFnZXIsIHVwcGVyU2NvcGUsIGJsb2NrKSB7XG4gICAgICAgIHN1cGVyKHNjb3BlTWFuYWdlciwgJ3N3aXRjaCcsIHVwcGVyU2NvcGUsIGJsb2NrLCBmYWxzZSk7XG4gICAgfVxufVxuXG5leHBvcnQgY2xhc3MgRnVuY3Rpb25TY29wZSBleHRlbmRzIFNjb3BlIHtcbiAgICBjb25zdHJ1Y3RvcihzY29wZU1hbmFnZXIsIHVwcGVyU2NvcGUsIGJsb2NrLCBpc01ldGhvZERlZmluaXRpb24pIHtcbiAgICAgICAgc3VwZXIoc2NvcGVNYW5hZ2VyLCAnZnVuY3Rpb24nLCB1cHBlclNjb3BlLCBibG9jaywgaXNNZXRob2REZWZpbml0aW9uKTtcblxuICAgICAgICAvLyBzZWN0aW9uIDkuMi4xMywgRnVuY3Rpb25EZWNsYXJhdGlvbkluc3RhbnRpYXRpb24uXG4gICAgICAgIC8vIE5PVEUgQXJyb3cgZnVuY3Rpb25zIG5ldmVyIGhhdmUgYW4gYXJndW1lbnRzIG9iamVjdHMuXG4gICAgICAgIGlmICh0aGlzLmJsb2NrLnR5cGUgIT09IFN5bnRheC5BcnJvd0Z1bmN0aW9uRXhwcmVzc2lvbikge1xuICAgICAgICAgICAgdGhpcy5fX2RlZmluZUFyZ3VtZW50cygpO1xuICAgICAgICB9XG4gICAgfVxuXG4gICAgaXNBcmd1bWVudHNNYXRlcmlhbGl6ZWQoKSB7XG4gICAgICAgIC8vIFRPRE8oQ29uc3RlbGxhdGlvbilcbiAgICAgICAgLy8gV2UgY2FuIG1vcmUgYWdncmVzc2l2ZSBvbiB0aGlzIGNvbmRpdGlvbiBsaWtlIHRoaXMuXG4gICAgICAgIC8vXG4gICAgICAgIC8vIGZ1bmN0aW9uIHQoKSB7XG4gICAgICAgIC8vICAgICAvLyBhcmd1bWVudHMgb2YgdCBpcyBhbHdheXMgaGlkZGVuLlxuICAgICAgICAvLyAgICAgZnVuY3Rpb24gYXJndW1lbnRzKCkge1xuICAgICAgICAvLyAgICAgfVxuICAgICAgICAvLyB9XG4gICAgICAgIGlmICh0aGlzLmJsb2NrLnR5cGUgPT09IFN5bnRheC5BcnJvd0Z1bmN0aW9uRXhwcmVzc2lvbikge1xuICAgICAgICAgICAgcmV0dXJuIGZhbHNlO1xuICAgICAgICB9XG5cbiAgICAgICAgaWYgKCF0aGlzLmlzU3RhdGljKCkpIHtcbiAgICAgICAgICAgIHJldHVybiB0cnVlO1xuICAgICAgICB9XG5cbiAgICAgICAgbGV0IHZhcmlhYmxlID0gdGhpcy5zZXQuZ2V0KCdhcmd1bWVudHMnKTtcbiAgICAgICAgYXNzZXJ0KHZhcmlhYmxlLCAnQWx3YXlzIGhhdmUgYXJndW1lbnRzIHZhcmlhYmxlLicpO1xuICAgICAgICByZXR1cm4gdmFyaWFibGUudGFpbnRlZCB8fCB2YXJpYWJsZS5yZWZlcmVuY2VzLmxlbmd0aCAgIT09IDA7XG4gICAgfVxuXG4gICAgaXNUaGlzTWF0ZXJpYWxpemVkKCkge1xuICAgICAgICBpZiAoIXRoaXMuaXNTdGF0aWMoKSkge1xuICAgICAgICAgICAgcmV0dXJuIHRydWU7XG4gICAgICAgIH1cbiAgICAgICAgcmV0dXJuIHRoaXMudGhpc0ZvdW5kO1xuICAgIH1cblxuICAgIF9fZGVmaW5lQXJndW1lbnRzKCkge1xuICAgICAgICB0aGlzLl9fZGVmaW5lR2VuZXJpYyhcbiAgICAgICAgICAgICAgICAnYXJndW1lbnRzJyxcbiAgICAgICAgICAgICAgICB0aGlzLnNldCxcbiAgICAgICAgICAgICAgICB0aGlzLnZhcmlhYmxlcyxcbiAgICAgICAgICAgICAgICBudWxsLFxuICAgICAgICAgICAgICAgIG51bGwpO1xuICAgICAgICB0aGlzLnRhaW50cy5zZXQoJ2FyZ3VtZW50cycsIHRydWUpO1xuICAgIH1cbn1cblxuZXhwb3J0IGNsYXNzIEZvclNjb3BlIGV4dGVuZHMgU2NvcGUge1xuICAgIGNvbnN0cnVjdG9yKHNjb3BlTWFuYWdlciwgdXBwZXJTY29wZSwgYmxvY2spIHtcbiAgICAgICAgc3VwZXIoc2NvcGVNYW5hZ2VyLCAnZm9yJywgdXBwZXJTY29wZSwgYmxvY2ssIGZhbHNlKTtcbiAgICB9XG59XG5cbmV4cG9ydCBjbGFzcyBDbGFzc1Njb3BlIGV4dGVuZHMgU2NvcGUge1xuICAgIGNvbnN0cnVjdG9yKHNjb3BlTWFuYWdlciwgdXBwZXJTY29wZSwgYmxvY2spIHtcbiAgICAgICAgc3VwZXIoc2NvcGVNYW5hZ2VyLCAnY2xhc3MnLCB1cHBlclNjb3BlLCBibG9jaywgZmFsc2UpO1xuICAgIH1cbn1cblxuLyogdmltOiBzZXQgc3c9NCB0cz00IGV0IHR3PTgwIDogKi9cbiJdLCJzb3VyY2VSb290IjoiL3NvdXJjZS8ifQ==