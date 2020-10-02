declare namespace ip {
  interface Options {
    /**
    Only match an exact string. Useful with `RegExp#test()` to check if a string is a CIDR IP address. *(`false` matches any CIDR IP address in a string)*

    @default false
    */
    readonly exact?: boolean;
  }
}

declare const ip: {
  /**
  Regular expression for matching IP addresses in CIDR notation.

  @returns A regex for matching both IPv4 and IPv6 CIDR IP addresses.

  @example
  ```
  import cidrRegex = require("cidr-regex");

  // Contains a CIDR IP address?
  cidrRegex().test("foo 192.168.0.1/24");
  //=> true

  // Is a CIDR IP address?
  cidrRegex({exact: true}).test("foo 192.168.0.1/24");
  //=> false

  "foo 192.168.0.1/24 bar 1:2:3:4:5:6:7:8/64 baz".match(cidrRegex());
  //=> ["192.168.0.1/24", "1:2:3:4:5:6:7:8/64"]
  ```
  */
  (options?: ip.Options): RegExp;

  /**
  @returns A regex for matching IPv4 CIDR IP addresses.
  */
  v4(options?: ip.Options): RegExp;

  /**
  @returns A regex for matching IPv6 CIDR IP addresses.

  @example
  ```
  import cidrRegex = require("cidr-regex");

  cidrRegex.v6({exact: true}).test("1:2:3:4:5:6:7:8/64");
  //=> true
  ```
  */
  v6(options?: ip.Options): RegExp;
};

export = ip;
