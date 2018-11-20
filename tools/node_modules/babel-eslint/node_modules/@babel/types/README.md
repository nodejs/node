# @babel/types

> This module contains methods for building ASTs manually and for checking the types of AST nodes.

## Install

```sh
npm install --save-dev @babel/types
```

## API

<!-- begin generated section -->

### anyTypeAnnotation
```javascript
t.anyTypeAnnotation()
```

See also `t.isAnyTypeAnnotation(node, opts)` and `t.assertAnyTypeAnnotation(node, opts)`.

Aliases: `Flow`, `FlowBaseAnnotation`


---

### arrayExpression
```javascript
t.arrayExpression(elements)
```

See also `t.isArrayExpression(node, opts)` and `t.assertArrayExpression(node, opts)`.

Aliases: `Expression`

 - `elements`: `Array<null | Expression | SpreadElement>` (default: `[]`)

---

### arrayPattern
```javascript
t.arrayPattern(elements)
```

See also `t.isArrayPattern(node, opts)` and `t.assertArrayPattern(node, opts)`.

Aliases: `Pattern`, `PatternLike`, `LVal`

 - `elements`: `Array<PatternLike>` (required)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `typeAnnotation`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)

---

### arrayTypeAnnotation
```javascript
t.arrayTypeAnnotation(elementType)
```

See also `t.isArrayTypeAnnotation(node, opts)` and `t.assertArrayTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `elementType` (required)

---

### arrowFunctionExpression
```javascript
t.arrowFunctionExpression(params, body, async)
```

See also `t.isArrowFunctionExpression(node, opts)` and `t.assertArrowFunctionExpression(node, opts)`.

Aliases: `Scopable`, `Function`, `BlockParent`, `FunctionParent`, `Expression`, `Pureish`

 - `params`: `Array<LVal>` (required)
 - `body`: `BlockStatement | Expression` (required)
 - `async`: `boolean` (default: `false`)
 - `expression`: `boolean` (default: `null`)
 - `generator`: `boolean` (default: `false`)
 - `returnType`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)
 - `typeParameters`: `TypeParameterDeclaration | TSTypeParameterDeclaration | Noop` (default: `null`)

---

### assignmentExpression
```javascript
t.assignmentExpression(operator, left, right)
```

See also `t.isAssignmentExpression(node, opts)` and `t.assertAssignmentExpression(node, opts)`.

Aliases: `Expression`

 - `operator`: `string` (required)
 - `left`: `LVal` (required)
 - `right`: `Expression` (required)

---

### assignmentPattern
```javascript
t.assignmentPattern(left, right)
```

See also `t.isAssignmentPattern(node, opts)` and `t.assertAssignmentPattern(node, opts)`.

Aliases: `Pattern`, `PatternLike`, `LVal`

 - `left`: `Identifier | ObjectPattern | ArrayPattern` (required)
 - `right`: `Expression` (required)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `typeAnnotation`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)

---

### awaitExpression
```javascript
t.awaitExpression(argument)
```

See also `t.isAwaitExpression(node, opts)` and `t.assertAwaitExpression(node, opts)`.

Aliases: `Expression`, `Terminatorless`

 - `argument`: `Expression` (required)

---

### binaryExpression
```javascript
t.binaryExpression(operator, left, right)
```

See also `t.isBinaryExpression(node, opts)` and `t.assertBinaryExpression(node, opts)`.

Aliases: `Binary`, `Expression`

 - `operator`: `'+' | '-' | '/' | '%' | '*' | '**' | '&' | '|' | '>>' | '>>>' | '<<' | '^' | '==' | '===' | '!=' | '!==' | 'in' | 'instanceof' | '>' | '<' | '>=' | '<='` (required)
 - `left`: `Expression` (required)
 - `right`: `Expression` (required)

---

### bindExpression
```javascript
t.bindExpression(object, callee)
```

See also `t.isBindExpression(node, opts)` and `t.assertBindExpression(node, opts)`.

Aliases: `Expression`

 - `object` (required)
 - `callee` (required)

---

### blockStatement
```javascript
t.blockStatement(body, directives)
```

See also `t.isBlockStatement(node, opts)` and `t.assertBlockStatement(node, opts)`.

Aliases: `Scopable`, `BlockParent`, `Block`, `Statement`

 - `body`: `Array<Statement>` (required)
 - `directives`: `Array<Directive>` (default: `[]`)

---

### booleanLiteral
```javascript
t.booleanLiteral(value)
```

See also `t.isBooleanLiteral(node, opts)` and `t.assertBooleanLiteral(node, opts)`.

Aliases: `Expression`, `Pureish`, `Literal`, `Immutable`

 - `value`: `boolean` (required)

---

### booleanLiteralTypeAnnotation
```javascript
t.booleanLiteralTypeAnnotation()
```

See also `t.isBooleanLiteralTypeAnnotation(node, opts)` and `t.assertBooleanLiteralTypeAnnotation(node, opts)`.

Aliases: `Flow`


---

### booleanTypeAnnotation
```javascript
t.booleanTypeAnnotation()
```

See also `t.isBooleanTypeAnnotation(node, opts)` and `t.assertBooleanTypeAnnotation(node, opts)`.

Aliases: `Flow`, `FlowBaseAnnotation`


---

### breakStatement
```javascript
t.breakStatement(label)
```

See also `t.isBreakStatement(node, opts)` and `t.assertBreakStatement(node, opts)`.

Aliases: `Statement`, `Terminatorless`, `CompletionStatement`

 - `label`: `Identifier` (default: `null`)

---

### callExpression
```javascript
t.callExpression(callee, arguments)
```

See also `t.isCallExpression(node, opts)` and `t.assertCallExpression(node, opts)`.

Aliases: `Expression`

 - `callee`: `Expression` (required)
 - `arguments`: `Array<Expression | SpreadElement | JSXNamespacedName>` (required)
 - `optional`: `true | false` (default: `null`)
 - `typeParameters`: `TypeParameterInstantiation | TSTypeParameterInstantiation` (default: `null`)

---

### catchClause
```javascript
t.catchClause(param, body)
```

See also `t.isCatchClause(node, opts)` and `t.assertCatchClause(node, opts)`.

Aliases: `Scopable`, `BlockParent`

 - `param`: `Identifier` (default: `null`)
 - `body`: `BlockStatement` (required)

---

### classBody
```javascript
t.classBody(body)
```

See also `t.isClassBody(node, opts)` and `t.assertClassBody(node, opts)`.

 - `body`: `Array<ClassMethod | ClassProperty | TSDeclareMethod | TSIndexSignature>` (required)

---

### classDeclaration
```javascript
t.classDeclaration(id, superClass, body, decorators)
```

See also `t.isClassDeclaration(node, opts)` and `t.assertClassDeclaration(node, opts)`.

Aliases: `Scopable`, `Class`, `Statement`, `Declaration`, `Pureish`

 - `id`: `Identifier` (default: `null`)
 - `superClass`: `Expression` (default: `null`)
 - `body`: `ClassBody` (required)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `abstract`: `boolean` (default: `null`)
 - `declare`: `boolean` (default: `null`)
 - `implements`: `Array<TSExpressionWithTypeArguments | FlowClassImplements>` (default: `null`)
 - `mixins` (default: `null`)
 - `superTypeParameters`: `TypeParameterInstantiation | TSTypeParameterInstantiation` (default: `null`)
 - `typeParameters`: `TypeParameterDeclaration | TSTypeParameterDeclaration | Noop` (default: `null`)

---

### classExpression
```javascript
t.classExpression(id, superClass, body, decorators)
```

See also `t.isClassExpression(node, opts)` and `t.assertClassExpression(node, opts)`.

Aliases: `Scopable`, `Class`, `Expression`, `Pureish`

 - `id`: `Identifier` (default: `null`)
 - `superClass`: `Expression` (default: `null`)
 - `body`: `ClassBody` (required)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `implements`: `Array<TSExpressionWithTypeArguments | FlowClassImplements>` (default: `null`)
 - `mixins` (default: `null`)
 - `superTypeParameters`: `TypeParameterInstantiation | TSTypeParameterInstantiation` (default: `null`)
 - `typeParameters`: `TypeParameterDeclaration | TSTypeParameterDeclaration | Noop` (default: `null`)

---

### classImplements
```javascript
t.classImplements(id, typeParameters)
```

See also `t.isClassImplements(node, opts)` and `t.assertClassImplements(node, opts)`.

Aliases: `Flow`

 - `id` (required)
 - `typeParameters` (required)

---

### classMethod
```javascript
t.classMethod(kind, key, params, body, computed, static)
```

See also `t.isClassMethod(node, opts)` and `t.assertClassMethod(node, opts)`.

Aliases: `Function`, `Scopable`, `BlockParent`, `FunctionParent`, `Method`

 - `kind`: `"get" | "set" | "method" | "constructor"` (default: `'method'`)
 - `key`: if computed then `Expression` else `Identifier | Literal` (required)
 - `params`: `Array<LVal>` (required)
 - `body`: `BlockStatement` (required)
 - `computed`: `boolean` (default: `false`)
 - `static`: `boolean` (default: `null`)
 - `abstract`: `boolean` (default: `null`)
 - `access`: `"public" | "private" | "protected"` (default: `null`)
 - `accessibility`: `"public" | "private" | "protected"` (default: `null`)
 - `async`: `boolean` (default: `false`)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `generator`: `boolean` (default: `false`)
 - `optional`: `boolean` (default: `null`)
 - `returnType`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)
 - `typeParameters`: `TypeParameterDeclaration | TSTypeParameterDeclaration | Noop` (default: `null`)

---

### classProperty
```javascript
t.classProperty(key, value, typeAnnotation, decorators, computed)
```

See also `t.isClassProperty(node, opts)` and `t.assertClassProperty(node, opts)`.

Aliases: `Property`

 - `key` (required)
 - `value`: `Expression` (default: `null`)
 - `typeAnnotation`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `computed`: `boolean` (default: `false`)
 - `abstract`: `boolean` (default: `null`)
 - `accessibility`: `"public" | "private" | "protected"` (default: `null`)
 - `optional`: `boolean` (default: `null`)
 - `readonly`: `boolean` (default: `null`)
 - `static`: `boolean` (default: `null`)

---

### conditionalExpression
```javascript
t.conditionalExpression(test, consequent, alternate)
```

See also `t.isConditionalExpression(node, opts)` and `t.assertConditionalExpression(node, opts)`.

Aliases: `Expression`, `Conditional`

 - `test`: `Expression` (required)
 - `consequent`: `Expression` (required)
 - `alternate`: `Expression` (required)

---

### continueStatement
```javascript
t.continueStatement(label)
```

See also `t.isContinueStatement(node, opts)` and `t.assertContinueStatement(node, opts)`.

Aliases: `Statement`, `Terminatorless`, `CompletionStatement`

 - `label`: `Identifier` (default: `null`)

---

### debuggerStatement
```javascript
t.debuggerStatement()
```

See also `t.isDebuggerStatement(node, opts)` and `t.assertDebuggerStatement(node, opts)`.

Aliases: `Statement`


---

### declareClass
```javascript
t.declareClass(id, typeParameters, extends, body)
```

See also `t.isDeclareClass(node, opts)` and `t.assertDeclareClass(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)
 - `typeParameters` (required)
 - `extends` (required)
 - `body` (required)

