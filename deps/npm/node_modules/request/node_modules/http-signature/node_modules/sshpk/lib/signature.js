// Copyright 2015 Joyent, Inc.

module.exports = Signature;

var assert = require('assert-plus');
var algs = require('./algs');
var crypto = require('crypto');
var errs = require('./errors');
var utils = require('./utils');
var asn1 = require('asn1');
var SSHBuffer = require('./ssh-buffer');

var InvalidAlgorithmError = errs.InvalidAlgorithmError;
var SignatureParseError = errs.SignatureParseError;

function Signature(opts) {
	assert.object(opts, 'options');
	assert.arrayOfObject(opts.parts, 'options.parts');
	assert.string(opts.type, 'options.type');

	var partLookup = {};
	for (var i = 0; i < opts.parts.length; ++i) {
		var part = opts.parts[i];
		partLookup[part.name] = part;
	}

	this.type = opts.type;
	this.hashAlgorithm = opts.hashAlgo;
	this.parts = opts.parts;
	this.part = partLookup;
}

Signature.prototype.toBuffer = function (format) {
	if (format === undefined)
		format = 'asn1';
	assert.string(format, 'format');

	var buf;

	switch (this.type) {
	case 'rsa':
	case 'ed25519':
		if (format === 'ssh') {
			buf = new SSHBuffer({});
			buf.writeString('ssh-' + this.type);
			buf.writePart(this.part.sig);
			return (buf.toBuffer());
		} else {
			return (this.part.sig.data);
		}

	case 'dsa':
	case 'ecdsa':
		var r, s;
		if (format === 'asn1') {
			var der = new asn1.BerWriter();
			der.startSequence();
			r = utils.mpNormalize(this.part.r.data);
			s = utils.mpNormalize(this.part.s.data);
			der.writeBuffer(r, asn1.Ber.Integer);
			der.writeBuffer(s, asn1.Ber.Integer);
			der.endSequence();
			return (der.buffer);
		} else if (format === 'ssh' && this.type === 'dsa') {
			buf = new SSHBuffer({});
			buf.writeString('ssh-dss');
			r = this.part.r.data;
			if (r[0] === 0x00)
				r = r.slice(1);
			s = this.part.s.data;
			buf.writeBuffer(Buffer.concat([r, s]));
			return (buf.toBuffer());
		} else if (format === 'ssh' && this.type === 'ecdsa') {
			var inner = new SSHBuffer({});
			r = this.part.r;
			if (r[0] === 0x00)
				r = r.slice(1);
			inner.writePart(r);
			inner.writePart(this.part.s);

			buf = new SSHBuffer({});
			/* XXX: find a more proper way to do this? */
			var curve;
			var sz = this.part.r.data.length * 8;
			if (sz === 256)
				curve = 'nistp256';
			else if (sz === 384)
				curve = 'nistp384';
			else if (sz === 528)
				curve = 'nistp521';
			buf.writeString('ecdsa-sha2-' + curve);
			buf.writeBuffer(inner.toBuffer());
			return (buf.toBuffer());
		}
		throw (new Error('Invalid signature format'));
	default:
		throw (new Error('Invalid signature data'));
	}
};

Signature.prototype.toString = function (format) {
	assert.optionalString(format, 'format');
	return (this.toBuffer(format).toString('base64'));
};

Signature.parse = function (data, type, format) {
	if (typeof (data) === 'string')
		data = new Buffer(data, 'base64');
	assert.buffer(data, 'data');
	assert.string(format, 'format');
	assert.string(type, 'type');

	var opts = {};
	opts.type = type.toLowerCase();
	opts.parts = [];

	try {
		assert.ok(data.length > 0, 'signature must not be empty');
		switch (opts.type) {
		case 'rsa':
			return (parseOneNum(data, type, format, opts,
			    'ssh-rsa'));
		case 'ed25519':
			return (parseOneNum(data, type, format, opts,
			    'ssh-ed25519'));

		case 'dsa':
		case 'ecdsa':
			if (format === 'asn1')
				return (parseDSAasn1(data, type, format, opts));
			else if (opts.type === 'dsa')
				return (parseDSA(data, type, format, opts));
			else
				return (parseECDSA(data, type, format, opts));

		default:
			throw (new InvalidAlgorithmError(type));
		}

	} catch (e) {
		if (e instanceof InvalidAlgorithmError)
			throw (e);
		throw (new SignatureParseError(type, format, e));
	}
};

function parseOneNum(data, type, format, opts, headType) {
	if (format === 'ssh') {
		try {
			var buf = new SSHBuffer({buffer: data});
			var head = buf.readString();
		} catch (e) {
			/* fall through */
		}
		if (head === headType) {
			var sig = buf.readPart();
			assert.ok(buf.atEnd(), 'extra trailing bytes');
			sig.name = 'sig';
			opts.parts.push(sig);
			return (new Signature(opts));
		}
	}
	opts.parts.push({name: 'sig', data: data});
	return (new Signature(opts));
}

function parseDSAasn1(data, type, format, opts) {
	var der = new asn1.BerReader(data);
	der.readSequence();
	var r = der.readString(asn1.Ber.Integer, true);
	var s = der.readString(asn1.Ber.Integer, true);

	opts.parts.push({name: 'r', data: utils.mpNormalize(r)});
	opts.parts.push({name: 's', data: utils.mpNormalize(s)});

	return (new Signature(opts));
}

function parseDSA(data, type, format, opts) {
	if (data.length != 40) {
		var buf = new SSHBuffer({buffer: data});
		var d = buf.readBuffer();
		if (d.toString('ascii') === 'ssh-dss')
			d = buf.readBuffer();
		assert.ok(buf.atEnd(), 'extra trailing bytes');
		assert.strictEqual(d.length, 40, 'invalid inner length');
		data = d;
	}
	opts.parts.push({name: 'r', data: data.slice(0, 20)});
	opts.parts.push({name: 's', data: data.slice(20, 40)});
	return (new Signature(opts));
}

function parseECDSA(data, type, format, opts) {
	var buf = new SSHBuffer({buffer: data});

	var r, s;
	var inner = buf.readBuffer();
	if (inner.toString('ascii').match(/^ecdsa-/)) {
		inner = buf.readBuffer();
		assert.ok(buf.atEnd(), 'extra trailing bytes on outer');
		buf = new SSHBuffer({buffer: inner});
		r = buf.readPart();
	} else {
		r = {data: inner};
	}

	s = buf.readPart();
	assert.ok(buf.atEnd(), 'extra trailing bytes');

	r.name = 'r';
	s.name = 's';

	opts.parts.push(r);
	opts.parts.push(s);
	return (new Signature(opts));
}

Signature.isSignature = function (obj, ver) {
	return (utils.isCompatible(obj, Signature, ver));
};

/*
 * API versions for Signature:
 * [1,0] -- initial ver
 * [2,0] -- support for rsa in full ssh format, compat with sshpk-agent
 *          hashAlgorithm property
 * [2,1] -- first tagged version
 */
Signature.prototype._sshpkApiVersion = [2, 1];

Signature._oldVersionDetect = function (obj) {
	assert.func(obj.toBuffer);
	if (obj.hasOwnProperty('hashAlgorithm'))
		return ([2, 0]);
	return ([1, 0]);
};
