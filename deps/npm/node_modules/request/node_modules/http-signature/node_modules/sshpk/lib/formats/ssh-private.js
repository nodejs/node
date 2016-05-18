// Copyright 2015 Joyent, Inc.

module.exports = {
	read: read,
	readSSHPrivate: readSSHPrivate,
	write: write
};

var assert = require('assert-plus');
var asn1 = require('asn1');
var algs = require('../algs');
var utils = require('../utils');
var crypto = require('crypto');

var Key = require('../key');
var PrivateKey = require('../private-key');
var pem = require('./pem');
var rfc4253 = require('./rfc4253');
var SSHBuffer = require('../ssh-buffer');

function read(buf) {
	return (pem.read(buf));
}

var MAGIC = 'openssh-key-v1';

function readSSHPrivate(type, buf) {
	buf = new SSHBuffer({buffer: buf});

	var magic = buf.readCString();
	assert.strictEqual(magic, MAGIC, 'bad magic string');

	var cipher = buf.readString();
	var kdf = buf.readString();

	/* We only support unencrypted keys. */
	if (cipher !== 'none' || kdf !== 'none') {
		throw (new Error('OpenSSH-format key is encrypted ' +
		     '(password-protected). Please use the SSH agent ' +
		     'or decrypt the key.'));
	}

	/* Skip over kdfoptions. */
	buf.readString();

	var nkeys = buf.readInt();
	if (nkeys !== 1) {
		throw (new Error('OpenSSH-format key file contains ' +
		    'multiple keys: this is unsupported.'));
	}

	var pubKey = buf.readBuffer();

	if (type === 'public') {
		assert.ok(buf.atEnd(), 'excess bytes left after key');
		return (rfc4253.read(pubKey));
	}

	var privKeyBlob = buf.readBuffer();
	assert.ok(buf.atEnd(), 'excess bytes left after key');

	buf = new SSHBuffer({buffer: privKeyBlob});

	var checkInt1 = buf.readInt();
	var checkInt2 = buf.readInt();
	assert.strictEqual(checkInt1, checkInt2, 'checkints do not match');

	var ret = {};
	var key = rfc4253.readInternal(ret, 'private', buf.remainder());

	buf.skip(ret.consumed);

	var comment = buf.readString();
	key.comment = comment;

	return (key);
}

function write(key) {
	var pubKey;
	if (PrivateKey.isPrivateKey(key))
		pubKey = key.toPublic();
	else
		pubKey = key;

	var privBuf;
	if (PrivateKey.isPrivateKey(key)) {
		privBuf = new SSHBuffer({});
		var checkInt = crypto.randomBytes(4).readUInt32BE(0);
		privBuf.writeInt(checkInt);
		privBuf.writeInt(checkInt);
		privBuf.write(key.toBuffer('rfc4253'));
		privBuf.writeString(key.comment || '');

		var n = 1;
		while (privBuf._offset % 8 !== 0)
			privBuf.writeChar(n++);
	}

	var buf = new SSHBuffer({});

	buf.writeCString(MAGIC);
	buf.writeString('none');	/* cipher */
	buf.writeString('none');	/* kdf */
	buf.writeBuffer(new Buffer(0));	/* kdfoptions */

	buf.writeInt(1);		/* nkeys */
	buf.writeBuffer(pubKey.toBuffer('rfc4253'));

	if (privBuf)
		buf.writeBuffer(privBuf.toBuffer());

	buf = buf.toBuffer();

	var header;
	if (PrivateKey.isPrivateKey(key))
		header = 'OPENSSH PRIVATE KEY';
	else
		header = 'OPENSSH PUBLIC KEY';

	var tmp = buf.toString('base64');
	var len = tmp.length + (tmp.length / 70) +
	    18 + 16 + header.length*2 + 10;
	buf = new Buffer(len);
	var o = 0;
	o += buf.write('-----BEGIN ' + header + '-----\n', o);
	for (var i = 0; i < tmp.length; ) {
		var limit = i + 70;
		if (limit > tmp.length)
			limit = tmp.length;
		o += buf.write(tmp.slice(i, limit), o);
		buf[o++] = 10;
		i = limit;
	}
	o += buf.write('-----END ' + header + '-----\n', o);

	return (buf.slice(0, o));
}