---

### declareExportAllDeclaration
```javascript
t.declareExportAllDeclaration(source)
```

See also `t.isDeclareExportAllDeclaration(node, opts)` and `t.assertDeclareExportAllDeclaration(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `source` (required)

---

### declareExportDeclaration
```javascript
t.declareExportDeclaration(declaration, specifiers, source)
```

See also `t.isDeclareExportDeclaration(node, opts)` and `t.assertDeclareExportDeclaration(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `declaration` (required)
 - `specifiers` (required)
 - `source` (required)

---

### declareFunction
```javascript
t.declareFunction(id)
```

See also `t.isDeclareFunction(node, opts)` and `t.assertDeclareFunction(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)

---

### declareInterface
```javascript
t.declareInterface(id, typeParameters, extends, body)
```

See also `t.isDeclareInterface(node, opts)` and `t.assertDeclareInterface(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)
 - `typeParameters` (required)
 - `extends` (required)
 - `body` (required)

---

### declareModule
```javascript
t.declareModule(id, body)
```

See also `t.isDeclareModule(node, opts)` and `t.assertDeclareModule(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)
 - `body` (required)

---

### declareModuleExports
```javascript
t.declareModuleExports(typeAnnotation)
```

See also `t.isDeclareModuleExports(node, opts)` and `t.assertDeclareModuleExports(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `typeAnnotation` (required)

---

### declareOpaqueType
```javascript
t.declareOpaqueType(id, typeParameters, supertype)
```

See also `t.isDeclareOpaqueType(node, opts)` and `t.assertDeclareOpaqueType(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)
 - `typeParameters` (required)
 - `supertype` (required)

---

### declareTypeAlias
```javascript
t.declareTypeAlias(id, typeParameters, right)
```

See also `t.isDeclareTypeAlias(node, opts)` and `t.assertDeclareTypeAlias(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)
 - `typeParameters` (required)
 - `right` (required)

---

### declareVariable
```javascript
t.declareVariable(id)
```

See also `t.isDeclareVariable(node, opts)` and `t.assertDeclareVariable(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)

---

### declaredPredicate
```javascript
t.declaredPredicate(value)
```

See also `t.isDeclaredPredicate(node, opts)` and `t.assertDeclaredPredicate(node, opts)`.

Aliases: `Flow`, `FlowPredicate`

 - `value` (required)

---

### decorator
```javascript
t.decorator(expression)
```

See also `t.isDecorator(node, opts)` and `t.assertDecorator(node, opts)`.

 - `expression`: `Expression` (required)

---

### directive
```javascript
t.directive(value)
```

See also `t.isDirective(node, opts)` and `t.assertDirective(node, opts)`.

 - `value`: `DirectiveLiteral` (required)

---

### directiveLiteral
```javascript
t.directiveLiteral(value)
```

See also `t.isDirectiveLiteral(node, opts)` and `t.assertDirectiveLiteral(node, opts)`.

 - `value`: `string` (required)

---

### doExpression
```javascript
t.doExpression(body)
```

See also `t.isDoExpression(node, opts)` and `t.assertDoExpression(node, opts)`.

Aliases: `Expression`

 - `body`: `BlockStatement` (required)

---

### doWhileStatement
```javascript
t.doWhileStatement(test, body)
```

See also `t.isDoWhileStatement(node, opts)` and `t.assertDoWhileStatement(node, opts)`.

Aliases: `Statement`, `BlockParent`, `Loop`, `While`, `Scopable`

 - `test`: `Expression` (required)
 - `body`: `Statement` (required)

---

### emptyStatement
```javascript
t.emptyStatement()
```

See also `t.isEmptyStatement(node, opts)` and `t.assertEmptyStatement(node, opts)`.

Aliases: `Statement`


---

### emptyTypeAnnotation
```javascript
t.emptyTypeAnnotation()
```

See also `t.isEmptyTypeAnnotation(node, opts)` and `t.assertEmptyTypeAnnotation(node, opts)`.

Aliases: `Flow`, `FlowBaseAnnotation`


---

### existsTypeAnnotation
```javascript
t.existsTypeAnnotation()
```

See also `t.isExistsTypeAnnotation(node, opts)` and `t.assertExistsTypeAnnotation(node, opts)`.

Aliases: `Flow`


---

### exportAllDeclaration
```javascript
t.exportAllDeclaration(source)
```

See also `t.isExportAllDeclaration(node, opts)` and `t.assertExportAllDeclaration(node, opts)`.

Aliases: `Statement`, `Declaration`, `ModuleDeclaration`, `ExportDeclaration`

 - `source`: `StringLiteral` (required)

---

### exportDefaultDeclaration
```javascript
t.exportDefaultDeclaration(declaration)
```

See also `t.isExportDefaultDeclaration(node, opts)` and `t.assertExportDefaultDeclaration(node, opts)`.

Aliases: `Statement`, `Declaration`, `ModuleDeclaration`, `ExportDeclaration`

 - `declaration`: `FunctionDeclaration | TSDeclareFunction | ClassDeclaration | Expression` (required)

---

### exportDefaultSpecifier
```javascript
t.exportDefaultSpecifier(exported)
```

See also `t.isExportDefaultSpecifier(node, opts)` and `t.assertExportDefaultSpecifier(node, opts)`.

Aliases: `ModuleSpecifier`

 - `exported`: `Identifier` (required)

---

### exportNamedDeclaration
```javascript
t.exportNamedDeclaration(declaration, specifiers, source)
```

See also `t.isExportNamedDeclaration(node, opts)` and `t.assertExportNamedDeclaration(node, opts)`.

Aliases: `Statement`, `Declaration`, `ModuleDeclaration`, `ExportDeclaration`

 - `declaration`: `Declaration` (default: `null`)
 - `specifiers`: `Array<ExportSpecifier | ExportDefaultSpecifier | ExportNamespaceSpecifier>` (required)
 - `source`: `StringLiteral` (default: `null`)

---

### exportNamespaceSpecifier
```javascript
t.exportNamespaceSpecifier(exported)
```

See also `t.isExportNamespaceSpecifier(node, opts)` and `t.assertExportNamespaceSpecifier(node, opts)`.

Aliases: `ModuleSpecifier`

 - `exported`: `Identifier` (required)

---

### exportSpecifier
```javascript
t.exportSpecifier(local, exported)
```

See also `t.isExportSpecifier(node, opts)` and `t.assertExportSpecifier(node, opts)`.

Aliases: `ModuleSpecifier`

 - `local`: `Identifier` (required)
 - `exported`: `Identifier` (required)

---

### expressionStatement
```javascript
t.expressionStatement(expression)
```

See also `t.isExpressionStatement(node, opts)` and `t.assertExpressionStatement(node, opts)`.

Aliases: `Statement`, `ExpressionWrapper`

 - `expression`: `Expression` (required)

---

### file
```javascript
t.file(program, comments, tokens)
```

See also `t.isFile(node, opts)` and `t.assertFile(node, opts)`.

 - `program`: `Program` (required)
 - `comments` (required)
 - `tokens` (required)

---

### forInStatement
```javascript
t.forInStatement(left, right, body)
```

See also `t.isForInStatement(node, opts)` and `t.assertForInStatement(node, opts)`.

Aliases: `Scopable`, `Statement`, `For`, `BlockParent`, `Loop`, `ForXStatement`

 - `left`: `VariableDeclaration | LVal` (required)
 - `right`: `Expression` (required)
 - `body`: `Statement` (required)

---

### forOfStatement
```javascript
t.forOfStatement(left, right, body)
```

See also `t.isForOfStatement(node, opts)` and `t.assertForOfStatement(node, opts)`.

Aliases: `Scopable`, `Statement`, `For`, `BlockParent`, `Loop`, `ForXStatement`

 - `left`: `VariableDeclaration | LVal` (required)
 - `right`: `Expression` (required)
 - `body`: `Statement` (required)
 - `await`: `boolean` (default: `false`)

---

### forStatement
```javascript
t.forStatement(init, test, update, body)
```

See also `t.isForStatement(node, opts)` and `t.assertForStatement(node, opts)`.

Aliases: `Scopable`, `Statement`, `For`, `BlockParent`, `Loop`

 - `init`: `VariableDeclaration | Expression` (default: `null`)
 - `test`: `Expression` (default: `null`)
 - `update`: `Expression` (default: `null`)
 - `body`: `Statement` (required)

---

### functionDeclaration
```javascript
t.functionDeclaration(id, params, body, generator, async)
```

See also `t.isFunctionDeclaration(node, opts)` and `t.assertFunctionDeclaration(node, opts)`.

Aliases: `Scopable`, `Function`, `BlockParent`, `FunctionParent`, `Statement`, `Pureish`, `Declaration`

 - `id`: `Identifier` (default: `null`)
 - `params`: `Array<LVal>` (required)
 - `body`: `BlockStatement` (required)
 - `generator`: `boolean` (default: `false`)
 - `async`: `boolean` (default: `false`)
 - `declare`: `boolean` (default: `null`)
 - `returnType`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)
 - `typeParameters`: `TypeParameterDeclaration | TSTypeParameterDeclaration | Noop` (default: `null`)

---

### functionExpression
```javascript
t.functionExpression(id, params, body, generator, async)
```

See also `t.isFunctionExpression(node, opts)` and `t.assertFunctionExpression(node, opts)`.

Aliases: `Scopable`, `Function`, `BlockParent`, `FunctionParent`, `Expression`, `Pureish`

 - `id`: `Identifier` (default: `null`)
 - `params`: `Array<LVal>` (required)
 - `body`: `BlockStatement` (required)
 - `generator`: `boolean` (default: `false`)
 - `async`: `boolean` (default: `false`)
 - `returnType`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)
 - `typeParameters`: `TypeParameterDeclaration | TSTypeParameterDeclaration | Noop` (default: `null`)

---

### functionTypeAnnotation
```javascript
t.functionTypeAnnotation(typeParameters, params, rest, returnType)
```

See also `t.isFunctionTypeAnnotation(node, opts)` and `t.assertFunctionTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `typeParameters` (required)
 - `params` (required)
 - `rest` (required)
 - `returnType` (required)

---

### functionTypeParam
```javascript
t.functionTypeParam(name, typeAnnotation)
```

See also `t.isFunctionTypeParam(node, opts)` and `t.assertFunctionTypeParam(node, opts)`.

Aliases: `Flow`

 - `name` (required)
 - `typeAnnotation` (required)

---

### genericTypeAnnotation
```javascript
t.genericTypeAnnotation(id, typeParameters)
```

See also `t.isGenericTypeAnnotation(node, opts)` and `t.assertGenericTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `id` (required)
 - `typeParameters` (required)

---

### identifier
```javascript
t.identifier(name)
```

See also `t.isIdentifier(node, opts)` and `t.assertIdentifier(node, opts)`.

Aliases: `Expression`, `PatternLike`, `LVal`, `TSEntityName`

 - `name`: `string` (required)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `optional`: `boolean` (default: `null`)
 - `typeAnnotation`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)

---

### ifStatement
```javascript
t.ifStatement(test, consequent, alternate)
```

See also `t.isIfStatement(node, opts)` and `t.assertIfStatement(node, opts)`.

Aliases: `Statement`, `Conditional`

 - `test`: `Expression` (required)
 - `consequent`: `Statement` (required)
 - `alternate`: `Statement` (default: `null`)

---

### import
```javascript
t.import()
```

See also `t.isImport(node, opts)` and `t.assertImport(node, opts)`.

Aliases: `Expression`


---

### importDeclaration
```javascript
t.importDeclaration(specifiers, source)
```

See also `t.isImportDeclaration(node, opts)` and `t.assertImportDeclaration(node, opts)`.

Aliases: `Statement`, `Declaration`, `ModuleDeclaration`

 - `specifiers`: `Array<ImportSpecifier | ImportDefaultSpecifier | ImportNamespaceSpecifier>` (required)
 - `source`: `StringLiteral` (required)

---

### importDefaultSpecifier
```javascript
t.importDefaultSpecifier(local)
```

See also `t.isImportDefaultSpecifier(node, opts)` and `t.assertImportDefaultSpecifier(node, opts)`.

Aliases: `ModuleSpecifier`

 - `local`: `Identifier` (required)

---

### importNamespaceSpecifier
```javascript
t.importNamespaceSpecifier(local)
```

See also `t.isImportNamespaceSpecifier(node, opts)` and `t.assertImportNamespaceSpecifier(node, opts)`.

Aliases: `ModuleSpecifier`

 - `local`: `Identifier` (required)

---

### importSpecifier
```javascript
t.importSpecifier(local, imported)
```

See also `t.isImportSpecifier(node, opts)` and `t.assertImportSpecifier(node, opts)`.

Aliases: `ModuleSpecifier`

 - `local`: `Identifier` (required)
 - `imported`: `Identifier` (required)
 - `importKind`: `null | 'type' | 'typeof'` (default: `null`)

---

### inferredPredicate
```javascript
t.inferredPredicate()
```

See also `t.isInferredPredicate(node, opts)` and `t.assertInferredPredicate(node, opts)`.

Aliases: `Flow`, `FlowPredicate`


---

### interfaceDeclaration
```javascript
t.interfaceDeclaration(id, typeParameters, extends, body)
```

See also `t.isInterfaceDeclaration(node, opts)` and `t.assertInterfaceDeclaration(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)
 - `typeParameters` (required)
 - `extends` (required)
 - `body` (required)

---

### interfaceExtends
```javascript
t.interfaceExtends(id, typeParameters)
```

See also `t.isInterfaceExtends(node, opts)` and `t.assertInterfaceExtends(node, opts)`.

Aliases: `Flow`

 - `id` (required)
 - `typeParameters` (required)

---

### intersectionTypeAnnotation
```javascript
t.intersectionTypeAnnotation(types)
```

See also `t.isIntersectionTypeAnnotation(node, opts)` and `t.assertIntersectionTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `types` (required)

---

### jSXAttribute
```javascript
t.jSXAttribute(name, value)
```

See also `t.isJSXAttribute(node, opts)` and `t.assertJSXAttribute(node, opts)`.

Aliases: `JSX`, `Immutable`

 - `name`: `JSXIdentifier | JSXNamespacedName` (required)
 - `value`: `JSXElement | JSXFragment | StringLiteral | JSXExpressionContainer` (default: `null`)

---

### jSXClosingElement
```javascript
t.jSXClosingElement(name)
```

See also `t.isJSXClosingElement(node, opts)` and `t.assertJSXClosingElement(node, opts)`.

Aliases: `JSX`, `Immutable`

 - `name`: `JSXIdentifier | JSXMemberExpression` (required)

---

### jSXClosingFragment
```javascript
t.jSXClosingFragment()
```

See also `t.isJSXClosingFragment(node, opts)` and `t.assertJSXClosingFragment(node, opts)`.

Aliases: `JSX`, `Immutable`


---

### jSXElement
```javascript
t.jSXElement(openingElement, closingElement, children, selfClosing)
```

See also `t.isJSXElement(node, opts)` and `t.assertJSXElement(node, opts)`.

Aliases: `JSX`, `Immutable`, `Expression`

 - `openingElement`: `JSXOpeningElement` (required)
 - `closingElement`: `JSXClosingElement` (default: `null`)
 - `children`: `Array<JSXText | JSXExpressionContainer | JSXSpreadChild | JSXElement | JSXFragment>` (required)
 - `selfClosing` (required)

---

### jSXEmptyExpression
```javascript
t.jSXEmptyExpression()
```

See also `t.isJSXEmptyExpression(node, opts)` and `t.assertJSXEmptyExpression(node, opts)`.

Aliases: `JSX`


---

### jSXExpressionContainer
```javascript
t.jSXExpressionContainer(expression)
```

See also `t.isJSXExpressionContainer(node, opts)` and `t.assertJSXExpressionContainer(node, opts)`.

Aliases: `JSX`, `Immutable`

 - `expression`: `Expression` (required)

---

### jSXFragment
```javascript
t.jSXFragment(openingFragment, closingFragment, children)
```

See also `t.isJSXFragment(node, opts)` and `t.assertJSXFragment(node, opts)`.

Aliases: `JSX`, `Immutable`, `Expression`

 - `openingFragment`: `JSXOpeningFragment` (required)
 - `closingFragment`: `JSXClosingFragment` (required)
 - `children`: `Array<JSXText | JSXExpressionContainer | JSXSpreadChild | JSXElement | JSXFragment>` (required)

---

### jSXIdentifier
```javascript
t.jSXIdentifier(name)
```

See also `t.isJSXIdentifier(node, opts)` and `t.assertJSXIdentifier(node, opts)`.

Aliases: `JSX`

 - `name`: `string` (required)

---

### jSXMemberExpression
```javascript
t.jSXMemberExpression(object, property)
```

See also `t.isJSXMemberExpression(node, opts)` and `t.assertJSXMemberExpression(node, opts)`.

Aliases: `JSX`

 - `object`: `JSXMemberExpression | JSXIdentifier` (required)
 - `property`: `JSXIdentifier` (required)

---

### jSXNamespacedName
```javascript
t.jSXNamespacedName(namespace, name)
```

See also `t.isJSXNamespacedName(node, opts)` and `t.assertJSXNamespacedName(node, opts)`.

Aliases: `JSX`

 - `namespace`: `JSXIdentifier` (required)
 - `name`: `JSXIdentifier` (required)

---

### jSXOpeningElement
```javascript
t.jSXOpeningElement(name, attributes, selfClosing)
```

See also `t.isJSXOpeningElement(node, opts)` and `t.assertJSXOpeningElement(node, opts)`.

Aliases: `JSX`, `Immutable`

 - `name`: `JSXIdentifier | JSXMemberExpression` (required)
 - `attributes`: `Array<JSXAttribute | JSXSpreadAttribute>` (required)
 - `selfClosing`: `boolean` (default: `false`)

---

### jSXOpeningFragment
```javascript
t.jSXOpeningFragment()
```

See also `t.isJSXOpeningFragment(node, opts)` and `t.assertJSXOpeningFragment(node, opts)`.

Aliases: `JSX`, `Immutable`


---

### jSXSpreadAttribute
```javascript
t.jSXSpreadAttribute(argument)
```

See also `t.isJSXSpreadAttribute(node, opts)` and `t.assertJSXSpreadAttribute(node, opts)`.

Aliases: `JSX`

 - `argument`: `Expression` (required)

---

### jSXSpreadChild
```javascript
t.jSXSpreadChild(expression)
```

See also `t.isJSXSpreadChild(node, opts)` and `t.assertJSXSpreadChild(node, opts)`.

Aliases: `JSX`, `Immutable`

 - `expression`: `Expression` (required)

---

### jSXText
```javascript
t.jSXText(value)
```

See also `t.isJSXText(node, opts)` and `t.assertJSXText(node, opts)`.

Aliases: `JSX`, `Immutable`

 - `value`: `string` (required)

---

### labeledStatement
```javascript
t.labeledStatement(label, body)
```

See also `t.isLabeledStatement(node, opts)` and `t.assertLabeledStatement(node, opts)`.

Aliases: `Statement`

 - `label`: `Identifier` (required)
 - `body`: `Statement` (required)

---

### logicalExpression
```javascript
t.logicalExpression(operator, left, right)
```

See also `t.isLogicalExpression(node, opts)` and `t.assertLogicalExpression(node, opts)`.

Aliases: `Binary`, `Expression`

 - `operator`: `'||' | '&&' | '??'` (required)
 - `left`: `Expression` (required)
 - `right`: `Expression` (required)

---

### memberExpression
```javascript
t.memberExpression(object, property, computed, optional)
```

See also `t.isMemberExpression(node, opts)` and `t.assertMemberExpression(node, opts)`.

Aliases: `Expression`, `LVal`

 - `object`: `Expression` (required)
 - `property`: if computed then `Expression` else `Identifier` (required)
 - `computed`: `boolean` (default: `false`)
 - `optional`: `true | false` (default: `null`)

---

### metaProperty
```javascript
t.metaProperty(meta, property)
```

See also `t.isMetaProperty(node, opts)` and `t.assertMetaProperty(node, opts)`.

Aliases: `Expression`

 - `meta`: `Identifier` (required)
 - `property`: `Identifier` (required)

---

### mixedTypeAnnotation
```javascript
t.mixedTypeAnnotation()
```

See also `t.isMixedTypeAnnotation(node, opts)` and `t.assertMixedTypeAnnotation(node, opts)`.

Aliases: `Flow`, `FlowBaseAnnotation`


---

### newExpression
```javascript
t.newExpression(callee, arguments)
```

See also `t.isNewExpression(node, opts)` and `t.assertNewExpression(node, opts)`.

Aliases: `Expression`

 - `callee`: `Expression` (required)
 - `arguments`: `Array<Expression | SpreadElement | JSXNamespacedName>` (required)
 - `optional`: `true | false` (default: `null`)
 - `typeParameters`: `TypeParameterInstantiation | TSTypeParameterInstantiation` (default: `null`)

---

### noop
```javascript
t.noop()
```

See also `t.isNoop(node, opts)` and `t.assertNoop(node, opts)`.


---

### nullLiteral
```javascript
t.nullLiteral()
```

See also `t.isNullLiteral(node, opts)` and `t.assertNullLiteral(node, opts)`.

Aliases: `Expression`, `Pureish`, `Literal`, `Immutable`


---

### nullLiteralTypeAnnotation
```javascript
t.nullLiteralTypeAnnotation()
```

See also `t.isNullLiteralTypeAnnotation(node, opts)` and `t.assertNullLiteralTypeAnnotation(node, opts)`.

Aliases: `Flow`, `FlowBaseAnnotation`


---

### nullableTypeAnnotation
```javascript
t.nullableTypeAnnotation(typeAnnotation)
```

See also `t.isNullableTypeAnnotation(node, opts)` and `t.assertNullableTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `typeAnnotation` (required)

---

### numberLiteralTypeAnnotation
```javascript
t.numberLiteralTypeAnnotation()
```

See also `t.isNumberLiteralTypeAnnotation(node, opts)` and `t.assertNumberLiteralTypeAnnotation(node, opts)`.

Aliases: `Flow`


---

### numberTypeAnnotation
```javascript
t.numberTypeAnnotation()
```

See also `t.isNumberTypeAnnotation(node, opts)` and `t.assertNumberTypeAnnotation(node, opts)`.

Aliases: `Flow`, `FlowBaseAnnotation`


---

### numericLiteral
```javascript
t.numericLiteral(value)
```

See also `t.isNumericLiteral(node, opts)` and `t.assertNumericLiteral(node, opts)`.

Aliases: `Expression`, `Pureish`, `Literal`, `Immutable`

 - `value`: `number` (required)

---

### objectExpression
```javascript
t.objectExpression(properties)
```

See also `t.isObjectExpression(node, opts)` and `t.assertObjectExpression(node, opts)`.

Aliases: `Expression`

 - `properties`: `Array<ObjectMethod | ObjectProperty | SpreadElement>` (required)

---

### objectMethod
```javascript
t.objectMethod(kind, key, params, body, computed)
```

See also `t.isObjectMethod(node, opts)` and `t.assertObjectMethod(node, opts)`.

Aliases: `UserWhitespacable`, `Function`, `Scopable`, `BlockParent`, `FunctionParent`, `Method`, `ObjectMember`

 - `kind`: `"method" | "get" | "set"` (default: `'method'`)
 - `key`: if computed then `Expression` else `Identifier | Literal` (required)
 - `params`: `Array<LVal>` (required)
 - `body`: `BlockStatement` (required)
 - `computed`: `boolean` (default: `false`)
 - `async`: `boolean` (default: `false`)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `generator`: `boolean` (default: `false`)
 - `returnType`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)
 - `typeParameters`: `TypeParameterDeclaration | TSTypeParameterDeclaration | Noop` (default: `null`)

---

### objectPattern
```javascript
t.objectPattern(properties)
```

See also `t.isObjectPattern(node, opts)` and `t.assertObjectPattern(node, opts)`.

Aliases: `Pattern`, `PatternLike`, `LVal`

 - `properties`: `Array<RestElement | ObjectProperty>` (required)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `typeAnnotation`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)

