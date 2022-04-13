declare const isCidr: {
  /**
  Check if `string` is a IPv4 or IPv6 CIDR address.
  @returns Either `4`, `6` (indicating the IP version) or `0` if the string is not a CIDR.
  @example
  ```
  import isCidr = require('is-cidr');
  isCidr('192.168.0.1/24'); //=> 4
  isCidr('1:2:3:4:5:6:7:8/64'); //=> 6
  isCidr('10.0.0.0'); //=> 0
  ```
  */
  (string: string): 6 | 4 | 0;

  /**
  Check if `string` is a IPv4 CIDR address.
  */
  v4(string: string): boolean;

  /**
  Check if `string` is a IPv6 CIDR address.
  @example
  ```
  import isCidr = require('is-cidr');
  isCidr.v6('10.0.0.0/24'); //=> false
  ```
  */
  v6(string: string): boolean;
};

export = isCidr;
