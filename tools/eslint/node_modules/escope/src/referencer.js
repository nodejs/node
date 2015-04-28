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
import { Syntax } from 'estraverse';
import esrecurse from 'esrecurse';
import Reference from './reference';
import Variable from './variable';
import { ParameterDefinition, Definition } from './definition';
import assert from 'assert';

class PatternVisitor extends esrecurse.Visitor {
    constructor(rootPattern, referencer, callback) {
        super();
        this.referencer = referencer;
        this.callback = callback;
    }

    perform(pattern) {
        if (pattern.type === Syntax.Identifier) {
            this.callback(pattern, true);
            return;
        }
        this.visit(pattern);
    }

    Identifier(pattern) {
        this.callback(pattern, false);
    }

    ObjectPattern(pattern) {
        var i, iz, property;
        for (i = 0, iz = pattern.properties.length; i < iz; ++i) {
            property = pattern.properties[i];
            if (property.shorthand) {
                this.visit(property.key);
                continue;
            }
            this.visit(property.value);
        }
    }

    ArrayPattern(pattern) {
        var i, iz, element;
        for (i = 0, iz = pattern.elements.length; i < iz; ++i) {
            element = pattern.elements[i];
            if (element) {
                this.visit(element);
            }
        }
    }

    AssignmentPattern(pattern) {
        this.visit(pattern.left);
        // FIXME: Condier TDZ scope.
        this.referencer.visit(pattern.right);
    }
}

function traverseIdentifierInPattern(rootPattern, referencer, callback) {
    var visitor = new PatternVisitor(rootPattern, referencer, callback);
    visitor.perform(rootPattern);
}

function isPattern(node) {
    var nodeType = node.type;
    return nodeType === Syntax.Identifier || nodeType === Syntax.ObjectPattern || nodeType === Syntax.ArrayPattern || nodeType === Syntax.SpreadElement || nodeType === Syntax.RestElement || nodeType === Syntax.AssignmentPattern;
}

// Importing ImportDeclaration.
// http://people.mozilla.org/~jorendorff/es6-draft.html#sec-moduledeclarationinstantiation
// https://github.com/estree/estree/blob/master/es6.md#importdeclaration
// FIXME: Now, we don't create module environment, because the context is
// implementation dependent.

class Importer extends esrecurse.Visitor {
    constructor(declaration, referencer) {
        super();
        this.declaration = declaration;
        this.referencer = referencer;
    }

    visitImport(id, specifier) {
        this.referencer.visitPattern(id, (pattern) => {
            this.referencer.currentScope().__define(pattern,
                new Definition(
                    Variable.ImportBinding,
                    pattern,
                    specifier,
                    this.declaration,
                    null,
                    null
                    ));
        });
    }

    ImportNamespaceSpecifier(node) {
        let local = (node.local || node.id);
        if (local) {
            this.visitImport(local, node);
        }
    }

    ImportDefaultSpecifier(node) {
        let local = (node.local || node.id);
        this.visitImport(local, node);
    }

    ImportSpecifier(node) {
        let local = (node.local || node.id);
        if (node.name) {
            this.visitImport(node.name, node);
        } else {
            this.visitImport(local, node);
        }
    }
}

// Referencing variables and creating bindings.
export default class Referencer extends esrecurse.Visitor {
    constructor(scopeManager) {
        super();
        this.scopeManager = scopeManager;
        this.parent = null;
        this.isInnerMethodDefinition = false;
    }

    currentScope() {
        return this.scopeManager.__currentScope;
    }

    close(node) {
        while (this.currentScope() && node === this.currentScope().block) {
            this.scopeManager.__currentScope = this.currentScope().__close(this.scopeManager);
        }
    }

    pushInnerMethodDefinition(isInnerMethodDefinition) {
        var previous = this.isInnerMethodDefinition;
        this.isInnerMethodDefinition = isInnerMethodDefinition;
        return previous;
    }

    popInnerMethodDefinition(isInnerMethodDefinition) {
        this.isInnerMethodDefinition = isInnerMethodDefinition;
    }

    materializeTDZScope(node, iterationNode) {
        // http://people.mozilla.org/~jorendorff/es6-draft.html#sec-runtime-semantics-forin-div-ofexpressionevaluation-abstract-operation
        // TDZ scope hides the declaration's names.
        this.scopeManager.__nestTDZScope(node, iterationNode);
        this.visitVariableDeclaration(this.currentScope(), Variable.TDZ, iterationNode.left, 0);
    }

    materializeIterationScope(node) {
        // Generate iteration scope for upper ForIn/ForOf Statements.
        var letOrConstDecl;
        this.scopeManager.__nestForScope(node);
        letOrConstDecl = node.left;
        this.visitVariableDeclaration(this.currentScope(), Variable.Variable, letOrConstDecl, 0);
        this.visitPattern(letOrConstDecl.declarations[0].id, (pattern) => {
            this.currentScope().__referencing(pattern, Reference.WRITE, node.right, null, true);
        });
    }

    visitPattern(node, callback) {
        traverseIdentifierInPattern(node, this, callback);
    }