---

### objectProperty
```javascript
t.objectProperty(key, value, computed, shorthand, decorators)
```

See also `t.isObjectProperty(node, opts)` and `t.assertObjectProperty(node, opts)`.

Aliases: `UserWhitespacable`, `Property`, `ObjectMember`

 - `key`: if computed then `Expression` else `Identifier | Literal` (required)
 - `value`: `Expression | PatternLike` (required)
 - `computed`: `boolean` (default: `false`)
 - `shorthand`: `boolean` (default: `false`)
 - `decorators`: `Array<Decorator>` (default: `null`)

---

### objectTypeAnnotation
```javascript
t.objectTypeAnnotation(properties, indexers, callProperties)
```

See also `t.isObjectTypeAnnotation(node, opts)` and `t.assertObjectTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `properties` (required)
 - `indexers` (required)
 - `callProperties` (required)

---

### objectTypeCallProperty
```javascript
t.objectTypeCallProperty(value)
```

See also `t.isObjectTypeCallProperty(node, opts)` and `t.assertObjectTypeCallProperty(node, opts)`.

Aliases: `Flow`, `UserWhitespacable`

 - `value` (required)

---

### objectTypeIndexer
```javascript
t.objectTypeIndexer(id, key, value)
```

See also `t.isObjectTypeIndexer(node, opts)` and `t.assertObjectTypeIndexer(node, opts)`.

Aliases: `Flow`, `UserWhitespacable`

 - `id` (required)
 - `key` (required)
 - `value` (required)

---

### objectTypeProperty
```javascript
t.objectTypeProperty(key, value)
```

See also `t.isObjectTypeProperty(node, opts)` and `t.assertObjectTypeProperty(node, opts)`.

Aliases: `Flow`, `UserWhitespacable`

 - `key` (required)
 - `value` (required)

---

### objectTypeSpreadProperty
```javascript
t.objectTypeSpreadProperty(argument)
```

See also `t.isObjectTypeSpreadProperty(node, opts)` and `t.assertObjectTypeSpreadProperty(node, opts)`.

Aliases: `Flow`, `UserWhitespacable`

 - `argument` (required)

---

### opaqueType
```javascript
t.opaqueType(id, typeParameters, supertype, impltype)
```

See also `t.isOpaqueType(node, opts)` and `t.assertOpaqueType(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)
 - `typeParameters` (required)
 - `supertype` (required)
 - `impltype` (required)

