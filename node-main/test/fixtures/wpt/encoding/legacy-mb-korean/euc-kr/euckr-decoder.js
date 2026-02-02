function dec2char(n) {
	// converts a decimal number to a Unicode character
	// n: the dec codepoint value to be converted
	if (n <= 0xffff) {
		out = String.fromCharCode(n);
	} else if (n <= 0x10ffff) {
		n -= 0x10000;
		out =
			String.fromCharCode(0xd800 | (n >> 10)) +
			String.fromCharCode(0xdc00 | (n & 0x3ff));
	} else out = "dec2char error: Code point out of range: " + n;
	return out;
}

function euckrDecoder(stream) {
	stream = stream.replace(/%/g, " ");
	stream = stream.replace(/[\s]+/g, " ").trim();
	var bytes = stream.split(" ");
	for (var i = 0; i < bytes.length; i++) bytes[i] = parseInt(bytes[i], 16);
	var out = "";
	var lead, byte, offset, ptr, cp;
	var euckrLead = 0x00;
	var endofstream = 2000000;
	var finished = false;

	while (!finished) {
		if (bytes.length == 0) byte = endofstream;
		else byte = bytes.shift();

		if (byte == endofstream && euckrLead != 0x00) {
			euckrLead = 0x00;
			out += "�";
			continue;
		}
		if (byte == endofstream && euckrLead == 0x00) {
			finished = true;
			continue;
		}

		if (euckrLead != 0x00) {
			lead = euckrLead;
			ptr = null;
			euckrLead = 0x00;
			if (byte >= 0x41 && byte <= 0xfe)
				ptr = (lead - 0x81) * 190 + (byte - 0x41);
			if (ptr == null) cp = null;
			else cp = euckr[ptr];
			if (cp == null && byte >= 0x00 && byte <= 0x7f) bytes.unshift(byte);
			if (cp == null) {
				out += "�";
				continue;
			}
			out += dec2char(cp);
			continue;
		}
		if (byte >= 0x00 && byte <= 0x7f) {
			out += dec2char(byte);
			continue;
		}
		if (byte >= 0x81 && byte <= 0xfe) {
			euckrLead = byte;
			continue;
		}
		out += "�";
	}
	return out;
}
