'use strict';

var test = require('tape');
var keys = require('object-keys');
var semver = require('semver');
var isCore = require('../');
var data = require('../core.json');

var supportsNodePrefix = semver.satisfies(process.versions.node, '>= 16', { includePrerelease: true });

test('core modules', function (t) {
	t.test('isCore()', function (st) {
		st.ok(isCore('fs'));
		st.ok(isCore('net'));
		st.ok(isCore('http'));

		st.ok(!isCore('seq'));
		st.ok(!isCore('../'));

		st.ok(!isCore('toString'));

		st.end();
	});

	t.test('core list', function (st) {
		var cores = keys(data);
		st.plan(cores.length);

		for (var i = 0; i < cores.length; ++i) {
			var mod = cores[i];
			var requireFunc = function () { require(mod); }; // eslint-disable-line no-loop-func
			if (isCore(mod)) {
				st.doesNotThrow(requireFunc, mod + ' supported; requiring does not throw');
			} else {
				st['throws'](requireFunc, mod + ' not supported; requiring throws');
			}
		}

		st.end();
	});

	t.test('core via repl module', { skip: !data.repl }, function (st) {
		var libs = require('repl')._builtinLibs; // eslint-disable-line no-underscore-dangle
		if (!libs) {
			st.skip('module.builtinModules does not exist');
		} else {
			for (var i = 0; i < libs.length; ++i) {
				var mod = libs[i];
				st.ok(data[mod], mod + ' is a core module');
				st.doesNotThrow(
					function () { require(mod); }, // eslint-disable-line no-loop-func
					'requiring ' + mod + ' does not throw'
				);
				if (supportsNodePrefix) {
					st.doesNotThrow(
						function () { require('node:' + mod); }, // eslint-disable-line no-loop-func
						'requiring node:' + mod + ' does not throw'
					);
				} else {
					st['throws'](
						function () { require('node:' + mod); }, // eslint-disable-line no-loop-func
						'requiring node:' + mod + ' throws'
					);
				}
			}
		}
		st.end();
	});

	t.test('core via builtinModules list', { skip: !data.module }, function (st) {
		var libs = require('module').builtinModules;
		if (!libs) {
			st.skip('module.builtinModules does not exist');
		} else {
			var excludeList = [
				'_debug_agent',
				'v8/tools/tickprocessor-driver',
				'v8/tools/SourceMap',
				'v8/tools/tickprocessor',
				'v8/tools/profile'
			];
			for (var i = 0; i < libs.length; ++i) {
				var mod = libs[i];
				if (excludeList.indexOf(mod) === -1) {
					st.ok(data[mod], mod + ' is a core module');
					st.doesNotThrow(
						function () { require(mod); }, // eslint-disable-line no-loop-func
						'requiring ' + mod + ' does not throw'
					);
					if (supportsNodePrefix) {
						st.doesNotThrow(
							function () { require('node:' + mod); }, // eslint-disable-line no-loop-func
							'requiring node:' + mod + ' does not throw'
						);
					} else {
						st['throws'](
							function () { require('node:' + mod); }, // eslint-disable-line no-loop-func
							'requiring node:' + mod + ' throws'
						);
					}
				}
			}
		}
		st.end();
	});

	t.end();
});
