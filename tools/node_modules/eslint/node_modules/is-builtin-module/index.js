'use strict';
const builtinModules = require('builtin-modules');

const moduleSet = new Set(builtinModules);
const NODE_PROTOCOL = 'node:';

module.exports = moduleName => {
	if (typeof moduleName !== 'string') {
		throw new TypeError('Expected a string');
	}

	if (moduleName.startsWith(NODE_PROTOCOL)) {
		moduleName = moduleName.slice(NODE_PROTOCOL.length);
	}

	const slashIndex = moduleName.indexOf('/');
	if (slashIndex !== -1 && slashIndex !== moduleName.length - 1) {
		moduleName = moduleName.slice(0, slashIndex);
	}

	return moduleSet.has(moduleName);
};
