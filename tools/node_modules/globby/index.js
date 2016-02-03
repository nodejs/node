'use strict';
var Promise = require('pinkie-promise');
var arrayUnion = require('array-union');
var objectAssign = require('object-assign');
var glob = require('glob');
var arrify = require('arrify');
var pify = require('pify');

function sortPatterns(patterns) {
	patterns = arrify(patterns);

	var positives = [];
	var negatives = [];

	patterns.forEach(function (pattern, index) {
		var isNegative = pattern[0] === '!';
		(isNegative ? negatives : positives).push({
			index: index,
			pattern: isNegative ? pattern.slice(1) : pattern
		});
	});

	return {
		positives: positives,
		negatives: negatives
	};
}

function setIgnore(opts, negatives, positiveIndex) {
	opts = objectAssign({}, opts);

	var negativePatterns = negatives.filter(function (negative) {
		return negative.index > positiveIndex;
	}).map(function (negative) {
		return negative.pattern;
	});

	opts.ignore = (opts.ignore || []).concat(negativePatterns);
	return opts;
}

module.exports = function (patterns, opts) {
	var sortedPatterns = sortPatterns(patterns);
	opts = opts || {};

	if (sortedPatterns.positives.length === 0) {
		return Promise.resolve([]);
	}

	return Promise.all(sortedPatterns.positives.map(function (positive) {
		var globOpts = setIgnore(opts, sortedPatterns.negatives, positive.index);
		return pify(glob, Promise)(positive.pattern, globOpts);
	})).then(function (paths) {
		return arrayUnion.apply(null, paths);
	});
};

module.exports.sync = function (patterns, opts) {
	var sortedPatterns = sortPatterns(patterns);

	if (sortedPatterns.positives.length === 0) {
		return [];
	}

	return sortedPatterns.positives.reduce(function (ret, positive) {
		return arrayUnion(ret, glob.sync(positive.pattern, setIgnore(opts, sortedPatterns.negatives, positive.index)));
	}, []);
};