---

### parenthesizedExpression
```javascript
t.parenthesizedExpression(expression)
```

See also `t.isParenthesizedExpression(node, opts)` and `t.assertParenthesizedExpression(node, opts)`.

Aliases: `Expression`, `ExpressionWrapper`

 - `expression`: `Expression` (required)

---

### program
```javascript
t.program(body, directives, sourceType)
```

See also `t.isProgram(node, opts)` and `t.assertProgram(node, opts)`.

Aliases: `Scopable`, `BlockParent`, `Block`

 - `body`: `Array<Statement>` (required)
 - `directives`: `Array<Directive>` (default: `[]`)
 - `sourceType`: `'script' | 'module'` (default: `'script'`)
 - `sourceFile`: `string` (default: `null`)

---

### qualifiedTypeIdentifier
```javascript
t.qualifiedTypeIdentifier(id, qualification)
```

See also `t.isQualifiedTypeIdentifier(node, opts)` and `t.assertQualifiedTypeIdentifier(node, opts)`.

Aliases: `Flow`

 - `id` (required)
 - `qualification` (required)

---

### regExpLiteral
```javascript
t.regExpLiteral(pattern, flags)
```

See also `t.isRegExpLiteral(node, opts)` and `t.assertRegExpLiteral(node, opts)`.

