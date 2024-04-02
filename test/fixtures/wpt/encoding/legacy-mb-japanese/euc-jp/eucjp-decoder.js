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

function eucjpDecoder(stream) {
    stream = stream.replace(/%/g, " ");
    stream = stream.replace(/[\s]+/g, " ").trim();
    var bytes = stream.split(" ");
    for (var i = 0; i < bytes.length; i++) bytes[i] = parseInt(bytes[i], 16);
    var out = "";

    var lead, byte, offset, ptr, cp;
    var jis0212flag = false;
    var eucjpLead = 0x00;
    var endofstream = 2000000;
    var finished = false;

    while (!finished) {
        if (bytes.length == 0) byte = endofstream;
        else byte = bytes.shift();

        if (byte == endofstream && eucjpLead != 0x00) {
            eucjpLead = 0x00;
            out += "�";
            continue;
        }
        if (byte == endofstream && eucjpLead == 0x00) {
            finished = true;
            continue;
        }
        if (eucjpLead == 0x8e && byte >= 0xa1 && byte <= 0xdf) {
            eucjpLead = 0x00;
            out += dec2char(0xff61 + byte - 0xa1);
            continue;
        }
        if (eucjpLead == 0x8f && byte >= 0xa1 && byte <= 0xfe) {
            jis0212flag = true;
            eucjpLead = byte;
            continue;
        }
        if (eucjpLead != 0x00) {
            lead = eucjpLead;
            eucjpLead = 0x00;
            cp = null;

            if (
                lead >= 0xa1 &&
                lead <= 0xfe &&
                (byte >= 0xa1 && byte <= 0xfe)
            ) {
                ptr = (lead - 0xa1) * 94 + byte - 0xa1;
                if (jis0212flag) cp = jis0212[ptr];
                else cp = jis0208[ptr];
            }
            jis0212flag = false;
            if (cp != null) {
                out += dec2char(cp);
                continue;
            }
            if (byte >= 0x00 && byte <= 0x7f) bytes.unshift(byte);
            out += "�";
            continue;
        }
        if (byte >= 0x00 && byte <= 0x7f) {
            out += dec2char(byte);
            continue;
        }
        if (byte == 0x8e || byte == 0x8f || (byte >= 0xa1 && byte <= 0xfe)) {
            eucjpLead = byte;
            continue;
        }
        out += "�";
    }
    return out;
}
