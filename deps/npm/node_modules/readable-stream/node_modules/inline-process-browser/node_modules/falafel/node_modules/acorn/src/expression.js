// A recursive descent parser operates by defining functions for all
// syntactic elements, and recursively calling those, each function
// advancing the input stream and returning an AST node. Precedence
// of constructs (for example, the fact that `!x[1]` means `!(x[1])`
// instead of `(!x)[1]` is handled by the fact that the parser
// function that parses unary prefix operators is called first, and
// in turn calls the function that parses `[]` subscripts — that
// way, it'll receive the node for `x[1]` already parsed, and wraps
// *that* in the unary operator node.
//
// Acorn uses an [operator precedence parser][opp] to handle binary
// operator precedence, because it is much more compact than using
// the technique outlined above, which uses different, nesting
// functions to specify precedence, for all of the ten binary
// precedence levels that JavaScript defines.
//
// [opp]: http://en.wikipedia.org/wiki/Operator-precedence_parser

import {types as tt} from "./tokentype"
import {Parser} from "./state"
import {reservedWords} from "./identifier"
import {has} from "./util"

const pp = Parser.prototype

// Check if property name clashes with already added.
// Object/class getters and setters are not allowed to clash —
// either with each other or with an init property — and in
// strict mode, init properties are also not allowed to be repeated.

pp.checkPropClash = function(prop, propHash) {
  if (this.options.ecmaVersion >= 6) return
  let key = prop.key, name
  switch (key.type) {
  case "Identifier": name = key.name; break
  case "Literal": name = String(key.value); break
  default: return
  }
  let kind = prop.kind || "init", other
  if (has(propHash, name)) {
    other = propHash[name]
    let isGetSet = kind !== "init"
    if ((this.strict || isGetSet) && other[kind] || !(isGetSet ^ other.init))
      this.raise(key.start, "Redefinition of property")
  } else {
    other = propHash[name] = {
      init: false,
      get: false,
      set: false
    }
  }
  other[kind] = true
}

// ### Expression parsing

// These nest, from the most general expression type at the top to
// 'atomic', nondivisible expression types at the bottom. Most of
// the functions will simply let the function(s) below them parse,
// and, *if* the syntactic construct they handle is present, wrap
// the AST node that the inner parser gave them in another node.

// Parse a full expression. The optional arguments are used to
// forbid the `in` operator (in for loops initalization expressions)
// and provide reference for storing '=' operator inside shorthand
// property assignment in contexts where both object expression
// and object pattern might appear (so it's possible to raise
// delayed syntax error at correct position).

pp.parseExpression = function(noIn, refShorthandDefaultPos) {
  let startPos = this.start, startLoc = this.startLoc
  let expr = this.parseMaybeAssign(noIn, refShorthandDefaultPos)
  if (this.type === tt.comma) {
    let node = this.startNodeAt(startPos, startLoc)
    node.expressions = [expr]
    while (this.eat(tt.comma)) node.expressions.push(this.parseMaybeAssign(noIn, refShorthandDefaultPos))
    return this.finishNode(node, "SequenceExpression")
  }
  return expr
}

// Parse an assignment expression. This includes applications of
// operators like `+=`.

pp.parseMaybeAssign = function(noIn, refShorthandDefaultPos, afterLeftParse) {
  if (this.type == tt._yield && this.inGenerator) return this.parseYield()

  let failOnShorthandAssign
  if (!refShorthandDefaultPos) {
    refShorthandDefaultPos = {start: 0}
    failOnShorthandAssign = true
  } else {
    failOnShorthandAssign = false
  }
  let startPos = this.start, startLoc = this.startLoc
  if (this.type == tt.parenL || this.type == tt.name)
    this.potentialArrowAt = this.start
  let left = this.parseMaybeConditional(noIn, refShorthandDefaultPos)
  if (afterLeftParse) left = afterLeftParse.call(this, left, startPos, startLoc)
  if (this.type.isAssign) {
    let node = this.startNodeAt(startPos, startLoc)
    node.operator = this.value
    node.left = this.type === tt.eq ? this.toAssignable(left) : left
    refShorthandDefaultPos.start = 0 // reset because shorthand default was used correctly
    this.checkLVal(left)
    this.next()
    node.right = this.parseMaybeAssign(noIn)
    return this.finishNode(node, "AssignmentExpression")
  } else if (failOnShorthandAssign && refShorthandDefaultPos.start) {
    this.unexpected(refShorthandDefaultPos.start)
  }
  return left
}

