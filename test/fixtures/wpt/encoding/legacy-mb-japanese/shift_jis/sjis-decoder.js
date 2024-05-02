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

function sjisDecoder(stream) {
	stream = stream.replace(/%/g, " ");
	stream = stream.replace(/[\s]+/g, " ").trim();
	var bytes = stream.split(" ");
	for (var i = 0; i < bytes.length; i++) bytes[i] = parseInt(bytes[i], 16);
	var out = "";
	var lead, byte, leadoffset, offset, ptr, cp;
	var sjisLead = 0x00;
	var endofstream = 2000000;
	var finished = false;

	while (!finished) {
		if (bytes.length == 0) byte = endofstream;
		else byte = bytes.shift();

		if (byte == endofstream && sjisLead != 0x00) {
			sjisLead = 0x00;
			out += "�";
			continue;
		}
		if (byte == endofstream && sjisLead == 0x00) {
			finished = true;
			continue;
		}
		if (sjisLead != 0x00) {
			lead = sjisLead;
			ptr = null;
			sjisLead = 0x00;
			if (byte < 0x7f) offset = 0x40;
			else offset = 0x41;
			if (lead < 0xa0) leadoffset = 0x81;
			else leadoffset = 0xc1;
			if ((byte >= 0x40 && byte <= 0x7e) || (byte >= 0x80 && byte <= 0xfc))
				ptr = (lead - leadoffset) * 188 + byte - offset;
			if (ptr != null && ptr >= 8836 && ptr <= 10715) {
				out += dec2char(0xe000 + ptr - 8836);
				continue;
			}
			if (ptr == null) cp = null;
			else cp = jis0208[ptr];
			if (cp == null && byte >= 0x00 && byte <= 0x7f) {
				bytes.unshift(byte);
			}
			if (cp == null) {
				out += "�";
				continue;
			}
			out += dec2char(cp);
			continue;
		}
		if ((byte >= 0x00 && byte <= 0x7f) || byte == 0x80) {
			out += dec2char(byte);
			continue;
		}
		if (byte >= 0xa1 && byte <= 0xdf) {
			out += dec2char(0xff61 + byte - 0xa1);
			continue;
		}
		if ((byte >= 0x81 && byte <= 0x9f) || (byte >= 0xe0 && byte <= 0xfc)) {
			sjisLead = byte;
			continue;
		}
		out += "�";
	}
	return out;
}
