var estraverse = module.exports = require('estraverse');

var VisitorKeys = require('./keys');

for (var nodeType in VisitorKeys) {
	estraverse.Syntax[nodeType] = nodeType;

	var keys = VisitorKeys[nodeType];

	if (keys) {
		estraverse.VisitorKeys[nodeType] = keys;
	}
}