// Parse a ternary conditional (`?:`) operator.

pp.parseMaybeConditional = function(noIn, refShorthandDefaultPos) {
  let startPos = this.start, startLoc = this.startLoc
  let expr = this.parseExprOps(noIn, refShorthandDefaultPos)
  if (refShorthandDefaultPos && refShorthandDefaultPos.start) return expr
  if (this.eat(tt.question)) {
    let node = this.startNodeAt(startPos, startLoc)
    node.test = expr
    node.consequent = this.parseMaybeAssign()
    this.expect(tt.colon)
    node.alternate = this.parseMaybeAssign(noIn)
    return this.finishNode(node, "ConditionalExpression")
  }
  return expr
}

// Start the precedence parser.

pp.parseExprOps = function(noIn, refShorthandDefaultPos) {
  let startPos = this.start, startLoc = this.startLoc
  let expr = this.parseMaybeUnary(refShorthandDefaultPos)
  if (refShorthandDefaultPos && refShorthandDefaultPos.start) return expr
  return this.parseExprOp(expr, startPos, startLoc, -1, noIn)
}

// Parse binary operators with the operator precedence parsing
// algorithm. `left` is the left-hand side of the operator.
// `minPrec` provides context that allows the function to stop and
// defer further parser to one of its callers when it encounters an
// operator that has a lower precedence than the set it is parsing.

pp.parseExprOp = function(left, leftStartPos, leftStartLoc, minPrec, noIn) {
  let prec = this.type.binop
  if (Array.isArray(leftStartPos)){
    if (this.options.locations && noIn === undefined) {
      // shift arguments to left by one
      noIn = minPrec
      minPrec = leftStartLoc
      // flatten leftStartPos
      leftStartLoc = leftStartPos[1]
      leftStartPos = leftStartPos[0]
    }
  }
  if (prec != null && (!noIn || this.type !== tt._in)) {
    if (prec > minPrec) {
      let node = this.startNodeAt(leftStartPos, leftStartLoc)
      node.left = left
      node.operator = this.value
      let op = this.type
      this.next()
      let startPos = this.start, startLoc = this.startLoc
      node.right = this.parseExprOp(this.parseMaybeUnary(), startPos, startLoc, prec, noIn)
      this.finishNode(node, (op === tt.logicalOR || op === tt.logicalAND) ? "LogicalExpression" : "BinaryExpression")
      return this.parseExprOp(node, leftStartPos, leftStartLoc, minPrec, noIn)
    }
  }
  return left
}

// Parse unary operators, both prefix and postfix.

pp.parseMaybeUnary = function(refShorthandDefaultPos) {
  if (this.type.prefix) {
    let node = this.startNode(), update = this.type === tt.incDec
    node.operator = this.value
    node.prefix = true
    this.next()
    node.argument = this.parseMaybeUnary()
    if (refShorthandDefaultPos && refShorthandDefaultPos.start) this.unexpected(refShorthandDefaultPos.start)
    if (update) this.checkLVal(node.argument)
    else if (this.strict && node.operator === "delete" &&
             node.argument.type === "Identifier")
      this.raise(node.start, "Deleting local variable in strict mode")
    return this.finishNode(node, update ? "UpdateExpression" : "UnaryExpression")
  }
  let startPos = this.start, startLoc = this.startLoc
  let expr = this.parseExprSubscripts(refShorthandDefaultPos)
  if (refShorthandDefaultPos && refShorthandDefaultPos.start) return expr
  while (this.type.postfix && !this.canInsertSemicolon()) {
    let node = this.startNodeAt(startPos, startLoc)
    node.operator = this.value
    node.prefix = false
    node.argument = expr
    this.checkLVal(expr)
    this.next()
    expr = this.finishNode(node, "UpdateExpression")
  }
  return expr
}

// Parse call, dot, and `[]`-subscript expressions.

pp.parseExprSubscripts = function(refShorthandDefaultPos) {
  let startPos = this.start, startLoc = this.startLoc
  let expr = this.parseExprAtom(refShorthandDefaultPos)
  if (refShorthandDefaultPos && refShorthandDefaultPos.start) return expr
  return this.parseSubscripts(expr, startPos, startLoc)
}

