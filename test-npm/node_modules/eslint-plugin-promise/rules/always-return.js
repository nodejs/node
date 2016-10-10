function isFunctionWithBlockStatement (node) {
  if (node.type === 'FunctionExpression') {
    return true
  }
  if (node.type === 'ArrowFunctionExpression') {
    return node.body.type === 'BlockStatement'
  }
  return false
}

function isReturnOrThrowStatement (node) {
  return node.type === 'ReturnStatement' || node.type === 'ThrowStatement'
}

module.exports = function (context) {
  return {
    MemberExpression: function (node) {
      var firstArg, body, lastStatement

      if (node.property.name !== 'then' || node.parent.type !== 'CallExpression') {
        return
      }

      firstArg = node.parent.arguments[0]
      if (!firstArg || !isFunctionWithBlockStatement(firstArg)) {
        return
      }

      body = firstArg.body.body
      lastStatement = body[body.length - 1]
      if (!lastStatement || !isReturnOrThrowStatement(lastStatement)) {
        context.report(node, 'Each then() should return a value or throw')
      }
    }
  }
}
