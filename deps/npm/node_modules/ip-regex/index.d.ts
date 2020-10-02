declare namespace ip {
	interface Options {
		/**
		Only match an exact string. Useful with `RegExp#test()` to check if a string is an IP address. *(`false` matches any IP address in a string)*

		@default false
		*/
		readonly exact?: boolean;

		/**
		Include boundaries in the regex. When `true`, `192.168.0.2000000000` will report as an invalid IPv4 address. If this option is not set, the mentioned IPv4 address would report as valid (ignoring the trailing zeros).

		@default false
		*/
		readonly includeBoundaries?: boolean;
	}
}

declare const ip: {
	/**
	Regular expression for matching IP addresses.

	@returns A regex for matching both IPv4 and IPv6.

	@example
	```
	import ipRegex = require('ip-regex');

	// Contains an IP address?
	ipRegex().test('unicorn 192.168.0.1');
	//=> true

	// Is an IP address?
	ipRegex({exact: true}).test('unicorn 192.168.0.1');
	//=> false

	'unicorn 192.168.0.1 cake 1:2:3:4:5:6:7:8 rainbow'.match(ipRegex());
	//=> ['192.168.0.1', '1:2:3:4:5:6:7:8']

	// Contains an IP address?
	ipRegex({includeBoundaries: true}).test('192.168.0.2000000000');
	//=> false

	// Matches an IP address?
	'192.168.0.2000000000'.match(ipRegex({includeBoundaries: true}));
	//=> null
	```
	*/
	(options?: ip.Options): RegExp;

	/**
	@returns A regex for matching IPv4.
	*/
	v4(options?: ip.Options): RegExp;

	/**
	@returns A regex for matching IPv6.

	@example
	```
	import ipRegex = require('ip-regex');

	ipRegex.v6({exact: true}).test('1:2:3:4:5:6:7:8');
	//=> true
	```
	*/
	v6(options?: ip.Options): RegExp;
};

export = ip;
