module.exports = function (context) {
  return {
    ExpressionStatement: function (node) {
      // hello.then()
      if (node.expression.type === 'CallExpression' &&
        node.expression.callee.type === 'MemberExpression' &&
        node.expression.callee.property.name === 'then'
      ) {
        context.report(node, 'You should always catch() a then()')
        return
      }

      // hello.then().then().catch()
      if (node.expression.type === 'CallExpression' &&
        node.expression.callee.type === 'MemberExpression' &&
        node.expression.callee.object.type === 'CallExpression' &&
        node.expression.callee.object.callee.type === 'MemberExpression' &&
        node.expression.callee.object.callee.property.name === 'then'
      ) {
        if (node.expression.callee.property.name !== 'catch') {
          context.report(node, 'You should always catch() a then()')
        }
      }
    }
  }
}
