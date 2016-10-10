module.exports = function (context) {
  var options = context.options[0] || {}
  var allowThen = options.allowThen
  var terminationMethod = options.terminationMethod || 'catch'
  var STATIC_METHODS = [
    'all',
    'race',
    'reject',
    'resolve'
  ]
  function isPromise (expression) {
    return ( // hello.then()
      expression.type === 'CallExpression' &&
      expression.callee.type === 'MemberExpression' &&
      expression.callee.property.name === 'then'
    ) || ( // hello.catch()
      expression.type === 'CallExpression' &&
      expression.callee.type === 'MemberExpression' &&
      expression.callee.property.name === 'catch'
    ) || ( // somePromise.ANYTHING()
      expression.type === 'CallExpression' &&
      expression.callee.type === 'MemberExpression' &&
      isPromise(expression.callee.object)
    ) || ( // Promise.STATIC_METHOD()
      expression.type === 'CallExpression' &&
      expression.callee.type === 'MemberExpression' &&
      expression.callee.object.type === 'Identifier' &&
      expression.callee.object.name === 'Promise' &&
      STATIC_METHODS.indexOf(expression.callee.property.name) !== -1
    )
  }
  return {
    ExpressionStatement: function (node) {
      if (!isPromise(node.expression)) {
        return
      }

      // somePromise.then(a, b)
      if (allowThen &&
        node.expression.type === 'CallExpression' &&
        node.expression.callee.type === 'MemberExpression' &&
        node.expression.callee.property.name === 'then' &&
        node.expression.arguments.length === 2
      ) {
        return
      }

      // somePromise.catch()
      if (node.expression.type === 'CallExpression' &&
        node.expression.callee.type === 'MemberExpression' &&
        node.expression.callee.property.name === terminationMethod
      ) {
        return
      }
      context.report(node, 'Expected ' + terminationMethod + '() or return')
    }
  }
}