pp.parseSubscripts = function(base, startPos, startLoc, noCalls) {
  if (Array.isArray(startPos)){
    if (this.options.locations && noCalls === undefined) {
      // shift arguments to left by one
      noCalls = startLoc
      // flatten startPos
      startLoc = startPos[1]
      startPos = startPos[0]
    }
  }
  for (;;) {
    if (this.eat(tt.dot)) {
      let node = this.startNodeAt(startPos, startLoc)
      node.object = base
      node.property = this.parseIdent(true)
      node.computed = false
      base = this.finishNode(node, "MemberExpression")
    } else if (this.eat(tt.bracketL)) {
      let node = this.startNodeAt(startPos, startLoc)
      node.object = base
      node.property = this.parseExpression()
      node.computed = true
      this.expect(tt.bracketR)
      base = this.finishNode(node, "MemberExpression")
    } else if (!noCalls && this.eat(tt.parenL)) {
      let node = this.startNodeAt(startPos, startLoc)
      node.callee = base
      node.arguments = this.parseExprList(tt.parenR, false)
      base = this.finishNode(node, "CallExpression")
    } else if (this.type === tt.backQuote) {
      let node = this.startNodeAt(startPos, startLoc)
      node.tag = base
      node.quasi = this.parseTemplate()
      base = this.finishNode(node, "TaggedTemplateExpression")
    } else {
      return base
    }
  }
}

// Parse an atomic expression — either a single token that is an
// expression, an expression started by a keyword like `function` or
// `new`, or an expression wrapped in punctuation like `()`, `[]`,
// or `{}`.

pp.parseExprAtom = function(refShorthandDefaultPos) {
  let node, canBeArrow = this.potentialArrowAt == this.start
  switch (this.type) {
  case tt._this:
  case tt._super:
    let type = this.type === tt._this ? "ThisExpression" : "Super"
    node = this.startNode()
    this.next()
    return this.finishNode(node, type)

  case tt._yield:
    if (this.inGenerator) this.unexpected()

  case tt.name:
    let startPos = this.start, startLoc = this.startLoc
    let id = this.parseIdent(this.type !== tt.name)
    if (canBeArrow && !this.canInsertSemicolon() && this.eat(tt.arrow))
      return this.parseArrowExpression(this.startNodeAt(startPos, startLoc), [id])
    return id

  case tt.regexp:
    let value = this.value
    node = this.parseLiteral(value.value)
    node.regex = {pattern: value.pattern, flags: value.flags}
    return node

  case tt.num: case tt.string:
    return this.parseLiteral(this.value)

  case tt._null: case tt._true: case tt._false:
    node = this.startNode()
    node.value = this.type === tt._null ? null : this.type === tt._true
    node.raw = this.type.keyword
    this.next()
    return this.finishNode(node, "Literal")

  case tt.parenL:
    return this.parseParenAndDistinguishExpression(canBeArrow)

  case tt.bracketL:
    node = this.startNode()
    this.next()
    // check whether this is array comprehension or regular array
    if (this.options.ecmaVersion >= 7 && this.type === tt._for) {
      return this.parseComprehension(node, false)
    }
    node.elements = this.parseExprList(tt.bracketR, true, true, refShorthandDefaultPos)
    return this.finishNode(node, "ArrayExpression")

  case tt.braceL:
    return this.parseObj(false, refShorthandDefaultPos)

  case tt._function:
    node = this.startNode()
    this.next()
    return this.parseFunction(node, false)

  case tt._class:
    return this.parseClass(this.startNode(), false)

  case tt._new:
    return this.parseNew()

  case tt.backQuote:
    return this.parseTemplate()

  default:
    this.unexpected()
  }
}

pp.parseLiteral = function(value) {
  let node = this.startNode()
  node.value = value
  node.raw = this.input.slice(this.start, this.end)
  this.next()
  return this.finishNode(node, "Literal")
}

pp.parseParenExpression = function() {
  this.expect(tt.parenL)
  let val = this.parseExpression()
  this.expect(tt.parenR)
  return val
}

