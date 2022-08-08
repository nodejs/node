'use strict';

Object.defineProperty(exports, '__esModule', { value: true });

function _interopDefault (ex) { return (ex && (typeof ex === 'object') && 'default' in ex) ? ex['default'] : ex; }

var path = _interopDefault(require('path'));
var minimatch = _interopDefault(require('minimatch'));
var createDebug = _interopDefault(require('debug'));
var objectSchema = require('@humanwhocodes/object-schema');

/**
 * @fileoverview ConfigSchema
 * @author Nicholas C. Zakas
 */

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Assets that a given value is an array.
 * @param {*} value The value to check.
 * @returns {void}
 * @throws {TypeError} When the value is not an array. 
 */
function assertIsArray(value) {
	if (!Array.isArray(value)) {
		throw new TypeError('Expected value to be an array.');
	}
}

/**
 * Assets that a given value is an array containing only strings and functions.
 * @param {*} value The value to check.
 * @returns {void}
 * @throws {TypeError} When the value is not an array of strings and functions.
 */
function assertIsArrayOfStringsAndFunctions(value, name) {
	assertIsArray(value);

	if (value.some(item => typeof item !== 'string' && typeof item !== 'function')) {
		throw new TypeError('Expected array to only contain strings.');
	}
}

//------------------------------------------------------------------------------
// Exports
//------------------------------------------------------------------------------

/**
 * The base schema that every ConfigArray uses.
 * @type Object
 */
const baseSchema = Object.freeze({
	name: {
		required: false,
		merge() {
			return undefined;
		},
		validate(value) {
			if (typeof value !== 'string') {
				throw new TypeError('Property must be a string.');
			}
		}
	},
	files: {
		required: false,
		merge() {
			return undefined;
		},
		validate(value) {

			// first check if it's an array
			assertIsArray(value);

			// then check each member
			value.forEach(item => {
				if (Array.isArray(item)) {
					assertIsArrayOfStringsAndFunctions(item);
				} else if (typeof item !== 'string' && typeof item !== 'function') {
					throw new TypeError('Items must be a string, a function, or an array of strings and functions.');
				}
			});

		}
	},
	ignores: {
		required: false,
		merge() {
			return undefined;
		},
		validate: assertIsArrayOfStringsAndFunctions
	}
});

/**
 * @fileoverview ConfigArray
 * @author Nicholas C. Zakas
 */

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const debug = createDebug('@hwc/config-array');

const MINIMATCH_OPTIONS = {
	// matchBase: true,
	dot: true
};

const CONFIG_TYPES = new Set(['array', 'function']);

/**
 * Shorthand for checking if a value is a string.
 * @param {any} value The value to check.
 * @returns {boolean} True if a string, false if not. 
 */
function isString(value) {
	return typeof value === 'string';
}

/**
 * Normalizes a `ConfigArray` by flattening it and executing any functions
 * that are found inside.
 * @param {Array} items The items in a `ConfigArray`.
 * @param {Object} context The context object to pass into any function
 *      found.
 * @param {Array<string>} extraConfigTypes The config types to check.
 * @returns {Promise<Array>} A flattened array containing only config objects.
 * @throws {TypeError} When a config function returns a function.
 */
async function normalize(items, context, extraConfigTypes) {

	const allowFunctions = extraConfigTypes.includes('function');
	const allowArrays = extraConfigTypes.includes('array');

	async function *flatTraverse(array) {
		for (let item of array) {
			if (typeof item === 'function') {
				if (!allowFunctions) {
					throw new TypeError('Unexpected function.');
				}

				item = item(context);
				if (item.then) {
					item = await item;
				}
			}

			if (Array.isArray(item)) {
				if (!allowArrays) {
					throw new TypeError('Unexpected array.');
				}
				yield * flatTraverse(item);
			} else if (typeof item === 'function') {
				throw new TypeError('A config function can only return an object or array.');
			} else {
				yield item;
			}
		}
	}

	/*
	 * Async iterables cannot be used with the spread operator, so we need to manually
	 * create the array to return.
	 */
	const asyncIterable = await flatTraverse(items);
	const configs = [];

	for await (const config of asyncIterable) {
		configs.push(config);
	}

	return configs;
}