Aliases: `Expression`, `Literal`

 - `pattern`: `string` (required)
 - `flags`: `string` (default: `''`)

---

### restElement
```javascript
t.restElement(argument)
```

See also `t.isRestElement(node, opts)` and `t.assertRestElement(node, opts)`.

Aliases: `LVal`, `PatternLike`

 - `argument`: `LVal` (required)
 - `decorators`: `Array<Decorator>` (default: `null`)
 - `typeAnnotation`: `TypeAnnotation | TSTypeAnnotation | Noop` (default: `null`)

---

### returnStatement
```javascript
t.returnStatement(argument)
```

See also `t.isReturnStatement(node, opts)` and `t.assertReturnStatement(node, opts)`.

Aliases: `Statement`, `Terminatorless`, `CompletionStatement`

 - `argument`: `Expression` (default: `null`)

---

### sequenceExpression
```javascript
t.sequenceExpression(expressions)
```

See also `t.isSequenceExpression(node, opts)` and `t.assertSequenceExpression(node, opts)`.

Aliases: `Expression`

 - `expressions`: `Array<Expression>` (required)

---

### spreadElement
```javascript
t.spreadElement(argument)
```

See also `t.isSpreadElement(node, opts)` and `t.assertSpreadElement(node, opts)`.

Aliases: `UnaryLike`

 - `argument`: `Expression` (required)

---

### stringLiteral
```javascript
t.stringLiteral(value)
```

See also `t.isStringLiteral(node, opts)` and `t.assertStringLiteral(node, opts)`.

Aliases: `Expression`, `Pureish`, `Literal`, `Immutable`

 - `value`: `string` (required)

---

### stringLiteralTypeAnnotation
```javascript
t.stringLiteralTypeAnnotation()
```

See also `t.isStringLiteralTypeAnnotation(node, opts)` and `t.assertStringLiteralTypeAnnotation(node, opts)`.

Aliases: `Flow`


---

### stringTypeAnnotation
```javascript
t.stringTypeAnnotation()
```

See also `t.isStringTypeAnnotation(node, opts)` and `t.assertStringTypeAnnotation(node, opts)`.

Aliases: `Flow`, `FlowBaseAnnotation`


---

### super
```javascript
t.super()
```

See also `t.isSuper(node, opts)` and `t.assertSuper(node, opts)`.

Aliases: `Expression`


---

### switchCase
```javascript
t.switchCase(test, consequent)
```

See also `t.isSwitchCase(node, opts)` and `t.assertSwitchCase(node, opts)`.

 - `test`: `Expression` (default: `null`)
 - `consequent`: `Array<Statement>` (required)

---

### switchStatement
```javascript
t.switchStatement(discriminant, cases)
```

See also `t.isSwitchStatement(node, opts)` and `t.assertSwitchStatement(node, opts)`.

Aliases: `Statement`, `BlockParent`, `Scopable`

 - `discriminant`: `Expression` (required)
 - `cases`: `Array<SwitchCase>` (required)

---

### tSAnyKeyword
```javascript
t.tSAnyKeyword()
```

See also `t.isTSAnyKeyword(node, opts)` and `t.assertTSAnyKeyword(node, opts)`.

Aliases: `TSType`


---

### tSArrayType
```javascript
t.tSArrayType(elementType)
```

See also `t.isTSArrayType(node, opts)` and `t.assertTSArrayType(node, opts)`.

Aliases: `TSType`

 - `elementType`: `TSType` (required)

---

### tSAsExpression
```javascript
t.tSAsExpression(expression, typeAnnotation)
```

See also `t.isTSAsExpression(node, opts)` and `t.assertTSAsExpression(node, opts)`.

Aliases: `Expression`

 - `expression`: `Expression` (required)
 - `typeAnnotation`: `TSType` (required)

---

### tSBooleanKeyword
```javascript
t.tSBooleanKeyword()
```