    visitFunction(node) {
        var i, iz;
        // FunctionDeclaration name is defined in upper scope
        // NOTE: Not referring variableScope. It is intended.
        // Since
        //  in ES5, FunctionDeclaration should be in FunctionBody.
        //  in ES6, FunctionDeclaration should be block scoped.
        if (node.type === Syntax.FunctionDeclaration) {
            // id is defined in upper scope
            this.currentScope().__define(node.id,
                    new Definition(
                        Variable.FunctionName,
                        node.id,
                        node,
                        null,
                        null,
                        null
                    ));
        }

        // FunctionExpression with name creates its special scope;
        // FunctionExpressionNameScope.
        if (node.type === Syntax.FunctionExpression && node.id) {
            this.scopeManager.__nestFunctionExpressionNameScope(node);
        }

        // Consider this function is in the MethodDefinition.
        this.scopeManager.__nestFunctionScope(node, this.isInnerMethodDefinition);

        for (i = 0, iz = node.params.length; i < iz; ++i) {
            this.visitPattern(node.params[i], (pattern) => {
                this.currentScope().__define(pattern,
                    new ParameterDefinition(
                        pattern,
                        node,
                        i,
                        false
                    ));
            });
        }

        // if there's a rest argument, add that
        if (node.rest) {
            this.visitPattern({
                type: 'RestElement',
                argument: node.rest
            }, (pattern) => {
                this.currentScope().__define(pattern,
                    new ParameterDefinition(
                        pattern,
                        node,
                        node.params.length,
                        true
                    ));
            });
        }

        // Skip BlockStatement to prevent creating BlockStatement scope.
        if (node.body.type === Syntax.BlockStatement) {
            this.visitChildren(node.body);
        } else {
            this.visit(node.body);
        }

        this.close(node);
    }

    visitClass(node) {
        if (node.type === Syntax.ClassDeclaration) {
            this.currentScope().__define(node.id,
                    new Definition(
                        Variable.ClassName,
                        node.id,
                        node,
                        null,
                        null,
                        null
                    ));
        }

        // FIXME: Maybe consider TDZ.
        this.visit(node.superClass);

        this.scopeManager.__nestClassScope(node);

        if (node.id) {
            this.currentScope().__define(node.id,
                    new Definition(
                        Variable.ClassName,
                        node.id,
                        node
                    ));
        }
        this.visit(node.body);

        this.close(node);
    }

    visitProperty(node) {
        var previous, isMethodDefinition;
        if (node.computed) {
            this.visit(node.key);
        }

        isMethodDefinition = node.type === Syntax.MethodDefinition || node.method;
        if (isMethodDefinition) {
            previous = this.pushInnerMethodDefinition(true);
        }
        this.visit(node.value);
        if (isMethodDefinition) {
            this.popInnerMethodDefinition(previous);
        }
    }

    visitForIn(node) {
        if (node.left.type === Syntax.VariableDeclaration && node.left.kind !== 'var') {
            this.materializeTDZScope(node.right, node);
            this.visit(node.right);
            this.close(node.right);

            this.materializeIterationScope(node);
            this.visit(node.body);
            this.close(node);
        } else {
            if (node.left.type === Syntax.VariableDeclaration) {
                this.visit(node.left);
                this.visitPattern(node.left.declarations[0].id, (pattern) => {
                    this.currentScope().__referencing(pattern, Reference.WRITE, node.right, null, true);
                });
            } else {
                if (!isPattern(node.left)) {
                    this.visit(node.left);
                }
                this.visitPattern(node.left, (pattern) => {
                    var maybeImplicitGlobal = null;
                    if (!this.currentScope().isStrict) {
                        maybeImplicitGlobal = {
                            pattern: pattern,
                            node: node
                        };
                    }
                    this.currentScope().__referencing(pattern, Reference.WRITE, node.right, maybeImplicitGlobal, true);
                });
            }
            this.visit(node.right);
            this.visit(node.body);
        }
    }

    visitVariableDeclaration(variableTargetScope, type, node, index) {
        var decl, init;

        decl = node.declarations[index];
        init = decl.init;
        this.visitPattern(decl.id, (pattern, toplevel) => {
            variableTargetScope.__define(pattern,
                new Definition(
                    type,
                    pattern,
                    decl,
                    node,
                    index,
                    node.kind
                ));

            if (init) {
                this.currentScope().__referencing(pattern, Reference.WRITE, init, null, !toplevel);
            }
        });
    }

    AssignmentExpression(node) {
        if (isPattern(node.left)) {
            if (node.operator === '=') {
                this.visitPattern(node.left, (pattern, toplevel) => {
                    var maybeImplicitGlobal = null;
                    if (!this.currentScope().isStrict) {
                        maybeImplicitGlobal = {
                            pattern: pattern,
                            node: node
                        };
                    }
                    this.currentScope().__referencing(pattern, Reference.WRITE, node.right, maybeImplicitGlobal, !toplevel);
                });
            } else {
                this.currentScope().__referencing(node.left, Reference.RW, node.right);
            }
        } else {
            this.visit(node.left);
        }
        this.visit(node.right);
    }