/**
 * Normalizes a `ConfigArray` by flattening it and executing any functions
 * that are found inside.
 * @param {Array} items The items in a `ConfigArray`.
 * @param {Object} context The context object to pass into any function
 *      found.
 * @param {Array<string>} extraConfigTypes The config types to check.
 * @returns {Array} A flattened array containing only config objects.
 * @throws {TypeError} When a config function returns a function.
 */
function normalizeSync(items, context, extraConfigTypes) {

	const allowFunctions = extraConfigTypes.includes('function');
	const allowArrays = extraConfigTypes.includes('array');

	function *flatTraverse(array) {
		for (let item of array) {
			if (typeof item === 'function') {

				if (!allowFunctions) {
					throw new TypeError('Unexpected function.');
				}

				item = item(context);
				if (item.then) {
					throw new TypeError('Async config functions are not supported.');
				}
			}

			if (Array.isArray(item)) {

				if (!allowArrays) {
					throw new TypeError('Unexpected array.');
				}

				yield * flatTraverse(item);
			} else if (typeof item === 'function') {
				throw new TypeError('A config function can only return an object or array.');
			} else {
				yield item;
			}
		}
	}

	return [...flatTraverse(items)];
}

/**
 * Determines if a given file path should be ignored based on the given
 * matcher.
 * @param {Array<string|() => boolean>} ignores The ignore patterns to check. 
 * @param {string} filePath The absolute path of the file to check.
 * @param {string} relativeFilePath The relative path of the file to check.
 * @returns {boolean} True if the path should be ignored and false if not.
 */
function shouldIgnoreFilePath(ignores, filePath, relativeFilePath) {

	// all files outside of the basePath are ignored
	if (relativeFilePath.startsWith('..')) {
		return true;
	}

	let shouldIgnore = false;

	for (const matcher of ignores) {

		if (typeof matcher === 'function') {
			shouldIgnore = shouldIgnore || matcher(filePath);
			continue;
		}

		/*
		 * If there's a negated pattern, that means anything matching
		 * must NOT be ignored. To do that, we need to use the `flipNegate`
		 * option for minimatch to check if the filepath matches the
		 * pattern specified after the !, and if that result is true,
		 * then we return false immediately because this file should
		 * never be ignored.
		 */
		if (matcher.startsWith('!')) {

			/*
			 * The file must already be ignored in order to apply a negated
			 * pattern, because negated patterns simply remove files that
			 * would already be ignored.
			 */
			if (shouldIgnore &&
				minimatch(relativeFilePath, matcher, {
					...MINIMATCH_OPTIONS,
					flipNegate: true
				})) {
				return false;
			}
		} else {
			shouldIgnore = shouldIgnore || minimatch(relativeFilePath, matcher, MINIMATCH_OPTIONS);
		}

	}

	return shouldIgnore;
}

/**
 * Determines if a given file path is matched by a config. If the config
 * has no `files` field, then it matches; otherwise, if a `files` field
 * is present then we match the globs in `files` and exclude any globs in
 * `ignores`.
 * @param {string} filePath The absolute file path to check.
 * @param {Object} config The config object to check.
 * @returns {boolean} True if the file path is matched by the config,
 *      false if not.
 */
function pathMatches(filePath, basePath, config) {

	/*
	 * For both files and ignores, functions are passed the absolute
	 * file path while strings are compared against the relative
	 * file path.
	 */
	const relativeFilePath = path.relative(basePath, filePath);

	// if files isn't an array, throw an error
	if (!Array.isArray(config.files) || config.files.length === 0) {
		throw new TypeError('The files key must be a non-empty array.');
	}

	// match both strings and functions
	const match = pattern => {

		if (isString(pattern)) {
			return minimatch(relativeFilePath, pattern, MINIMATCH_OPTIONS);
		}

		if (typeof pattern === 'function') {
			return pattern(filePath);
		}

		throw new TypeError(`Unexpected matcher type ${pattern}.`);
	};

	// check for all matches to config.files
	let filePathMatchesPattern = config.files.some(pattern => {
		if (Array.isArray(pattern)) {
			return pattern.every(match);
		}

		return match(pattern);
	});

	/*
	 * If the file path matches the config.files patterns, then check to see
	 * if there are any files to ignore.
	 */
	if (filePathMatchesPattern && config.ignores) {
		filePathMatchesPattern = !shouldIgnoreFilePath(config.ignores, filePath, relativeFilePath);
	}

	return filePathMatchesPattern;
}

