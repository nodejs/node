"use strict";

/* Configuration */
let _enableBreadcrumbs = false;
let _errorHandler = console.log;

/**
 * Enable/disable full stack traces for all coroutines
 * @param flag {Boolean}
 */
function enableBreadcrumbs(flag) {
	_enableBreadcrumbs = flag;
}

/* Current executing coroutine */
let _resume = false;

/**
 * Set error handle function. Coroutine passes uncaught exceptions thrown from Coroutine into this function
 * @param {Function} error_handler_function(errorObject)
 */
function setErrorHandler(errHandler) {
	_errorHandler = errHandler;
}

function stitchBreadcrumbs(error, breadcrumbs) {
	if (breadcrumbs) {
		breadcrumbs.push(error);
		const filteredLines = [];
		for (let i = breadcrumbs.length - 1; i >= 0; i--) {
			const stack = breadcrumbs[i].stack;
			if (stack) {
				const lines = stack.split("\n");
				for (const line of lines) {
					if (
						!line.includes("coroutine.js") &&
						!line.includes("next (native)")
					) {
						filteredLines.push(line);
					}
				}
			}
			error.stack = filteredLines.join("\n");
		}
	}
}

/**
 * Utility function that resumes the coroutine by throwing TimedOut exception when coroutine/callback/future does not finish before set timeout
 * @param {Function} either resume or future function
 * @param {String} 'coroutine' or 'callback' or 'future'
 * @param {String} Name of the callback or future involved. optional.
 * @param {Number} timeout in milliseconds
 */
function timedOut(resume, type, name, timeout) {
	const e = new Error(
		(type || "") +
		" " +
		(name || "") +
		" did not finish within " +
		timeout +
		" ms."
	);
	e.cause = "TimedOut";
	e.name = name;
	resume(e);
}

/**
 * Wait till all the passed in futures are complete - i.e. done executing
 * @param {Array of futur objects} Futures to wait on
 * @return {Number} number of futures who returned error
 */
function* join(...futures) {
	let errorCount = 0;
	for (const future of futures) {
		while (future.done !== true) {
			future.isJoined = true;
			yield;
		}
		// future.isJoined = null;
		if (future.error) {
			errorCount += 1;
		}
	}
	return errorCount;
}

/**
 * Used to run coroutine for the first time after it is created
 * @param iter {Iterator} Iterator received by calling generator function
 * @param options {Object} Optional options object. It can include following properties
 *        - name: Name of the Coroutine
 *        - timeout: Maximum time in milliseconds up to which this Coroutine is allowed to run
 *        - enableStackTrace: Enable clean stack trace across yields
 *        - stackDepth: Max number of lines to print in case of clean stack traces enabled exceptions
 */
function run(iter, options) {
	if (_resume) {
		throw new Error(
			"Cannot spawn new coroutine from within another coroutine."
		);
	}

	if (!iter || typeof iter[Symbol.iterator] !== "function") {
		throw new Error(
			"First parameter to coroutine.create() must be iterator returned by a generator function."
		);
	}

	// const o = options || null;
	const name = (options && options.name) || "";
	const errorHandlerFn = (options && options.errorHandler) || _errorHandler;
	let breadcrumbs =
		(options && options.enableBreadcrumbs) || _enableBreadcrumbs ? [] : null;
	let state = new Map();

	const resume = function (error, ...rest) {
		if (!iter) {
			return; // Coroutine already finished
		}

		// Callback was invoked so cancel callback timer, if any
		const cbTimer = resume.callbackTimer;
		if (cbTimer) {
			clearTimeout(cbTimer);
		}

		try {
			if (error) {
				error.cause = error.cause || "Exception";
				stitchBreadcrumbs(error, breadcrumbs);
				error.coroutine = name;
			}

			// Resume suspended coroutine
			resume.cbInProgress = false;
			resume.timer = null;
			exports.state = state;
			_resume = resume;
			const result = error ? iter.throw(error) : iter.next(rest);

			if (result.done) {
				iter = breadcrumbs = state = null;
				return result.value;
			}
		} catch (e) {
			e.message =
				"Unhandled exception in coroutine " + (name || "") + " : " + e.message;
			e.coroutine = name;
			stitchBreadcrumbs(e, breadcrumbs);
			iter = breadcrumbs = state = null;
			errorHandlerFn(e);
		} finally {
			// We are outside running coroutine, clear "current coroutine" variables
			exports.state = null;
			_resume = null;
		}
	};

	// This is the global timeout that limits duration of the entire Coroutine execution
	const timeout = (options && options.timeout) || null;
	if (timeout && timeout > 0) {
		setTimeout(timedOut, timeout, resume, "coroutine", name, timeout);
	}

	resume.krName = name;
	resume.breadcrumbs = breadcrumbs;
	// Start coroutine execution
	resume();
}

function prepareCoroutineCB(resume) {
	if (!resume) {
		throw new Error(
			"coroutine.callback() must be invoked from within an active coroutine"
		);
	}

	if (resume.cbInProgress) {
		throw new Error(
			"coroutine.callback() called when there is already another callback in progress"
		);
	}

	const breadcrumbs = resume.breadcrumbs;
	if (breadcrumbs) {
		const name = resume.krName;
		const errMessage = name ? name + " suspended at" : "suspended at";
		breadcrumbs.push(new Error(errMessage));
	}
}

/**
 * Returns NodeJs style callback function - callback(err, data) - which resumes suspended coroutine when called
 * @param timeout {Number} in milliseconds. optional. set to null or 0 for infinite time out.
 * @param name callback name. optional
 * @return {Function} callback function
 */
function callback(timeout, name) {
	const resume = _resume;
	prepareCoroutineCB(resume);
	resume.cbInProgress = true;
	if (timeout && timeout > 0) {
		resume.callbackTimer = setTimeout(
			timedOut,
			timeout,
			resume,
			"callback",
			name,
			timeout
		);
	}
	return resume;
}

/**
 * Sleep for given number of milliseconds. Doesn't block the node's event loop
 * @param ms {Number} Number of milliseconds to sleep
 */
function sleep(timeout) {
	const resume = assertCalledFromCoroutine("sleep");
	setTimeout(resume, timeout);
}

/**
 * Returns a Future object that can be passed in place of normal node callback. Future
 * objects work with coroutine.join() to facilitate firing multiple async operations
 * from a single coroutine without blocking or yielding and then waiting for all of them
 * to finish at a single 'join' point in the code
 * @param timeout {Number} Number of milliseconds after which this future will timeout wih error.cause = 'timedout'
 * @param name {String} future name. optional.
 * @return {Function} future callback
 */
function future(timeout, name) {
	const resume = _resume;
	prepareCoroutineCB(resume);

	const future = function (error, ...rest) {
		if (future.done === true) {
			return;
		}
		future.done = true;

		if (error) {
			error.cause = error.cause || "Exception";
			future.error = error;
		} else {
			future.data = rest;
		}

		if (future.isJoined) {
			resume(null, future);
		}
	};

	if (timeout && timeout > 0) {
		setTimeout(timedOut, timeout, future, "future", name, timeout);
	}

	return future;
}

function assertCalledFromCoroutine(name) {
	const resume = _resume;
	if (!resume) {
		throw new Error(name + " must be called from within an active coroutine");
	}
	return resume;
}

/**
 * Akin to thread.yield()
 */
function defer() {
	const resume = assertCalledFromCoroutine("defer");
	setImmediate(resume);
}

module.exports = {
	defer,
	sleep,
	future,
	callback,
	setErrorHandler,
	enableBreadcrumbs,
	join,
	run,
};
