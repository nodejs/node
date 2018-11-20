var assert = require('chai').assert;
var parse = require('esprima-fb').parse;
var originalKeys = require('./keys');

describe('works', function () {
	var code = ['class MyClass{',
		'x: number;',
		'y: number;',
		'constructor(x: number, y: number){',
			'this.x = x;',
			'this.y = y;',
		'}',
		'render(){',
			'return <namespace:tag textAttr="value" exprAttr={expr} {...spreadAttr}><object.prop>!</object.prop>{}</namespace:tag>',
		'}',
	'}'].join('\n');

	var ast = parse(code);

	var expectedKeys = [
		'ClassProperty',
		'TypeAnnotation',
		'NumberTypeAnnotation',
		'ClassProperty',
		'TypeAnnotation',
		'NumberTypeAnnotation',
		'XJSElement',
		'XJSOpeningElement',
		'XJSNamespacedName',
		'XJSIdentifier',
		'XJSIdentifier',
		'XJSAttribute',
		'XJSIdentifier',
		'XJSAttribute',
		'XJSIdentifier',
		'XJSExpressionContainer',
		'XJSSpreadAttribute',
		'XJSClosingElement',
		'XJSNamespacedName',
		'XJSIdentifier',
		'XJSIdentifier',
		'XJSElement',
		'XJSOpeningElement',
		'XJSMemberExpression',
		'XJSIdentifier',
		'XJSIdentifier',
		'XJSClosingElement',
		'XJSMemberExpression',
		'XJSIdentifier',
		'XJSIdentifier',
		'XJSExpressionContainer',
		'XJSEmptyExpression'
	];

	beforeEach(function () {
		for (var key in require.cache) {
			delete require.cache[key];
		}
	});

	it('directly from dependency', function () {
		var traverse = require('./').traverse;
		var actualKeys = [];
		var actualTypeKeys = [];

		traverse(ast, {
			enter: function (node) {
				if (originalKeys[node.type] != null) {
					actualKeys.push(node.type);
				}
			}
		});

		assert.deepEqual(actualKeys, expectedKeys);
	});

	it('in injected mode', function () {
		require('./');
		var traverse = require('estraverse').traverse;
		var actualKeys = [];

		traverse(ast, {
			enter: function (node) {
				if (originalKeys[node.type] != null) {
					actualKeys.push(node.type);
				}
			}
		});

		assert.deepEqual(actualKeys, expectedKeys);
	});

	it('in single-pass mode', function () {
		var traverse = require('estraverse').traverse;
		var keys = require('./keys');

		var actualKeys = [];

		traverse(ast, {
			enter: function (node) {
				if (originalKeys[node.type] != null) {
					actualKeys.push(node.type);
				}
			},
			keys: keys
		});

		assert.throws(function () {
			traverse(ast, {
				enter: function () {}
			});
		});

		assert.deepEqual(actualKeys, expectedKeys);
	});
});