See also `t.isTSBooleanKeyword(node, opts)` and `t.assertTSBooleanKeyword(node, opts)`.

Aliases: `TSType`


---

### tSCallSignatureDeclaration
```javascript
t.tSCallSignatureDeclaration(typeParameters, parameters, typeAnnotation)
```

See also `t.isTSCallSignatureDeclaration(node, opts)` and `t.assertTSCallSignatureDeclaration(node, opts)`.

Aliases: `TSTypeElement`

 - `typeParameters`: `TSTypeParameterDeclaration` (default: `null`)
 - `parameters`: `Array<Identifier | RestElement>` (default: `null`)
 - `typeAnnotation`: `TSTypeAnnotation` (default: `null`)

---

### tSConstructSignatureDeclaration
```javascript
t.tSConstructSignatureDeclaration(typeParameters, parameters, typeAnnotation)
```

See also `t.isTSConstructSignatureDeclaration(node, opts)` and `t.assertTSConstructSignatureDeclaration(node, opts)`.

Aliases: `TSTypeElement`

 - `typeParameters`: `TSTypeParameterDeclaration` (default: `null`)
 - `parameters`: `Array<Identifier | RestElement>` (default: `null`)
 - `typeAnnotation`: `TSTypeAnnotation` (default: `null`)

---

### tSConstructorType
```javascript
t.tSConstructorType(typeParameters, typeAnnotation)
```

See also `t.isTSConstructorType(node, opts)` and `t.assertTSConstructorType(node, opts)`.

Aliases: `TSType`

 - `typeParameters`: `TSTypeParameterDeclaration` (default: `null`)
 - `typeAnnotation`: `TSTypeAnnotation` (default: `null`)
 - `parameters`: `Array<Identifier | RestElement>` (default: `null`)

---

### tSDeclareFunction
```javascript
t.tSDeclareFunction(id, typeParameters, params, returnType)
```

See also `t.isTSDeclareFunction(node, opts)` and `t.assertTSDeclareFunction(node, opts)`.

Aliases: `Statement`, `Declaration`

 - `id`: `Identifier` (default: `null`)
 - `typeParameters`: `TSTypeParameterDeclaration | Noop` (default: `null`)
 - `params`: `Array<LVal>` (required)
 - `returnType`: `TSTypeAnnotation | Noop` (default: `null`)
 - `async`: `boolean` (default: `false`)
 - `declare`: `boolean` (default: `null`)
 - `generator`: `boolean` (default: `false`)

---

### tSDeclareMethod
```javascript
t.tSDeclareMethod(decorators, key, typeParameters, params, returnType)
```

See also `t.isTSDeclareMethod(node, opts)` and `t.assertTSDeclareMethod(node, opts)`.

 - `decorators`: `Array<Decorator>` (default: `null`)
 - `key` (required)
 - `typeParameters`: `TSTypeParameterDeclaration | Noop` (default: `null`)
 - `params`: `Array<LVal>` (required)
 - `returnType`: `TSTypeAnnotation | Noop` (default: `null`)
 - `abstract`: `boolean` (default: `null`)
 - `access`: `"public" | "private" | "protected"` (default: `null`)
 - `accessibility`: `"public" | "private" | "protected"` (default: `null`)
 - `async`: `boolean` (default: `false`)
 - `computed`: `boolean` (default: `false`)
 - `generator`: `boolean` (default: `false`)
 - `kind`: `"get" | "set" | "method" | "constructor"` (default: `'method'`)
 - `optional`: `boolean` (default: `null`)
 - `static`: `boolean` (default: `null`)

---

### tSEnumDeclaration
```javascript
t.tSEnumDeclaration(id, members)
```

See also `t.isTSEnumDeclaration(node, opts)` and `t.assertTSEnumDeclaration(node, opts)`.

Aliases: `Statement`, `Declaration`

 - `id`: `Identifier` (required)
 - `members`: `Array<TSEnumMember>` (required)
 - `const`: `boolean` (default: `null`)
 - `declare`: `boolean` (default: `null`)
 - `initializer`: `Expression` (default: `null`)

---

### tSEnumMember
```javascript
t.tSEnumMember(id, initializer)
```

See also `t.isTSEnumMember(node, opts)` and `t.assertTSEnumMember(node, opts)`.

 - `id`: `Identifier | StringLiteral` (required)
 - `initializer`: `Expression` (default: `null`)

---

### tSExportAssignment
```javascript
t.tSExportAssignment(expression)
```

See also `t.isTSExportAssignment(node, opts)` and `t.assertTSExportAssignment(node, opts)`.

Aliases: `Statement`

 - `expression`: `Expression` (required)

---

### tSExpressionWithTypeArguments
```javascript
t.tSExpressionWithTypeArguments(expression, typeParameters)
```

See also `t.isTSExpressionWithTypeArguments(node, opts)` and `t.assertTSExpressionWithTypeArguments(node, opts)`.

Aliases: `TSType`

 - `expression`: `TSEntityName` (required)
 - `typeParameters`: `TSTypeParameterInstantiation` (default: `null`)

---

### tSExternalModuleReference
```javascript
t.tSExternalModuleReference(expression)
```

See also `t.isTSExternalModuleReference(node, opts)` and `t.assertTSExternalModuleReference(node, opts)`.

 - `expression`: `StringLiteral` (required)

---

### tSFunctionType
```javascript
t.tSFunctionType(typeParameters, typeAnnotation)
```

See also `t.isTSFunctionType(node, opts)` and `t.assertTSFunctionType(node, opts)`.

Aliases: `TSType`

 - `typeParameters`: `TSTypeParameterDeclaration` (default: `null`)
 - `typeAnnotation`: `TSTypeAnnotation` (default: `null`)
 - `parameters`: `Array<Identifier | RestElement>` (default: `null`)

---

### tSImportEqualsDeclaration
```javascript
t.tSImportEqualsDeclaration(id, moduleReference)
```

See also `t.isTSImportEqualsDeclaration(node, opts)` and `t.assertTSImportEqualsDeclaration(node, opts)`.

Aliases: `Statement`

 - `id`: `Identifier` (required)
 - `moduleReference`: `TSEntityName | TSExternalModuleReference` (required)
 - `isExport`: `boolean` (default: `null`)

---

### tSIndexSignature
```javascript
t.tSIndexSignature(parameters, typeAnnotation)
```

See also `t.isTSIndexSignature(node, opts)` and `t.assertTSIndexSignature(node, opts)`.

Aliases: `TSTypeElement`

 - `parameters`: `Array<Identifier>` (required)
 - `typeAnnotation`: `TSTypeAnnotation` (default: `null`)
 - `readonly`: `boolean` (default: `null`)

---

### tSIndexedAccessType
```javascript
t.tSIndexedAccessType(objectType, indexType)
```

See also `t.isTSIndexedAccessType(node, opts)` and `t.assertTSIndexedAccessType(node, opts)`.

Aliases: `TSType`

 - `objectType`: `TSType` (required)
 - `indexType`: `TSType` (required)

---

### tSInterfaceBody
```javascript
t.tSInterfaceBody(body)
```

See also `t.isTSInterfaceBody(node, opts)` and `t.assertTSInterfaceBody(node, opts)`.

 - `body`: `Array<TSTypeElement>` (required)

---

### tSInterfaceDeclaration
```javascript
t.tSInterfaceDeclaration(id, typeParameters, extends, body)
```

See also `t.isTSInterfaceDeclaration(node, opts)` and `t.assertTSInterfaceDeclaration(node, opts)`.

Aliases: `Statement`, `Declaration`

 - `id`: `Identifier` (required)
 - `typeParameters`: `TSTypeParameterDeclaration` (default: `null`)
 - `extends`: `Array<TSExpressionWithTypeArguments>` (default: `null`)
 - `body`: `TSInterfaceBody` (required)
 - `declare`: `boolean` (default: `null`)

---

### tSIntersectionType
```javascript
t.tSIntersectionType(types)
```

See also `t.isTSIntersectionType(node, opts)` and `t.assertTSIntersectionType(node, opts)`.

Aliases: `TSType`

 - `types`: `Array<TSType>` (required)

---

### tSLiteralType
```javascript
t.tSLiteralType(literal)
```

See also `t.isTSLiteralType(node, opts)` and `t.assertTSLiteralType(node, opts)`.

Aliases: `TSType`

 - `literal`: `NumericLiteral | StringLiteral | BooleanLiteral` (required)

---

### tSMappedType
```javascript
t.tSMappedType(typeParameter, typeAnnotation)
```

See also `t.isTSMappedType(node, opts)` and `t.assertTSMappedType(node, opts)`.

