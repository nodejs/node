'use strict';

const stringWidth = require('string-width');
const stripAnsi = require('strip-ansi');

const concat = Array.prototype.concat;
const defaults = {
	character: ' ',
	newline: '\n',
	padding: 2,
	sort: true,
	width: 0
};

function byPlainText(a, b) {
	const plainA = stripAnsi(a);
	const plainB = stripAnsi(b);

	if (plainA === plainB) {
		return 0;
	}

	if (plainA > plainB) {
		return 1;
	}

	return -1;
}

function makeArray() {
	return [];
}

function makeList(count) {
	return Array.apply(null, Array(count));
}

function padCell(fullWidth, character, value) {
	const valueWidth = stringWidth(value);
	const filler = makeList(fullWidth - valueWidth + 1);

	return value + filler.join(character);
}

function toRows(rows, cell, i) {
	rows[i % rows.length].push(cell);

	return rows;
}

function toString(arr) {
	return arr.join('');
}

function columns(values, options) {
	values = concat.apply([], values);
	options = Object.assign({}, defaults, options);

	let cells = values
		.filter(Boolean)
		.map(String);

	if (options.sort !== false) {
		cells = cells.sort(byPlainText);
	}

	const termWidth = options.width || process.stdout.columns;
	const cellWidth = Math.max.apply(null, cells.map(stringWidth)) + options.padding;
	const columnCount = Math.floor(termWidth / cellWidth) || 1;
	const rowCount = Math.ceil(cells.length / columnCount) || 1;

	if (columnCount === 1) {
		return cells.join(options.newline);
	}

	return cells
		.map(padCell.bind(null, cellWidth, options.character))
		.reduce(toRows, makeList(rowCount).map(makeArray))
		.map(toString)
		.join(options.newline);
}

module.exports = columns;
