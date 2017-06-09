import {Parser} from "./state"
import {has} from "./util"

const pp = Parser.prototype

// Object.assign polyfill
const assign = Object.assign || function(target, ...sources) {
  for (let i = 0; i < sources.length; i++) {
    const source = sources[i]
    for (const key in source) {
      if (has(source, key)) {
        target[key] = source[key]
      }
    }
  }
  return target
}

// The functions in this module keep track of declared variables in the current scope in order to detect duplicate variable names.

pp.enterFunctionScope = function() {
  // var: a hash of var-declared names in the current lexical scope
  // lexical: a hash of lexically-declared names in the current lexical scope
  // childVar: a hash of var-declared names in all child lexical scopes of the current lexical scope (within the current function scope)
  // parentLexical: a hash of lexically-declared names in all parent lexical scopes of the current lexical scope (within the current function scope)
  this.scopeStack.push({var: {}, lexical: {}, childVar: {}, parentLexical: {}})
}

pp.exitFunctionScope = function() {
  this.scopeStack.pop()
}

pp.enterLexicalScope = function() {
  const parentScope = this.scopeStack[this.scopeStack.length - 1]
  const childScope = {var: {}, lexical: {}, childVar: {}, parentLexical: {}}

  this.scopeStack.push(childScope)
  assign(childScope.parentLexical, parentScope.lexical, parentScope.parentLexical)
}

pp.exitLexicalScope = function() {
  const childScope = this.scopeStack.pop()
  const parentScope = this.scopeStack[this.scopeStack.length - 1]

  assign(parentScope.childVar, childScope.var, childScope.childVar)
}

/**
 * A name can be declared with `var` if there are no variables with the same name declared with `let`/`const`
 * in the current lexical scope or any of the parent lexical scopes in this function.
 */
pp.canDeclareVarName = function(name) {
  const currentScope = this.scopeStack[this.scopeStack.length - 1]

  return !has(currentScope.lexical, name) && !has(currentScope.parentLexical, name)
}

/**
 * A name can be declared with `let`/`const` if there are no variables with the same name declared with `let`/`const`
 * in the current scope, and there are no variables with the same name declared with `var` in the current scope or in
 * any child lexical scopes in this function.
 */
pp.canDeclareLexicalName = function(name) {
  const currentScope = this.scopeStack[this.scopeStack.length - 1]

  return !has(currentScope.lexical, name) && !has(currentScope.var, name) && !has(currentScope.childVar, name)
}

pp.declareVarName = function(name) {
  this.scopeStack[this.scopeStack.length - 1].var[name] = true
}

pp.declareLexicalName = function(name) {
  this.scopeStack[this.scopeStack.length - 1].lexical[name] = true
}