pp.parseParenAndDistinguishExpression = function(canBeArrow) {
  let startPos = this.start, startLoc = this.startLoc, val
  if (this.options.ecmaVersion >= 6) {
    this.next()

    if (this.options.ecmaVersion >= 7 && this.type === tt._for) {
      return this.parseComprehension(this.startNodeAt(startPos, startLoc), true)
    }

    let innerStartPos = this.start, innerStartLoc = this.startLoc
    let exprList = [], first = true
    let refShorthandDefaultPos = {start: 0}, spreadStart, innerParenStart
    while (this.type !== tt.parenR) {
      first ? first = false : this.expect(tt.comma)
      if (this.type === tt.ellipsis) {
        spreadStart = this.start
        exprList.push(this.parseParenItem(this.parseRest()))
        break
      } else {
        if (this.type === tt.parenL && !innerParenStart) {
          innerParenStart = this.start
        }
        exprList.push(this.parseMaybeAssign(false, refShorthandDefaultPos, this.parseParenItem))
      }
    }
    let innerEndPos = this.start, innerEndLoc = this.startLoc
    this.expect(tt.parenR)

    if (canBeArrow && !this.canInsertSemicolon() && this.eat(tt.arrow)) {
      if (innerParenStart) this.unexpected(innerParenStart)
        return this.parseParenArrowList(startPos, startLoc, exprList)
    }

    if (!exprList.length) this.unexpected(this.lastTokStart)
    if (spreadStart) this.unexpected(spreadStart)
    if (refShorthandDefaultPos.start) this.unexpected(refShorthandDefaultPos.start)

    if (exprList.length > 1) {
      val = this.startNodeAt(innerStartPos, innerStartLoc)
      val.expressions = exprList
      this.finishNodeAt(val, "SequenceExpression", innerEndPos, innerEndLoc)
    } else {
      val = exprList[0]
    }
  } else {
    val = this.parseParenExpression()
  }

  if (this.options.preserveParens) {
    let par = this.startNodeAt(startPos, startLoc)
    par.expression = val
    return this.finishNode(par, "ParenthesizedExpression")
  } else {
    return val
  }
}

pp.parseParenItem = function(item) {
  return item
}

pp.parseParenArrowList = function(startPos, startLoc, exprList) {
  return this.parseArrowExpression(this.startNodeAt(startPos, startLoc), exprList)
}

// New's precedence is slightly tricky. It must allow its argument
// to be a `[]` or dot subscript expression, but not a call — at
// least, not without wrapping it in parentheses. Thus, it uses the

const empty = []

pp.parseNew = function() {
  let node = this.startNode()
  let meta = this.parseIdent(true)
  if (this.options.ecmaVersion >= 6 && this.eat(tt.dot)) {
    node.meta = meta
    node.property = this.parseIdent(true)
    if (node.property.name !== "target")
      this.raise(node.property.start, "The only valid meta property for new is new.target")
    return this.finishNode(node, "MetaProperty")
  }
  let startPos = this.start, startLoc = this.startLoc
  node.callee = this.parseSubscripts(this.parseExprAtom(), startPos, startLoc, true)
  if (this.eat(tt.parenL)) node.arguments = this.parseExprList(tt.parenR, false)
  else node.arguments = empty
  return this.finishNode(node, "NewExpression")
}

// Parse template expression.

pp.parseTemplateElement = function() {
  let elem = this.startNode()
  elem.value = {
    raw: this.input.slice(this.start, this.end),
    cooked: this.value
  }
  this.next()
  elem.tail = this.type === tt.backQuote
  return this.finishNode(elem, "TemplateElement")
}

pp.parseTemplate = function() {
  let node = this.startNode()
  this.next()
  node.expressions = []
  let curElt = this.parseTemplateElement()
  node.quasis = [curElt]
  while (!curElt.tail) {
    this.expect(tt.dollarBraceL)
    node.expressions.push(this.parseExpression())
    this.expect(tt.braceR)
    node.quasis.push(curElt = this.parseTemplateElement())
  }
  this.next()
  return this.finishNode(node, "TemplateLiteral")
}

// Parse an object literal or binding pattern.

