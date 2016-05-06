var falafel = require('../');
var acorn = require('acorn-jsx');
var test = require('tape');

test('custom parser', function (t) {

  var src = '(function() { var f = {a: "b"}; var a = <div {...f} className="test"></div>; })()';

  var nodeTypes = [
    'Identifier',
    'Identifier',
    'Literal',
    'Property',
    'ObjectExpression',
    'VariableDeclarator',
    'VariableDeclaration',
    'Identifier',
    'Identifier',
    'JSXSpreadAttribute',
    'JSXIdentifier',
    'Literal',
    'JSXAttribute',
    'JSXIdentifier',
    'JSXOpeningElement',
    'JSXIdentifier',
    'JSXClosingElement',
    'JSXElement',
    'VariableDeclarator',
    'VariableDeclaration',
    'BlockStatement',
    'FunctionExpression',
    'CallExpression',
    'ExpressionStatement',
    'Program'
  ];

  t.plan(nodeTypes.length);

  var output = falafel(src, {parser: acorn, ecmaVersion: 6, plugins: { jsx: true }}, function(node) {
    t.equal(node.type, nodeTypes.shift());
  });
});