Aliases: `TSType`

 - `typeParameter`: `TSTypeParameter` (required)
 - `typeAnnotation`: `TSType` (default: `null`)
 - `optional`: `boolean` (default: `null`)
 - `readonly`: `boolean` (default: `null`)

---

### tSMethodSignature
```javascript
t.tSMethodSignature(key, typeParameters, parameters, typeAnnotation)
```

See also `t.isTSMethodSignature(node, opts)` and `t.assertTSMethodSignature(node, opts)`.

Aliases: `TSTypeElement`

 - `key`: `Expression` (required)
 - `typeParameters`: `TSTypeParameterDeclaration` (default: `null`)
 - `parameters`: `Array<Identifier | RestElement>` (default: `null`)
 - `typeAnnotation`: `TSTypeAnnotation` (default: `null`)
 - `computed`: `boolean` (default: `null`)
 - `optional`: `boolean` (default: `null`)

---

### tSModuleBlock
```javascript
t.tSModuleBlock(body)
```

See also `t.isTSModuleBlock(node, opts)` and `t.assertTSModuleBlock(node, opts)`.

 - `body`: `Array<Statement>` (required)

---

### tSModuleDeclaration
```javascript
t.tSModuleDeclaration(id, body)
```

See also `t.isTSModuleDeclaration(node, opts)` and `t.assertTSModuleDeclaration(node, opts)`.

Aliases: `Statement`, `Declaration`

 - `id`: `Identifier | StringLiteral` (required)
 - `body`: `TSModuleBlock | TSModuleDeclaration` (required)
 - `declare`: `boolean` (default: `null`)
 - `global`: `boolean` (default: `null`)

---

### tSNamespaceExportDeclaration
```javascript
t.tSNamespaceExportDeclaration(id)
```

See also `t.isTSNamespaceExportDeclaration(node, opts)` and `t.assertTSNamespaceExportDeclaration(node, opts)`.

Aliases: `Statement`

 - `id`: `Identifier` (required)

---

### tSNeverKeyword
```javascript
t.tSNeverKeyword()
```

See also `t.isTSNeverKeyword(node, opts)` and `t.assertTSNeverKeyword(node, opts)`.

Aliases: `TSType`


---

### tSNonNullExpression
```javascript
t.tSNonNullExpression(expression)
```

See also `t.isTSNonNullExpression(node, opts)` and `t.assertTSNonNullExpression(node, opts)`.

Aliases: `Expression`

 - `expression`: `Expression` (required)

---

### tSNullKeyword
```javascript
t.tSNullKeyword()
```

See also `t.isTSNullKeyword(node, opts)` and `t.assertTSNullKeyword(node, opts)`.

Aliases: `TSType`


---

### tSNumberKeyword
```javascript
t.tSNumberKeyword()
```

See also `t.isTSNumberKeyword(node, opts)` and `t.assertTSNumberKeyword(node, opts)`.

Aliases: `TSType`


---

### tSObjectKeyword
```javascript
t.tSObjectKeyword()
```

See also `t.isTSObjectKeyword(node, opts)` and `t.assertTSObjectKeyword(node, opts)`.

Aliases: `TSType`


---

### tSParameterProperty
```javascript
t.tSParameterProperty(parameter)
```

See also `t.isTSParameterProperty(node, opts)` and `t.assertTSParameterProperty(node, opts)`.

Aliases: `LVal`

 - `parameter`: `Identifier | AssignmentPattern` (required)
 - `accessibility`: `'public' | 'private' | 'protected'` (default: `null`)
 - `readonly`: `boolean` (default: `null`)

---

### tSParenthesizedType
```javascript
t.tSParenthesizedType(typeAnnotation)
```

See also `t.isTSParenthesizedType(node, opts)` and `t.assertTSParenthesizedType(node, opts)`.

Aliases: `TSType`

 - `typeAnnotation`: `TSType` (required)

---

### tSPropertySignature
```javascript
t.tSPropertySignature(key, typeAnnotation, initializer)
```

See also `t.isTSPropertySignature(node, opts)` and `t.assertTSPropertySignature(node, opts)`.

Aliases: `TSTypeElement`

 - `key`: `Expression` (required)
 - `typeAnnotation`: `TSTypeAnnotation` (default: `null`)
 - `initializer`: `Expression` (default: `null`)
 - `computed`: `boolean` (default: `null`)
 - `optional`: `boolean` (default: `null`)
 - `readonly`: `boolean` (default: `null`)

---

### tSQualifiedName
```javascript
t.tSQualifiedName(left, right)
```

See also `t.isTSQualifiedName(node, opts)` and `t.assertTSQualifiedName(node, opts)`.

Aliases: `TSEntityName`

 - `left`: `TSEntityName` (required)
 - `right`: `Identifier` (required)

---

### tSStringKeyword
```javascript
t.tSStringKeyword()
```

See also `t.isTSStringKeyword(node, opts)` and `t.assertTSStringKeyword(node, opts)`.

Aliases: `TSType`


---

### tSSymbolKeyword
```javascript
t.tSSymbolKeyword()
```

See also `t.isTSSymbolKeyword(node, opts)` and `t.assertTSSymbolKeyword(node, opts)`.

Aliases: `TSType`


---

### tSThisType
```javascript
t.tSThisType()
```

See also `t.isTSThisType(node, opts)` and `t.assertTSThisType(node, opts)`.

Aliases: `TSType`


---

### tSTupleType
```javascript
t.tSTupleType(elementTypes)
```

See also `t.isTSTupleType(node, opts)` and `t.assertTSTupleType(node, opts)`.

Aliases: `TSType`

 - `elementTypes`: `Array<TSType>` (required)

---

### tSTypeAliasDeclaration
```javascript
t.tSTypeAliasDeclaration(id, typeParameters, typeAnnotation)
```

See also `t.isTSTypeAliasDeclaration(node, opts)` and `t.assertTSTypeAliasDeclaration(node, opts)`.

Aliases: `Statement`, `Declaration`

 - `id`: `Identifier` (required)
 - `typeParameters`: `TSTypeParameterDeclaration` (default: `null`)
 - `typeAnnotation`: `TSType` (required)
 - `declare`: `boolean` (default: `null`)

---

### tSTypeAnnotation
```javascript
t.tSTypeAnnotation(typeAnnotation)
```

See also `t.isTSTypeAnnotation(node, opts)` and `t.assertTSTypeAnnotation(node, opts)`.

 - `typeAnnotation`: `TSType` (required)

---

### tSTypeAssertion
```javascript
t.tSTypeAssertion(typeAnnotation, expression)
```

See also `t.isTSTypeAssertion(node, opts)` and `t.assertTSTypeAssertion(node, opts)`.

Aliases: `Expression`

 - `typeAnnotation`: `TSType` (required)
 - `expression`: `Expression` (required)

---

### tSTypeLiteral
```javascript
t.tSTypeLiteral(members)
```

See also `t.isTSTypeLiteral(node, opts)` and `t.assertTSTypeLiteral(node, opts)`.

Aliases: `TSType`

 - `members`: `Array<TSTypeElement>` (required)

---

### tSTypeOperator
```javascript
t.tSTypeOperator(typeAnnotation)
```

See also `t.isTSTypeOperator(node, opts)` and `t.assertTSTypeOperator(node, opts)`.

Aliases: `TSType`

 - `typeAnnotation`: `TSType` (required)
 - `operator`: `string` (default: `null`)

---

### tSTypeParameter
```javascript
t.tSTypeParameter(constraint, default)
```

See also `t.isTSTypeParameter(node, opts)` and `t.assertTSTypeParameter(node, opts)`.

 - `constraint`: `TSType` (default: `null`)
 - `default`: `TSType` (default: `null`)
 - `name`: `string` (default: `null`)

---

### tSTypeParameterDeclaration
```javascript
t.tSTypeParameterDeclaration(params)
```

See also `t.isTSTypeParameterDeclaration(node, opts)` and `t.assertTSTypeParameterDeclaration(node, opts)`.

 - `params`: `Array<TSTypeParameter>` (required)

---

### tSTypeParameterInstantiation
```javascript
t.tSTypeParameterInstantiation(params)
```

See also `t.isTSTypeParameterInstantiation(node, opts)` and `t.assertTSTypeParameterInstantiation(node, opts)`.

 - `params`: `Array<TSType>` (required)

---

### tSTypePredicate
```javascript
t.tSTypePredicate(parameterName, typeAnnotation)
```

See also `t.isTSTypePredicate(node, opts)` and `t.assertTSTypePredicate(node, opts)`.

Aliases: `TSType`

 - `parameterName`: `Identifier | TSThisType` (required)
 - `typeAnnotation`: `TSTypeAnnotation` (required)

---

