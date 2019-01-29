'use strict';

module.exports = (callback, ...args) => new Promise(resolve => {
	resolve(callback(...args));
});
