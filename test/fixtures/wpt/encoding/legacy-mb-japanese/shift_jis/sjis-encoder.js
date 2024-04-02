var sjisCPs = []; // index is unicode cp, value is pointer
for (var p = 0; p < 8272; p++) {
	if (jis0208[p] != null && sjisCPs[jis0208[p]] == null) {
		sjisCPs[jis0208[p]] = p;
	}
}
for (p = 8836; p < jis0208.length; p++) {
	if (jis0208[p] != null && sjisCPs[jis0208[p]] == null) {
		sjisCPs[jis0208[p]] = p;
	}
}

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

function sjisEncoder(stream) {
	var cps = chars2cps(stream);
	var out = "";
	var cp;
	var finished = false;
	var endofstream = 2000000;
	var temp, offset, leadoffset, first, second;
	while (!finished) {
		if (cps.length == 0) cp = endofstream;
		else cp = cps.shift();

		if (cp == endofstream) {
			finished = true;
			continue;
		}
		if ((cp >= 0x00 && cp <= 0x7f) || cp == 0x80) {
			out += " " + cp.toString(16).toUpperCase();
			continue;
		}
		if (cp == 0xa5) {
			out += " 5C";
			continue;
		}
		if (cp == 0x203e) {
			out += " 7E";
			continue;
		}
		if (cp >= 0xff61 && cp <= 0xff9f) {
			temp = cp - 0xff61 + 0xa1;
			out += " " + temp.toString(16).toUpperCase();
			continue;
		}
		if (cp == 0x2212) {
			cp = 0xff0d;
		}
		var ptr = sjisCPs[cp];
		if (ptr == null) {
			return null;
			//			out += ' &#'+cp+';'
			//			continue
		}
		var lead = Math.floor(ptr / 188);
		if (lead < 0x1f) leadoffset = 0x81;
		else leadoffset = 0xc1;
		var trail = ptr % 188;
		first = lead + leadoffset;
		if (trail < 0x3f) offset = 0x40;
		else offset = 0x41;
		second = trail + offset;
		out +=
			" " +
			first.toString(16).toUpperCase() +
			" " +
			second.toString(16).toUpperCase();
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
