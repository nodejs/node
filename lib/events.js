'use strict';



/* Auxiliary global data/code */
var internalUtil, domain;

function emit0(h,s) { //I wonder if 'this' would refer to the event emitter here - would allow eliminating 's'
	if (typeof h == 'function')
		h.call(s);
	else
		for (let i = 0, l = h.length; i < l; i++) //declared 'i' because otherwise the handlers would be called in reverse order
			h[i].call(s);
}

function emit1(h,s,a1) {
	if (typeof h == 'function')
		h.call(s,a1);
	else
		for (let i = 0, l = h.length; i < l; i++)
			h[i].call(s,a1);
}

function emit2(h,s,a1,a2) {
	if (typeof h == 'function')
		h.call(s,a1,a2);
	else
		for (let i = 0, l = h.length; i < l; i++)
			h[i].call(s,a1,a2);
}

function emit3(h,s,a1,a2,a3) {
	if (typeof h == 'function')
		h.call(s,a1,a2,a3);
	else
		for (let i = 0, l = h.length; i < l; i++)
			h[i].call(s,a1,a2,a3);
}

function emitN(h,s,as) {
	if (typeof h == 'function')
		h.apply(s,as);
	else
		for (let i = 0, l = h.length; i < l; i++)
			h[i].apply(s,as);
}

function listenerCount(e) {
	var he = this.events[e];
	return he ? (he instanceof Array ? he.length : 1) : 0;
}

function getMaxListeners() {
	return this.maxListeners ? this.maxListeners : EventEmitter.defaultMaxListeners;
}

function arrayClone(arr1) {
	var i = t.length, arr2 = new Array(i);
	while (i--)
		arr2[i] = arr1[i];
	return arr2;
}



/* Constructor */
function EventEmitter() {
	this.domain = null;
	if (EventEmitter.usingDomains) {
		domain = domain || require('domain');
		if (domain.active && !(this instanceof domain.Domain))
			this.domain = domain.active;
	}
	this.events = null;
	this.maxListeners = null;
}

//For backwards compatibility with Node v0.10.x
EventEmitter.EventEmitter = EventEmitter;



/* Class data/code */
EventEmitter.defaultMaxListeners = 10;
EventEmitter.usingDomains = false;
EventEmitter.listenerCount = function(em,ev) {
	return typeof em.listenerCount === 'function' ? em.listenerCount(ev): listenerCount.call(em,ev);
}



/* Prototype data/code */
EventEmitter.prototype.getMaxListeners = getMaxListeners;

EventEmitter.prototype.setMaxListeners = function(n) {
	if (typeof n !== 'number' || n < 0 || n % 1 !== 0) //any numeric test will also catch NaN and return false
		throw new TypeError("n must be a positive integer");
	this.maxListeners = n;
	return this;
};

EventEmitter.prototype.emit = function(e) {
	var he = this.events[e], domain = this.domain, needDomainExit = false;

	if (!he) {
		if (e === 'error') {
			let er = arguments[1];
			if (domain) {
				if(!er)
					er = new Error("Uncaught, unspecified 'error' event.");
				er.domainEmitter = this;
				er.domain = domain;
				er.domainThrown = false;
				domain.emit('error',er);}
			else if (er instanceof Error)
				throw er;
			else {
				let err = new Error("Uncaught, unspecified 'error' event. (" + er + ")");
				err.context = er;
				throw err;
			}
		}
		return false;
	}
	if (domain && this !== process) {
		domain.enter();
		needDomainExit = true;
	}

	var l = arguments.length;
	switch (l) {
		case 1: emit0(he,this); break;
		case 2: emit1(he,this,arguments[1]); break;
		case 3: emit2(he,this,arguments[1],arguments[2]); break;
		case 4: emit3(he,this,arguments[1],arguments[2],arguments[3]); break;
		default:
			let j = l - 1, as = new Array(j);
			for (let i = j - 1; --i, --j;)
				as[i] = arguments[j];
			emitN(he,this,as);
	}

	if (needDomainExit)
		domain.exit();
	return true;
};

EventEmitter.prototype.on = function(e,h) {
	var he = this.events[e];
	if (typeof h !== 'function')
		throw new TypeError("Listener must be a function");
	if (this.events.newListener)
		this.emit('newListener',e,h);

	if (!he)
		this.events[e] = h;
	else if (he instanceof Array)
		this.events[e].push(h);
	else
		this.events[e] = [he,h];


	if (!he.warned) {
		let m = getMaxListeners.call(this), l = listenerCount.call(this,e);
		if (m > 0 && l > m) {
			he.warned = true;
			if (!internalUtil)
				internalUtil = require('internal/util');
			internalUtil.error("(node) Warning: possible EventEmitter memory leak detected. %d %s listeners added. Use emitter.setMaxListeners to increase limit.",l,e);
			console.trace();
		}

	return this;
};

EventEmitter.prototype.addListener = EventEmitter.prototype.on;

EventEmitter.prototype.once = function(e,h) {
	if (typeof h !== 'function')
		throw new TypeError("Listener must be a function");
	function g() {
		h.apply(this,arguments);
		this.removeListener(e,g);
	}
	this.on(e,g);
	return this;
};

EventEmitter.prototype.removeListener = function(e,h) {
	var he = this.events[e], l = listenerCount.call(this,e);

	switch (l) {
		case 0: return this; break;
		case 1: delete this.events[e]; break;
		case 2: this.events[e] = he[0]; break;
		default:
			let i = l;
			while (i-- && he[i] !== h);
			if (i === 0)
				return this;
			if (i < l - 1)
				for (let j = i + 1; j < l; i++,j++)
					he[i] = he[j];
			he.pop(); //can't inline subsequent assignment because 'pop' returns the popped element, not the array
			this.events[e] = he;
	}

	if (this.events.removeListener)
		this.emit('removeListener');
	return this;
};

EventEmitter.prototype.removeAllListeners = function(e) {
	if (!this.events.removeListener) {
		if (!e)
			this.events = null;
		else
			delete this.events[e];
	}
	else {
		if (!e) {
			let h = this.events;
			for (let keys = Object.keys(h), i = keys.length; i--;)
				if (keys[i] !== 'removeListener')
					this.removeAllListeners(keys[i]);
			this.removeAllListeners('removeListener');
		}
		else {
			let he = this.events[e], l = listenerCount.call(this,e) - 1;
			if (l == 1)
				this.removeListener(e,he);
			else
				while (--l)
					this.removeListener(e,he[l]);
		}
	return this;
};

EventEmitter.prototype.listeners = function(e) {
	var he = arrayClone(this.events[e]);
	return he ? (he instanceof Array ? he : [he]) : [];
};

EventEmitter.prototype.listenerCount = listenerCount;



/* Export */
module.exports = EventEmitter;
