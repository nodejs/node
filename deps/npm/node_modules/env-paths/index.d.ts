declare namespace envPaths {
	export interface Options {
		/**
		__Don't use this option unless you really have to!__

		Suffix appended to the project name to avoid name conflicts with native apps. Pass an empty string to disable it.

		@default 'nodejs'
		*/
		readonly suffix?: string;
	}

	export interface Paths {
		/**
		Directory for data files.
		*/
		readonly data: string;

		/**
		Directory for data files.
		*/
		readonly config: string;

		/**
		Directory for non-essential data files.
		*/
		readonly cache: string;

		/**
		Directory for log files.
		*/
		readonly log: string;

		/**
		Directory for temporary files.
		*/
		readonly temp: string;
	}
}

declare const envPaths: {
	/**
	Get paths for storing things like data, config, cache, etc.

	@param name - Name of your project. Used to generate the paths.
	@returns The paths to use for your project on current OS.

	@example
	```
	import envPaths = require('env-paths');

	const paths = envPaths('MyApp');

	paths.data;
	//=> '/home/sindresorhus/.local/share/MyApp-nodejs'

	paths.config
	//=> '/home/sindresorhus/.config/MyApp-nodejs'
	```
	*/
	(name: string, options?: envPaths.Options): envPaths.Paths;

	// TODO: Remove this for the next major release, refactor the whole definition to:
	// declare function envPaths(name: string, options?: envPaths.Options): envPaths.Paths;
	// export = envPaths;
	default: typeof envPaths;
};

export = envPaths;
