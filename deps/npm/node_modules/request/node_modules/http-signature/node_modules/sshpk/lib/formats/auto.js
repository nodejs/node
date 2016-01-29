// Copyright 2015 Joyent, Inc.

module.exports = {
	read: read,
	write: write
};

var assert = require('assert-plus');
var utils = require('../utils');
var Key = require('../key');
var PrivateKey = require('../private-key');

var pem = require('./pem');
var ssh = require('./ssh');
var rfc4253 = require('./rfc4253');

function read(buf) {
	if (typeof (buf) === 'string') {
		if (buf.trim().match(/^[-]+[ ]*BEGIN/))
			return (pem.read(buf));
		if (buf.match(/^\s*ssh-[a-z]/))
			return (ssh.read(buf));
		if (buf.match(/^\s*ecdsa-/))
			return (ssh.read(buf));
		buf = new Buffer(buf, 'binary');
	} else {
		assert.buffer(buf);
		if (findPEMHeader(buf))
			return (pem.read(buf));
		if (findSSHHeader(buf))
			return (ssh.read(buf));
	}
	if (buf.readUInt32BE(0) < buf.length)
		return (rfc4253.read(buf));
	throw (new Error('Failed to auto-detect format of key'));
}

function findSSHHeader(buf) {
	var offset = 0;
	while (offset < buf.length &&
	    (buf[offset] === 32 || buf[offset] === 10 || buf[offset] === 9))
		++offset;
	if (offset + 4 <= buf.length &&
	    buf.slice(offset, offset + 4).toString('ascii') === 'ssh-')
		return (true);
	if (offset + 6 <= buf.length &&
	    buf.slice(offset, offset + 6).toString('ascii') === 'ecdsa-')
		return (true);
	return (false);
}

function findPEMHeader(buf) {
	var offset = 0;
	while (offset < buf.length &&
	    (buf[offset] === 32 || buf[offset] === 10))
		++offset;
	if (buf[offset] !== 45)
		return (false);
	while (offset < buf.length &&
	    (buf[offset] === 45))
		++offset;
	while (offset < buf.length &&
	    (buf[offset] === 32))
		++offset;
	if (offset + 5 > buf.length ||
	    buf.slice(offset, offset + 5).toString('ascii') !== 'BEGIN')
		return (false);
	return (true);
}

function write(key) {
	throw (new Error('"auto" format cannot be used for writing'));
}
