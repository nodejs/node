'use strict';
const path = require('path');
const os = require('os');

const homedir = os.homedir();
const tmpdir = os.tmpdir();
const env = process.env;

const macos = name => {
	const library = path.join(homedir, 'Library');

	return {
		data: path.join(library, 'Application Support', name),
		config: path.join(library, 'Preferences', name),
		cache: path.join(library, 'Caches', name),
		log: path.join(library, 'Logs', name),
		temp: path.join(tmpdir, name)
	};
};

const windows = name => {
	const appData = env.LOCALAPPDATA || path.join(homedir, 'AppData', 'Local');

	return {
		// data/config/cache/log are invented by me as Windows isn't opinionated about this
		data: path.join(appData, name, 'Data'),
		config: path.join(appData, name, 'Config'),
		cache: path.join(appData, name, 'Cache'),
		log: path.join(appData, name, 'Log'),
		temp: path.join(tmpdir, name)
	};
};

// https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
const linux = name => {
	const username = path.basename(homedir);

	return {
		data: path.join(env.XDG_DATA_HOME || path.join(homedir, '.local', 'share'), name),
		config: path.join(env.XDG_CONFIG_HOME || path.join(homedir, '.config'), name),
		cache: path.join(env.XDG_CACHE_HOME || path.join(homedir, '.cache'), name),
		// https://wiki.debian.org/XDGBaseDirectorySpecification#state
		log: path.join(env.XDG_STATE_HOME || path.join(homedir, '.local', 'state'), name),
		temp: path.join(tmpdir, username, name)
	};
};

module.exports = (name, opts) => {
	if (typeof name !== 'string') {
		throw new TypeError(`Expected string, got ${typeof name}`);
	}

	opts = Object.assign({suffix: 'nodejs'}, opts);

	if (opts.suffix) {
		// add suffix to prevent possible conflict with native apps
		name += `-${opts.suffix}`;
	}

	if (process.platform === 'darwin') {
		return macos(name);
	}

	if (process.platform === 'win32') {
		return windows(name);
	}

	return linux(name);
};
