/**
 * @fileoverview The visitor keys for the node types Espree supports
 * @author Nicholas C. Zakas
 * @copyright 2015 Nicholas C. Zakas. All rights reserved.
 * @copyright 2012-2013 Yusuke Suzuki (twitter: @Constellation) and other contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Contains code from estraverse-fb.
 *
 * The MIT license. Copyright (c) 2014 Ingvar Stepanyan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

// None!

//------------------------------------------------------------------------------
// Public
//------------------------------------------------------------------------------

module.exports = {

    // ECMAScript
    AssignmentExpression: ["left", "right"],
    AssignmentPattern: ["left", "right"],
    ArrayExpression: ["elements"],
    ArrayPattern: ["elements"],
    ArrowFunctionExpression: ["params", "body"],
    BlockStatement: ["body"],
    BinaryExpression: ["left", "right"],
    BreakStatement: ["label"],
    CallExpression: ["callee", "arguments"],
    CatchClause: ["param", "body"],
    ClassBody: ["body"],
    ClassDeclaration: ["id", "superClass", "body"],
    ClassExpression: ["id", "superClass", "body"],
    ConditionalExpression: ["test", "consequent", "alternate"],
    ContinueStatement: ["label"],
    DebuggerStatement: [],
    DirectiveStatement: [],
    DoWhileStatement: ["body", "test"],
    EmptyStatement: [],
    ExportAllDeclaration: ["source"],
    ExportDefaultDeclaration: ["declaration"],
    ExportNamedDeclaration: ["declaration", "specifiers", "source"],
    ExportSpecifier: ["exported", "local"],
    ExpressionStatement: ["expression"],
    ForStatement: ["init", "test", "update", "body"],
    ForInStatement: ["left", "right", "body"],
    ForOfStatement: ["left", "right", "body"],
    FunctionDeclaration: ["id", "params", "body"],
    FunctionExpression: ["id", "params", "body"],
    Identifier: [],
    IfStatement: ["test", "consequent", "alternate"],
    ImportDeclaration: ["specifiers", "source"],
    ImportDefaultSpecifier: ["local"],
    ImportNamespaceSpecifier: ["local"],
    ImportSpecifier: ["imported", "local"],
    Literal: [],
    LabeledStatement: ["label", "body"],
    LogicalExpression: ["left", "right"],
    MemberExpression: ["object", "property"],
    MetaProperty: ["meta", "property"],
    MethodDefinition: ["key", "value"],
    ModuleSpecifier: [],
    NewExpression: ["callee", "arguments"],
    ObjectExpression: ["properties"],
    ObjectPattern: ["properties"],
    Program: ["body"],
    Property: ["key", "value"],
    RestElement: [ "argument" ],
    ReturnStatement: ["argument"],
    SequenceExpression: ["expressions"],
    SpreadElement: ["argument"],
    Super: [],
    SwitchStatement: ["discriminant", "cases"],
    SwitchCase: ["test", "consequent"],
    TaggedTemplateExpression: ["tag", "quasi"],
    TemplateElement: [],
    TemplateLiteral: ["quasis", "expressions"],
    ThisExpression: [],
    ThrowStatement: ["argument"],
    TryStatement: ["block", "handler", "finalizer"],
    UnaryExpression: ["argument"],
    UpdateExpression: ["argument"],
    VariableDeclaration: ["declarations"],
    VariableDeclarator: ["id", "init"],
    WhileStatement: ["test", "body"],
    WithStatement: ["object", "body"],
    YieldExpression: ["argument"],

    // JSX
    JSXIdentifier: [],
    JSXNamespacedName: ["namespace", "name"],
    JSXMemberExpression: ["object", "property"],
    JSXEmptyExpression: [],
    JSXExpressionContainer: ["expression"],
    JSXElement: ["openingElement", "closingElement", "children"],
    JSXClosingElement: ["name"],
    JSXOpeningElement: ["name", "attributes"],
    JSXAttribute: ["name", "value"],
    JSXText: null,
    JSXSpreadAttribute: ["argument"],

    // Experimental features
    ExperimentalRestProperty: ["argument"],
    ExperimentalSpreadProperty: ["argument"]
};