/**
 * Ensures that a ConfigArray has been normalized.
 * @param {ConfigArray} configArray The ConfigArray to check. 
 * @returns {void}
 * @throws {Error} When the `ConfigArray` is not normalized.
 */
function assertNormalized(configArray) {
	// TODO: Throw more verbose error
	if (!configArray.isNormalized()) {
		throw new Error('ConfigArray must be normalized to perform this operation.');
	}
}

/**
 * Ensures that config types are valid.
 * @param {Array<string>} extraConfigTypes The config types to check.
 * @returns {void}
 * @throws {Error} When the config types array is invalid.
 */
function assertExtraConfigTypes(extraConfigTypes) {
	if (extraConfigTypes.length > 2) {
		throw new TypeError('configTypes must be an array with at most two items.');
	}

	for (const configType of extraConfigTypes) {
		if (!CONFIG_TYPES.has(configType)) {
			throw new TypeError(`Unexpected config type "${configType}" found. Expected one of: "object", "array", "function".`);
		}
	}
}

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

const ConfigArraySymbol = {
	isNormalized: Symbol('isNormalized'),
	configCache: Symbol('configCache'),
	schema: Symbol('schema'),
	finalizeConfig: Symbol('finalizeConfig'),
	preprocessConfig: Symbol('preprocessConfig')
};

// used to store calculate data for faster lookup
const dataCache = new WeakMap();

/**
 * Represents an array of config objects and provides method for working with
 * those config objects.
 */
class ConfigArray extends Array {

