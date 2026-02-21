var big5CPs = []; // index is unicode cp, value is pointer
for (var p = 5024; p < big5.length; p++) {
	// "Let index be index jis0208 excluding all pointers in the range 8272 to 8835, inclusive."
	if (big5[p] != null && big5CPs[big5[p]] == null) {
		big5CPs[big5[p]] = p;
	}
}
//  If code point is U+2550, U+255E, U+2561, U+256A, U+5341, or U+5345, return the last pointer corresponding to code point in index.
big5CPs[0x2550] = 18991;
big5CPs[0x255e] = 18975;
big5CPs[0x2561] = 18977;
big5CPs[0x256a] = 18976;
big5CPs[0x5341] = 5512;
big5CPs[0x5345] = 5599;

function chars2cps(chars) {
	// this is needed because of javascript's handling of supplementary characters
	// char: a string of unicode characters
	// returns an array of decimal code point values
	var haut = 0;
	var out = [];
	for (var i = 0; i < chars.length; i++) {
		var b = chars.charCodeAt(i);
		if (b < 0 || b > 0xffff) {
			alert("Error in chars2cps: byte out of range " + b.toString(16) + "!");
		}
		if (haut != 0) {
			if (0xdc00 <= b && b <= 0xdfff) {
				out.push(0x10000 + ((haut - 0xd800) << 10) + (b - 0xdc00));
				haut = 0;
				continue;
			} else {
				alert(
					"Error in chars2cps: surrogate out of range " +
						haut.toString(16) +
						"!"
				);
				haut = 0;
			}
		}
		if (0xd800 <= b && b <= 0xdbff) {
			haut = b;
		} else {
			out.push(b);
		}
	}
	return out;
}

function big5Encoder(stream) {
	var cps = chars2cps(stream);
	var out = "";
	var cp;
	var finished = false;
	var endofstream = 2000000;

	while (!finished) {
		if (cps.length == 0) cp = endofstream;
		else cp = cps.shift();

		var cpx = 0;

		if (cp == endofstream) {
			finished = true;
			continue;
		}
		if (cp >= 0x00 && cp <= 0x7f) {
			// ASCII
			out += " " + cp.toString(16).toUpperCase();
			continue;
		}
		var ptr = big5CPs[cp];
		if (ptr == null) {
			return null;
			//			out += ' &#'+cp+';'
			//			continue
		}
		var lead = Math.floor(ptr / 157) + 0x81;
		var trail = ptr % 157;
		var offset;
		if (trail < 0x3f) offset = 0x40;
		else {
			offset = 0x62;
		}
		var end = trail + offset;
		out +=
			" " +
			lead.toString(16).toUpperCase() +
			" " +
			end.toString(16).toUpperCase();
	}

	return out.trim();
}

function convertToHex(str) {
	// converts a string of ASCII characters to hex byte codes
	var out = "";
	var result;
	for (var c = 0; c < str.length; c++) {
		result = str.charCodeAt(c).toString(16).toUpperCase() + " ";
		out += result;
	}
	return out;
}

function normalizeStr(str) {
	var out = "";
	for (var c = 0; c < str.length; c++) {
		if (str.charAt(c) == "%") {
			out += String.fromCodePoint(
				parseInt(str.charAt(c + 1) + str.charAt(c + 2), 16)
			);
			c += 2;
		} else out += str.charAt(c);
	}
	var result = "";
	for (var o = 0; o < out.length; o++) {
		result += "%" + out.charCodeAt(o).toString(16).toUpperCase();
	}
	return result.replace(/%1B%28%42$/, "");
}