### tSTypeQuery
```javascript
t.tSTypeQuery(exprName)
```

See also `t.isTSTypeQuery(node, opts)` and `t.assertTSTypeQuery(node, opts)`.

Aliases: `TSType`

 - `exprName`: `TSEntityName` (required)

---

### tSTypeReference
```javascript
t.tSTypeReference(typeName, typeParameters)
```

See also `t.isTSTypeReference(node, opts)` and `t.assertTSTypeReference(node, opts)`.

Aliases: `TSType`

 - `typeName`: `TSEntityName` (required)
 - `typeParameters`: `TSTypeParameterInstantiation` (default: `null`)

---

### tSUndefinedKeyword
```javascript
t.tSUndefinedKeyword()
```

See also `t.isTSUndefinedKeyword(node, opts)` and `t.assertTSUndefinedKeyword(node, opts)`.

Aliases: `TSType`


---

### tSUnionType
```javascript
t.tSUnionType(types)
```

See also `t.isTSUnionType(node, opts)` and `t.assertTSUnionType(node, opts)`.

Aliases: `TSType`

 - `types`: `Array<TSType>` (required)

---

### tSVoidKeyword
```javascript
t.tSVoidKeyword()
```

See also `t.isTSVoidKeyword(node, opts)` and `t.assertTSVoidKeyword(node, opts)`.

Aliases: `TSType`


---

### taggedTemplateExpression
```javascript
t.taggedTemplateExpression(tag, quasi)
```

See also `t.isTaggedTemplateExpression(node, opts)` and `t.assertTaggedTemplateExpression(node, opts)`.

Aliases: `Expression`

 - `tag`: `Expression` (required)
 - `quasi`: `TemplateLiteral` (required)

---

### templateElement
```javascript
t.templateElement(value, tail)
```

See also `t.isTemplateElement(node, opts)` and `t.assertTemplateElement(node, opts)`.

 - `value` (required)
 - `tail`: `boolean` (default: `false`)

---

### templateLiteral
```javascript
t.templateLiteral(quasis, expressions)
```

See also `t.isTemplateLiteral(node, opts)` and `t.assertTemplateLiteral(node, opts)`.

Aliases: `Expression`, `Literal`

 - `quasis`: `Array<TemplateElement>` (required)
 - `expressions`: `Array<Expression>` (required)

---

### thisExpression
```javascript
t.thisExpression()
```

See also `t.isThisExpression(node, opts)` and `t.assertThisExpression(node, opts)`.

Aliases: `Expression`


---

### thisTypeAnnotation
```javascript
t.thisTypeAnnotation()
```

See also `t.isThisTypeAnnotation(node, opts)` and `t.assertThisTypeAnnotation(node, opts)`.

Aliases: `Flow`, `FlowBaseAnnotation`


---

### throwStatement
```javascript
t.throwStatement(argument)
```

See also `t.isThrowStatement(node, opts)` and `t.assertThrowStatement(node, opts)`.

Aliases: `Statement`, `Terminatorless`, `CompletionStatement`

 - `argument`: `Expression` (required)

---

### tryStatement
```javascript
t.tryStatement(block, handler, finalizer)
```

See also `t.isTryStatement(node, opts)` and `t.assertTryStatement(node, opts)`.

Aliases: `Statement`

 - `block`: `BlockStatement` (required)
 - `handler`: `CatchClause` (default: `null`)
 - `finalizer`: `BlockStatement` (default: `null`)

---

### tupleTypeAnnotation
```javascript
t.tupleTypeAnnotation(types)
```

See also `t.isTupleTypeAnnotation(node, opts)` and `t.assertTupleTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `types` (required)

---

### typeAlias
```javascript
t.typeAlias(id, typeParameters, right)
```

See also `t.isTypeAlias(node, opts)` and `t.assertTypeAlias(node, opts)`.

Aliases: `Flow`, `FlowDeclaration`, `Statement`, `Declaration`

 - `id` (required)
 - `typeParameters` (required)
 - `right` (required)

---

### typeAnnotation
```javascript
t.typeAnnotation(typeAnnotation)
```

See also `t.isTypeAnnotation(node, opts)` and `t.assertTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `typeAnnotation`: `Flow` (required)

---

### typeCastExpression
```javascript
t.typeCastExpression(expression, typeAnnotation)
```

See also `t.isTypeCastExpression(node, opts)` and `t.assertTypeCastExpression(node, opts)`.

Aliases: `Flow`, `ExpressionWrapper`, `Expression`

 - `expression` (required)
 - `typeAnnotation` (required)

---

### typeParameter
```javascript
t.typeParameter(bound, default)
```

See also `t.isTypeParameter(node, opts)` and `t.assertTypeParameter(node, opts)`.

Aliases: `Flow`

 - `bound`: `TypeAnnotation` (default: `null`)
 - `default`: `Flow` (default: `null`)
 - `name`: `string` (default: `null`)

---

### typeParameterDeclaration
```javascript
t.typeParameterDeclaration(params)
```

See also `t.isTypeParameterDeclaration(node, opts)` and `t.assertTypeParameterDeclaration(node, opts)`.

Aliases: `Flow`

 - `params`: `Array<TypeParameter>` (required)

---

### typeParameterInstantiation
```javascript
t.typeParameterInstantiation(params)
```

See also `t.isTypeParameterInstantiation(node, opts)` and `t.assertTypeParameterInstantiation(node, opts)`.

Aliases: `Flow`

 - `params`: `Array<Flow>` (required)

---

### typeofTypeAnnotation
```javascript
t.typeofTypeAnnotation(argument)
```

See also `t.isTypeofTypeAnnotation(node, opts)` and `t.assertTypeofTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `argument` (required)

---

### unaryExpression
```javascript
t.unaryExpression(operator, argument, prefix)
```

See also `t.isUnaryExpression(node, opts)` and `t.assertUnaryExpression(node, opts)`.

Aliases: `UnaryLike`, `Expression`

 - `operator`: `'void' | 'throw' | 'delete' | '!' | '+' | '-' | '~' | 'typeof'` (required)
 - `argument`: `Expression` (required)
 - `prefix`: `boolean` (default: `true`)

---

### unionTypeAnnotation
```javascript
t.unionTypeAnnotation(types)
```

See also `t.isUnionTypeAnnotation(node, opts)` and `t.assertUnionTypeAnnotation(node, opts)`.

Aliases: `Flow`

 - `types` (required)

---

### updateExpression
```javascript
t.updateExpression(operator, argument, prefix)
```

See also `t.isUpdateExpression(node, opts)` and `t.assertUpdateExpression(node, opts)`.

Aliases: `Expression`

 - `operator`: `'++' | '--'` (required)
 - `argument`: `Expression` (required)
 - `prefix`: `boolean` (default: `false`)

---

### variableDeclaration
```javascript
t.variableDeclaration(kind, declarations)
```

See also `t.isVariableDeclaration(node, opts)` and `t.assertVariableDeclaration(node, opts)`.

Aliases: `Statement`, `Declaration`

 - `kind`: `"var" | "let" | "const"` (required)
 - `declarations`: `Array<VariableDeclarator>` (required)
 - `declare`: `boolean` (default: `null`)

---

### variableDeclarator
```javascript
t.variableDeclarator(id, init)
```

See also `t.isVariableDeclarator(node, opts)` and `t.assertVariableDeclarator(node, opts)`.

 - `id`: `LVal` (required)
 - `init`: `Expression` (default: `null`)

---

### voidTypeAnnotation
```javascript
t.voidTypeAnnotation()
```

See also `t.isVoidTypeAnnotation(node, opts)` and `t.assertVoidTypeAnnotation(node, opts)`.

Aliases: `Flow`, `FlowBaseAnnotation`


---

### whileStatement
```javascript
t.whileStatement(test, body)
```

See also `t.isWhileStatement(node, opts)` and `t.assertWhileStatement(node, opts)`.

Aliases: `Statement`, `BlockParent`, `Loop`, `While`, `Scopable`

 - `test`: `Expression` (required)
 - `body`: `BlockStatement | Statement` (required)

---

### withStatement
```javascript
t.withStatement(object, body)
```

See also `t.isWithStatement(node, opts)` and `t.assertWithStatement(node, opts)`.

Aliases: `Statement`

 - `object`: `Expression` (required)
 - `body`: `BlockStatement | Statement` (required)

---

### yieldExpression
```javascript
t.yieldExpression(argument, delegate)
```

See also `t.isYieldExpression(node, opts)` and `t.assertYieldExpression(node, opts)`.

Aliases: `Expression`, `Terminatorless`

 - `argument`: `Expression` (default: `null`)
 - `delegate`: `boolean` (default: `false`)

---


<!-- end generated section -->