	/**
	 * Creates a new instance of ConfigArray.
	 * @param {Iterable|Function|Object} configs An iterable yielding config
	 *      objects, or a config function, or a config object.
	 * @param {string} [options.basePath=""] The path of the config file
	 * @param {boolean} [options.normalized=false] Flag indicating if the
	 *      configs have already been normalized.
	 * @param {Object} [options.schema] The additional schema 
	 *      definitions to use for the ConfigArray schema.
	 * @param {Array<string>} [options.configTypes] List of config types supported.
	 */
	constructor(configs, {
		basePath = '',
		normalized = false,
		schema: customSchema,
		extraConfigTypes = []
	} = {}
) {
		super();

		/**
	 * Tracks if the array has been normalized.
	 * @property isNormalized
	 * @type boolean
	 * @private
	 */
		this[ConfigArraySymbol.isNormalized] = normalized;

		/**
	 * The schema used for validating and merging configs.
	 * @property schema
	 * @type ObjectSchema
	 * @private
	 */
		this[ConfigArraySymbol.schema] = new objectSchema.ObjectSchema({
			...customSchema,
			...baseSchema
		});

		/**
	 * The path of the config file that this array was loaded from.
	 * This is used to calculate filename matches.
	 * @property basePath
	 * @type string
	 */
		this.basePath = basePath;

		assertExtraConfigTypes(extraConfigTypes);

		/**
	 * The supported config types.
	 * @property configTypes
	 * @type Array<string>
	 */
		this.extraConfigTypes = Object.freeze([...extraConfigTypes]);

		/**
	 * A cache to store calculated configs for faster repeat lookup.
	 * @property configCache
	 * @type Map
	 * @private
	 */
		this[ConfigArraySymbol.configCache] = new Map();

		// init cache
		dataCache.set(this, { explicitMatches: new Map() });

		// load the configs into this array
		if (Array.isArray(configs)) {
		this.push(...configs);
		} else {
		this.push(configs);
		}

	}

/**
	 * Prevent normal array methods from creating a new `ConfigArray` instance.
	 * This is to ensure that methods such as `slice()` won't try to create a 
	 * new instance of `ConfigArray` behind the scenes as doing so may throw
	 * an error due to the different constructor signature.
	 * @returns {Function} The `Array` constructor.
	 */
static get [Symbol.species]() {
	return Array;
}

/**
	 * Returns the `files` globs from every config object in the array.
	 * This can be used to determine which files will be matched by a
	 * config array or to use as a glob pattern when no patterns are provided
	 * for a command line interface.
	 * @returns {Array<string|Function>} An array of matchers.
	 */
get files() {

	assertNormalized(this);

	// if this data has been cached, retrieve it
	const cache = dataCache.get(this);

	if (cache.files) {
		return cache.files;
	}

	// otherwise calculate it

	const result = [];

	for (const config of this) {
		if (config.files) {
			config.files.forEach(filePattern => {
				result.push(filePattern);
			});
		}
	}

	// store result
	cache.files = result;
	dataCache.set(this, cache);

	return result;
}

/**
	 * Returns ignore matchers that should always be ignored regardless of
	 * the matching `files` fields in any configs. This is necessary to mimic
	 * the behavior of things like .gitignore and .eslintignore, allowing a
	 * globbing operation to be faster.
	 * @returns {string[]} An array of string patterns and functions to be ignored.
	 */
get ignores() {

	assertNormalized(this);

	// if this data has been cached, retrieve it
	const cache = dataCache.get(this);

	if (cache.ignores) {
		return cache.ignores;
	}

	// otherwise calculate it

	const result = [];

	for (const config of this) {

		/*
		 * We only count ignores if there are no other keys in the object.
		 * In this case, it acts list a globally ignored pattern. If there
		 * are additional keys, then ignores act like exclusions.
		 */
		if (config.ignores && Object.keys(config).length === 1) {
			result.push(...config.ignores);
		}
	}

	// store result
	cache.ignores = result;
	dataCache.set(this, cache);

	return result;
}

/**
	 * Indicates if the config array has been normalized.
	 * @returns {boolean} True if the config array is normalized, false if not.
	 */
isNormalized() {
	return this[ConfigArraySymbol.isNormalized];
}

/**
	 * Normalizes a config array by flattening embedded arrays and executing
	 * config functions.
	 * @param {ConfigContext} context The context object for config functions.
	 * @returns {Promise<ConfigArray>} The current ConfigArray instance.
	 */
async normalize(context = {}) {

	if (!this.isNormalized()) {
		const normalizedConfigs = await normalize(this, context, this.extraConfigTypes);
		this.length = 0;
		this.push(...normalizedConfigs.map(this[ConfigArraySymbol.preprocessConfig].bind(this)));
		this[ConfigArraySymbol.isNormalized] = true;

		// prevent further changes
		Object.freeze(this);
	}

	return this;
}

/**
	 * Normalizes a config array by flattening embedded arrays and executing
	 * config functions.
	 * @param {ConfigContext} context The context object for config functions.
	 * @returns {ConfigArray} The current ConfigArray instance.
	 */
normalizeSync(context = {}) {

	if (!this.isNormalized()) {
		const normalizedConfigs = normalizeSync(this, context, this.extraConfigTypes);
		this.length = 0;
		this.push(...normalizedConfigs.map(this[ConfigArraySymbol.preprocessConfig].bind(this)));
		this[ConfigArraySymbol.isNormalized] = true;

		// prevent further changes
		Object.freeze(this);
	}

	return this;
}

/**
	 * Finalizes the state of a config before being cached and returned by
	 * `getConfig()`. Does nothing by default but is provided to be
	 * overridden by subclasses as necessary.
	 * @param {Object} config The config to finalize.
	 * @returns {Object} The finalized config.
	 */
[ConfigArraySymbol.finalizeConfig](config) {
	return config;
}

/**
	 * Preprocesses a config during the normalization process. This is the
	 * method to override if you want to convert an array item before it is
	 * validated for the first time. For example, if you want to replace a
	 * string with an object, this is the method to override.
	 * @param {Object} config The config to preprocess.
	 * @returns {Object} The config to use in place of the argument.
	 */
[ConfigArraySymbol.preprocessConfig](config) {
	return config;
}

/**
	 * Determines if a given file path explicitly matches a `files` entry
	 * and also doesn't match an `ignores` entry. Configs that don't have
	 * a `files` property are not considered an explicit match.
	 * @param {string} filePath The complete path of a file to check.
	 * @returns {boolean} True if the file path matches a `files` entry
	 * 		or false if not.
	 */
isExplicitMatch(filePath) {

	assertNormalized(this);

	const cache = dataCache.get(this);

	// first check the cache to avoid duplicate work
	let result = cache.explicitMatches.get(filePath);

	if (typeof result == 'boolean') {
		return result;
	}

	// TODO: Maybe move elsewhere? Maybe combine with getConfig() logic?
	const relativeFilePath = path.relative(this.basePath, filePath);

	if (shouldIgnoreFilePath(this.ignores, filePath, relativeFilePath)) {
		debug(`Ignoring ${filePath}`);

		// cache and return result
		cache.explicitMatches.set(filePath, false);
		return false;
	}

	// filePath isn't automatically ignored, so try to find a match

	for (const config of this) {

		if (!config.files) {
			continue;
		}

		if (pathMatches(filePath, this.basePath, config)) {
			debug(`Matching config found for ${filePath}`);
			cache.explicitMatches.set(filePath, true);
			return true;
		}
	}

	return false;
}

/**
	 * Returns the config object for a given file path.
	 * @param {string} filePath The complete path of a file to get a config for.
	 * @returns {Object} The config object for this file.
	 */
getConfig(filePath) {

	assertNormalized(this);

	// first check the cache to avoid duplicate work
	let finalConfig = this[ConfigArraySymbol.configCache].get(filePath);

	if (finalConfig) {
		return finalConfig;
	}

	// TODO: Maybe move elsewhere?
	const relativeFilePath = path.relative(this.basePath, filePath);

	if (shouldIgnoreFilePath(this.ignores, filePath, relativeFilePath)) {
		debug(`Ignoring ${filePath}`);

		// cache and return result - finalConfig is undefined at this point
		this[ConfigArraySymbol.configCache].set(filePath, finalConfig);
		return finalConfig;
	}

	// filePath isn't automatically ignored, so try to construct config

	const matchingConfigs = [];
	let matchFound = false;

	for (const config of this) {

		if (!config.files) {
			debug(`Universal config found for ${filePath}`);
			matchingConfigs.push(config);
			continue;
		}

		if (pathMatches(filePath, this.basePath, config)) {
			debug(`Matching config found for ${filePath}`);
			matchingConfigs.push(config);
			matchFound = true;
			continue;
		}
	}

	// if matching both files and ignores, there will be no config to create
	if (!matchFound) {
		debug(`No matching configs found for ${filePath}`);

		// cache and return result - finalConfig is undefined at this point
		this[ConfigArraySymbol.configCache].set(filePath, finalConfig);
		return finalConfig;
	}

	// otherwise construct the config

	finalConfig = matchingConfigs.reduce((result, config) => {
		return this[ConfigArraySymbol.schema].merge(result, config);
	}, {}, this);

	finalConfig = this[ConfigArraySymbol.finalizeConfig](finalConfig);

	this[ConfigArraySymbol.configCache].set(filePath, finalConfig);

	return finalConfig;
}

/**
	 * Determines if the given filepath is ignored based on the configs.
	 * @param {string} filePath The complete path of a file to check.
	 * @returns {boolean} True if the path is ignored, false if not.
	 */
isIgnored(filePath) {
	return this.getConfig(filePath) === undefined;
}

}

exports.ConfigArray = ConfigArray;
exports.ConfigArraySymbol = ConfigArraySymbol;
