'use strict';

function data(parent) {
	return {
		styles: [],
		parent,
		contents: []
	};
}

const zeroBound = n => n < 0 ? 0 : n;
const lastIndex = a => zeroBound(a.length - 1);

const last = a => a[lastIndex(a)];

const takeWhileReverse = (array, predicate, start) => {
	const out = [];

	for (let i = start; i >= 0 && i <= start; i--) {
		if (predicate(array[i])) {
			out.unshift(array[i]);
		} else {
			break;
		}
	}

	return out;
};

/**
 * Checks if the character at position i in string is a normal character a.k.a a non control character.
 * */
const isNormalCharacter = (string, i) => {
	const char = string[i];
	const backslash = '\\';

	if (!(char === backslash || char === '{' || char === '}')) {
		return true;
	}

	const n = i === 0 ? 0 : takeWhileReverse(string, x => x === '\\', zeroBound(i - 1)).length;

	return n % 2 === 1;
};

const collectStyles = data => data ? collectStyles(data.parent).concat(data.styles) : ['reset'];

/**
 * Computes the style for a given data based on it's style and the style of it's parent. Also accounts for !style styles
 * which remove a style from the list if present.
 * */
const sumStyles = data => {
	const negateRegex = /^~.+/;
	let out = [];

	for (const style of collectStyles(data)) {
		if (negateRegex.test(style)) {
			const exclude = style.slice(1);
			out = out.filter(x => x !== exclude);
		} else {
			out.push(style);
		}
	}

	return out;
};

/**
 * Takes a string and parses it into a tree of data objects which inherit styles from their parent.
 * */
function parse(string) {
	const root = data(null);
	let pushingStyle = false;

	let current = root;

	for (let i = 0; i < string.length; i++) {
		const char = string[i];

		const addNormalCharacter = () => {
			const lastChunk = last(current.contents);

			if (typeof lastChunk === 'string') {
				current.contents[lastIndex(current.contents)] = lastChunk + char;
			} else {
				current.contents.push(char);
			}
		};

		if (pushingStyle) {
			if (' \t'.indexOf(char) > -1) {
				pushingStyle = false;
			} else if (char === '\n') {
				pushingStyle = false;
				addNormalCharacter();
			} else if (char === '.') {
				current.styles.push('');
			} else {
				current.styles[lastIndex(current.styles)] = (last(current.styles) || '') + char;
			}
		} else if (isNormalCharacter(string, i)) {
			addNormalCharacter();
		} else if (char === '{') {
			pushingStyle = true;
			const nCurrent = data(current);
			current.contents.push(nCurrent);
			current = nCurrent;
		} else if (char === '}') {
			current = current.parent;
		}
	}

	if (current !== root) {
		throw new Error('literal template has an unclosed block');
	}

	return root;
}

/**
 * Takes a tree of data objects and flattens it to a list of data objects with the inherited and negations styles
 * accounted for.
 * */
function flatten(data) {
	let flat = [];

	for (const content of data.contents) {
		if (typeof content === 'string') {
			flat.push({
				styles: sumStyles(data),
				content
			});
		} else {
			flat = flat.concat(flatten(content));
		}
	}

	return flat;
}

function assertStyle(chalk, style) {
	if (!chalk[style]) {
		throw new Error(`invalid Chalk style: ${style}`);
	}
}

/**
 * Checks if a given style is valid and parses style functions.
 * */
function parseStyle(chalk, style) {
	const fnMatch = style.match(/^\s*(\w+)\s*\(\s*([^)]*)\s*\)\s*/);
	if (!fnMatch) {
		assertStyle(chalk, style);
		return chalk[style];
	}

	const name = fnMatch[1].trim();
	const args = fnMatch[2].split(/,/g).map(s => s.trim());

	assertStyle(chalk, name);

	return chalk[name].apply(chalk, args);
}

/**
 * Performs the actual styling of the string, essentially lifted from cli.js.
 * */
function style(chalk, flat) {
	return flat.map(data => {
		const fn = data.styles.reduce(parseStyle, chalk);
		return fn(data.content.replace(/\n$/, ''));
	}).join('');
}

module.exports = (chalk, string) => style(chalk, flatten(parse(string)));