pp.parseObj = function(isPattern, refShorthandDefaultPos) {
  let node = this.startNode(), first = true, propHash = {}
  node.properties = []
  this.next()
  while (!this.eat(tt.braceR)) {
    if (!first) {
      this.expect(tt.comma)
      if (this.afterTrailingComma(tt.braceR)) break
    } else first = false

    let prop = this.startNode(), isGenerator, startPos, startLoc
    if (this.options.ecmaVersion >= 6) {
      prop.method = false
      prop.shorthand = false
      if (isPattern || refShorthandDefaultPos) {
        startPos = this.start
        startLoc = this.startLoc
      }
      if (!isPattern)
        isGenerator = this.eat(tt.star)
    }
    this.parsePropertyName(prop)
    this.parsePropertyValue(prop, isPattern, isGenerator, startPos, startLoc, refShorthandDefaultPos)
    this.checkPropClash(prop, propHash)
    node.properties.push(this.finishNode(prop, "Property"))
  }
  return this.finishNode(node, isPattern ? "ObjectPattern" : "ObjectExpression")
}

pp.parsePropertyValue = function(prop, isPattern, isGenerator, startPos, startLoc, refShorthandDefaultPos) {
  if (this.eat(tt.colon)) {
      prop.value = isPattern ? this.parseMaybeDefault(this.start, this.startLoc) : this.parseMaybeAssign(false, refShorthandDefaultPos)
      prop.kind = "init"
    } else if (this.options.ecmaVersion >= 6 && this.type === tt.parenL) {
      if (isPattern) this.unexpected()
      prop.kind = "init"
      prop.method = true
      prop.value = this.parseMethod(isGenerator)
    } else if (this.options.ecmaVersion >= 5 && !prop.computed && prop.key.type === "Identifier" &&
               (prop.key.name === "get" || prop.key.name === "set") &&
               (this.type != tt.comma && this.type != tt.braceR)) {
      if (isGenerator || isPattern) this.unexpected()
      prop.kind = prop.key.name
      this.parsePropertyName(prop)
      prop.value = this.parseMethod(false)
    } else if (this.options.ecmaVersion >= 6 && !prop.computed && prop.key.type === "Identifier") {
      prop.kind = "init"
      if (isPattern) {
        if (this.isKeyword(prop.key.name) ||
            (this.strict && (reservedWords.strictBind(prop.key.name) || reservedWords.strict(prop.key.name))) ||
            (!this.options.allowReserved && this.isReservedWord(prop.key.name)))
          this.raise(prop.key.start, "Binding " + prop.key.name)
        prop.value = this.parseMaybeDefault(startPos, startLoc, prop.key)
      } else if (this.type === tt.eq && refShorthandDefaultPos) {
        if (!refShorthandDefaultPos.start)
          refShorthandDefaultPos.start = this.start
        prop.value = this.parseMaybeDefault(startPos, startLoc, prop.key)
      } else {
        prop.value = prop.key
      }
      prop.shorthand = true
    } else this.unexpected()
}

pp.parsePropertyName = function(prop) {
  if (this.options.ecmaVersion >= 6) {
    if (this.eat(tt.bracketL)) {
      prop.computed = true
      prop.key = this.parseMaybeAssign()
      this.expect(tt.bracketR)
      return prop.key
    } else {
      prop.computed = false
    }
  }
  return prop.key = (this.type === tt.num || this.type === tt.string) ? this.parseExprAtom() : this.parseIdent(true)
}

// Initialize empty function node.

pp.initFunction = function(node) {
  node.id = null
  if (this.options.ecmaVersion >= 6) {
    node.generator = false
    node.expression = false
  }
}

// Parse object or class method.

pp.parseMethod = function(isGenerator) {
  let node = this.startNode()
  this.initFunction(node)
  this.expect(tt.parenL)
  node.params = this.parseBindingList(tt.parenR, false, false)
  let allowExpressionBody
  if (this.options.ecmaVersion >= 6) {
    node.generator = isGenerator
    allowExpressionBody = true
  } else {
    allowExpressionBody = false
  }
  this.parseFunctionBody(node, allowExpressionBody)
  return this.finishNode(node, "FunctionExpression")
}

// Parse arrow function expression with given parameters.

pp.parseArrowExpression = function(node, params) {
  this.initFunction(node)
  node.params = this.toAssignableList(params, true)
  this.parseFunctionBody(node, true)
  return this.finishNode(node, "ArrowFunctionExpression")
}

