import stripAnsi from 'strip-ansi';
import isFullwidthCodePoint from 'is-fullwidth-code-point';
import emojiRegex from 'emoji-regex';

export default function stringWidth(string) {
	if (typeof string !== 'string' || string.length === 0) {
		return 0;
	}

	string = stripAnsi(string);

	if (string.length === 0) {
		return 0;
	}

	string = string.replace(emojiRegex(), '  ');

	let width = 0;

	for (let index = 0; index < string.length; index++) {
		const codePoint = string.codePointAt(index);

		// Ignore control characters
		if (codePoint <= 0x1F || (codePoint >= 0x7F && codePoint <= 0x9F)) {
			continue;
		}

		// Ignore combining characters
		if (codePoint >= 0x300 && codePoint <= 0x36F) {
			continue;
		}

		// Surrogates
		if (codePoint > 0xFFFF) {
			index++;
		}

		width += isFullwidthCodePoint(codePoint) ? 2 : 1;
	}

	return width;
}