    CatchClause(node) {
        this.scopeManager.__nestCatchScope(node);

        this.visitPattern(node.param, (pattern) => {
            this.currentScope().__define(pattern,
                new Definition(
                    Variable.CatchClause,
                    node.param,
                    node,
                    null,
                    null,
                    null
                ));
        });
        this.visit(node.body);

        this.close(node);
    }

    Program(node) {
        this.scopeManager.__nestGlobalScope(node);

        if (this.scopeManager.__isNodejsScope()) {
            // Force strictness of GlobalScope to false when using node.js scope.
            this.currentScope().isStrict = false;
            this.scopeManager.__nestFunctionScope(node, false);
        }

        if (this.scopeManager.__isES6() && this.scopeManager.isModule()) {
            this.scopeManager.__nestModuleScope(node);
        }

        this.visitChildren(node);
        this.close(node);
    }

    Identifier(node) {
        this.currentScope().__referencing(node);
    }

    UpdateExpression(node) {
        if (isPattern(node.argument)) {
            this.currentScope().__referencing(node.argument, Reference.RW, null);
        } else {
            this.visitChildren(node);
        }
    }

    MemberExpression(node) {
        this.visit(node.object);
        if (node.computed) {
            this.visit(node.property);
        }
    }

    Property(node) {
        this.visitProperty(node);
    }

    MethodDefinition(node) {
        this.visitProperty(node);
    }

    BreakStatement() {}

    ContinueStatement() {}

    LabeledStatement(node) {
        this.visit(node.body);
    }

    ForStatement(node) {
        // Create ForStatement declaration.
        // NOTE: In ES6, ForStatement dynamically generates
        // per iteration environment. However, escope is
        // a static analyzer, we only generate one scope for ForStatement.
        if (node.init && node.init.type === Syntax.VariableDeclaration && node.init.kind !== 'var') {
            this.scopeManager.__nestForScope(node);
        }

        this.visitChildren(node);

        this.close(node);
    }

    ClassExpression(node) {
        this.visitClass(node);
    }

    ClassDeclaration(node) {
        this.visitClass(node);
    }

    CallExpression(node) {
        // Check this is direct call to eval
        if (!this.scopeManager.__ignoreEval() && node.callee.type === Syntax.Identifier && node.callee.name === 'eval') {
            // NOTE: This should be `variableScope`. Since direct eval call always creates Lexical environment and
            // let / const should be enclosed into it. Only VariableDeclaration affects on the caller's environment.
            this.currentScope().variableScope.__detectEval();
        }
        this.visitChildren(node);
    }

    BlockStatement(node) {
        if (this.scopeManager.__isES6()) {
            this.scopeManager.__nestBlockScope(node);
        }

        this.visitChildren(node);

        this.close(node);
    }

    ThisExpression() {
        this.currentScope().variableScope.__detectThis();
    }

    WithStatement(node) {
        this.visit(node.object);
        // Then nest scope for WithStatement.
        this.scopeManager.__nestWithScope(node);

        this.visit(node.body);

        this.close(node);
    }

    VariableDeclaration(node) {
        var variableTargetScope, i, iz, decl;
        variableTargetScope = (node.kind === 'var') ? this.currentScope().variableScope : this.currentScope();
        for (i = 0, iz = node.declarations.length; i < iz; ++i) {
            decl = node.declarations[i];
            this.visitVariableDeclaration(variableTargetScope, Variable.Variable, node, i);
            if (decl.init) {
                this.visit(decl.init);
            }
        }
    }

    // sec 13.11.8
    SwitchStatement(node) {
        var i, iz;

        this.visit(node.discriminant);

        if (this.scopeManager.__isES6()) {
            this.scopeManager.__nestSwitchScope(node);
        }

        for (i = 0, iz = node.cases.length; i < iz; ++i) {
            this.visit(node.cases[i]);
        }

        this.close(node);
    }

    FunctionDeclaration(node) {
        this.visitFunction(node);
    }

    FunctionExpression(node) {
        this.visitFunction(node);
    }

    ForOfStatement(node) {
        this.visitForIn(node);
    }

    ForInStatement(node) {
        this.visitForIn(node);
    }

    ArrowFunctionExpression(node) {
        this.visitFunction(node);
    }

    ImportDeclaration(node) {
        var importer;

        assert(this.scopeManager.__isES6() && this.scopeManager.isModule(), 'ImportDeclaration should appear when the mode is ES6 and in the module context.');

        importer = new Importer(node, this);
        importer.visit(node);
    }

    visitExportDeclaration(node) {
        if (node.source) {
            return;
        }
        if (node.declaration) {
            this.visit(node.declaration);
            return;
        }

        this.visitChildren(node);
    }

    ExportDeclaration(node) {
        this.visitExportDeclaration(node);
    }

    ExportNamedDeclaration(node) {
        this.visitExportDeclaration(node);
    }

    ExportSpecifier(node) {
        let local = (node.id || node.local);
        this.visit(local);
    }
}

/* vim: set sw=4 ts=4 et tw=80 : */