// Parse function body and check parameters.

pp.parseFunctionBody = function(node, allowExpression) {
  let isExpression = allowExpression && this.type !== tt.braceL

  if (isExpression) {
    node.body = this.parseMaybeAssign()
    node.expression = true
  } else {
    // Start a new scope with regard to labels and the `inFunction`
    // flag (restore them to their old value afterwards).
    let oldInFunc = this.inFunction, oldInGen = this.inGenerator, oldLabels = this.labels
    this.inFunction = true; this.inGenerator = node.generator; this.labels = []
    node.body = this.parseBlock(true)
    node.expression = false
    this.inFunction = oldInFunc; this.inGenerator = oldInGen; this.labels = oldLabels
  }

  // If this is a strict mode function, verify that argument names
  // are not repeated, and it does not try to bind the words `eval`
  // or `arguments`.
  if (this.strict || !isExpression && node.body.body.length && this.isUseStrict(node.body.body[0])) {
    let nameHash = {}, oldStrict = this.strict
    this.strict = true
    if (node.id)
      this.checkLVal(node.id, true)
    for (let i = 0; i < node.params.length; i++)
      this.checkLVal(node.params[i], true, nameHash)
    this.strict = oldStrict
  }
}

// Parses a comma-separated list of expressions, and returns them as
// an array. `close` is the token type that ends the list, and
// `allowEmpty` can be turned on to allow subsequent commas with
// nothing in between them to be parsed as `null` (which is needed
// for array literals).

pp.parseExprList = function(close, allowTrailingComma, allowEmpty, refShorthandDefaultPos) {
  let elts = [], first = true
  while (!this.eat(close)) {
    if (!first) {
      this.expect(tt.comma)
      if (allowTrailingComma && this.afterTrailingComma(close)) break
    } else first = false

    if (allowEmpty && this.type === tt.comma) {
      elts.push(null)
    } else {
      if (this.type === tt.ellipsis)
        elts.push(this.parseSpread(refShorthandDefaultPos))
      else
        elts.push(this.parseMaybeAssign(false, refShorthandDefaultPos))
    }
  }
  return elts
}

// Parse the next token as an identifier. If `liberal` is true (used
// when parsing properties), it will also convert keywords into
// identifiers.

pp.parseIdent = function(liberal) {
  let node = this.startNode()
  if (liberal && this.options.allowReserved == "never") liberal = false
  if (this.type === tt.name) {
    if (!liberal &&
        ((!this.options.allowReserved && this.isReservedWord(this.value)) ||
         (this.strict && reservedWords.strict(this.value)) &&
         (this.options.ecmaVersion >= 6 ||
          this.input.slice(this.start, this.end).indexOf("\\") == -1)))
      this.raise(this.start, "The keyword '" + this.value + "' is reserved")
    node.name = this.value
  } else if (liberal && this.type.keyword) {
    node.name = this.type.keyword
  } else {
    this.unexpected()
  }
  this.next()
  return this.finishNode(node, "Identifier")
}

// Parses yield expression inside generator.

pp.parseYield = function() {
  let node = this.startNode()
  this.next()
  if (this.type == tt.semi || this.canInsertSemicolon() || (this.type != tt.star && !this.type.startsExpr)) {
    node.delegate = false
    node.argument = null
  } else {
    node.delegate = this.eat(tt.star)
    node.argument = this.parseMaybeAssign()
  }
  return this.finishNode(node, "YieldExpression")
}

// Parses array and generator comprehensions.

pp.parseComprehension = function(node, isGenerator) {
  node.blocks = []
  while (this.type === tt._for) {
    let block = this.startNode()
    this.next()
    this.expect(tt.parenL)
    block.left = this.parseBindingAtom()
    this.checkLVal(block.left, true)
    this.expectContextual("of")
    block.right = this.parseExpression()
    this.expect(tt.parenR)
    node.blocks.push(this.finishNode(block, "ComprehensionBlock"))
  }
  node.filter = this.eat(tt._if) ? this.parseParenExpression() : null
  node.body = this.parseExpression()
  this.expect(isGenerator ? tt.parenR : tt.bracketR)
  node.generator = isGenerator
  return this.finishNode(node, "ComprehensionExpression")
}
