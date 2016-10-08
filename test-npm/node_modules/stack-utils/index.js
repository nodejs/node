module.exports = StackUtils;

function StackUtils(opts) {
	if (!(this instanceof StackUtils)) {
		throw new Error('StackUtils constructor must be called with new');
	}
	opts = opts || {};
	this._cwd = (opts.cwd || process.cwd()).replace(/\\/g, '/');
	this._internals = opts.internals || [];
}

module.exports.nodeInternals = nodeInternals;

function nodeInternals() {
	return [
		/\(native\)$/,
		/\(domain.js:\d+:\d+\)$/,
		/\(events.js:\d+:\d+\)$/,
		/\(node.js:\d+:\d+\)$/,
		/\(timers.js:\d+:\d+\)$/,
		/\(module.js:\d+:\d+\)$/,
		/\(internal\/[\w_-]+\.js:\d+:\d+\)$/,
		/\s*at node\.js:\d+:\d+?$/,
		/\/\.node-spawn-wrap-\w+-\w+\/node:\d+:\d+\)?$/
	];
}

StackUtils.prototype.clean = function (stack) {
	if (!Array.isArray(stack)) {
		stack = stack.split('\n');
	}

	if (!(/^\s*at /.test(stack[0])) &&
		(/^\s*at /.test(stack[1]))) {
		stack = stack.slice(1);
	}

	var outdent = false;
	var lastNonAtLine = null;
	var result = [];

	stack.forEach(function (st) {
		st = st.replace(/\\/g, '/');
		var isInternal = this._internals.some(function (internal) {
			return internal.test(st);
		});

		if (isInternal) {
			return null;
		}

		var isAtLine = /^\s*at /.test(st);

		if (outdent) {
			st = st.replace(/\s+$/, '').replace(/^(\s+)at /, '$1');
		} else {
			st = st.trim();
			if (isAtLine) {
				st = st.substring(3);
			}
		}

		st = st.replace(this._cwd + '/', '');

		if (st) {
			if (isAtLine) {
				if (lastNonAtLine) {
					result.push(lastNonAtLine);
					lastNonAtLine = null;
				}
				result.push(st);
			} else {
				outdent = true;
				lastNonAtLine = st;
			}
		}
	}, this);

	stack = result.join('\n').trim();

	if (stack) {
		return stack + '\n';
	}
	return '';
};

StackUtils.prototype.captureString = function (limit, fn) {
	if (typeof limit === 'function') {
		fn = limit;
		limit = Infinity;
	}
	if (!fn) {
		fn = this.captureString;
	}

	var limitBefore = Error.stackTraceLimit;
	if (limit) {
		Error.stackTraceLimit = limit;
	}

	var obj = {};

	Error.captureStackTrace(obj, fn);
	var stack = obj.stack;
	Error.stackTraceLimit = limitBefore;

	return this.clean(stack);
};

StackUtils.prototype.capture = function (limit, fn) {
	if (typeof limit === 'function') {
		fn = limit;
		limit = Infinity;
	}
	if (!fn) {
		fn = this.capture;
	}
	var prepBefore = Error.prepareStackTrace;
	var limitBefore = Error.stackTraceLimit;

	Error.prepareStackTrace = function (obj, site) {
		return site;
	};

	if (limit) {
		Error.stackTraceLimit = limit;
	}

	var obj = {};
	Error.captureStackTrace(obj, fn);
	var stack = obj.stack;
	Error.prepareStackTrace = prepBefore;
	Error.stackTraceLimit = limitBefore;

	return stack;
};

StackUtils.prototype.at = function at(fn) {
	if (!fn) {
		fn = at;
	}

	var site = this.capture(1, fn)[0];

	if (!site) {
		return {};
	}

	var res = {
		line: site.getLineNumber(),
		column: site.getColumnNumber()
	};

	this._setFile(res, site.getFileName());

	if (site.isConstructor()) {
		res.constructor = true;
	}

	if (site.isEval()) {
		res.evalOrigin = site.getEvalOrigin();
	}

	if (site.isNative()) {
		res.native = true;
	}

	var typename = null;
	try {
		typename = site.getTypeName();
	} catch (er) {}

	if (typename &&
		typename !== 'Object' &&
		typename !== '[object Object]') {
		res.type = typename;
	}

	var fname = site.getFunctionName();
	if (fname) {
		res.function = fname;
	}

	var meth = site.getMethodName();
	if (meth && fname !== meth) {
		res.method = meth;
	}

	return res;
};

StackUtils.prototype._setFile = function (result, filename) {
	if (filename) {
		filename = filename.replace(/\\/g, '/');
		if ((filename.indexOf(this._cwd + '/') === 0)) {
			filename = filename.substr(this._cwd.length + 1);
		}
		result.file = filename;
	}
};

var re = new RegExp(
	'^' +
		// Sometimes we strip out the '    at' because it's noisy
	'(?:\\s*at )?' +
		// $1 = ctor if 'new'
	'(?:(new) )?' +
		// Object.method [as foo] (, maybe
		// $2 = function name
		// $3 = method name
	'(?:([^\\(\\[]*)(?: \\[as ([^\\]]+)\\])? \\()?' +
		// (eval at <anonymous> (file.js:1:1),
		// $4 = eval origin
		// $5:$6:$7 are eval file/line/col, but not normally reported
	'(?:eval at ([^ ]+) \\(([^\\)]+):(\\d+):(\\d+)\\), )?' +
		// file:line:col
		// $8:$9:$10
		// $11 = 'native' if native
	'(?:([^\\)]+):(\\d+):(\\d+)|(native))' +
		// maybe close the paren, then end
	'\\)?$'
);

StackUtils.prototype.parseLine = function parseLine(line) {
	var match = line && line.match(re);
	if (!match) {
		return null;
	}

	var ctor = match[1] === 'new';
	var fname = match[2];
	var meth = match[3];
	var evalOrigin = match[4];
	var evalFile = match[5];
	var evalLine = Number(match[6]);
	var evalCol = Number(match[7]);
	var file = match[8];
	var lnum = match[9];
	var col = match[10];
	var native = match[11] === 'native';

	var res = {};

	if (lnum) {
		res.line = Number(lnum);
	}

	if (col) {
		res.column = Number(col);
	}

	this._setFile(res, file);

	if (ctor) {
		res.constructor = true;
	}

	if (evalOrigin) {
		res.evalOrigin = evalOrigin;
		res.evalLine = evalLine;
		res.evalColumn = evalCol;
		res.evalFile = evalFile && evalFile.replace(/\\/g, '/');
	}

	if (native) {
		res.native = true;
	}

	if (fname) {
		res.function = fname;
	}

	if (meth && fname !== meth) {
		res.method = meth;
	}

	return res;
};

var bound = new StackUtils();

Object.keys(StackUtils.prototype).forEach(function (key) {
	StackUtils[key] = bound[key].bind(bound);
});
