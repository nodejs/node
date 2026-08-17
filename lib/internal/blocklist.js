'use strict';

const {
  ArrayIsArray,
  Boolean,
  JSONParse,
  NumberIsNaN,
  NumberParseInt,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  StringPrototypeIncludes,
  StringPrototypeLastIndexOf,
  StringPrototypeSlice,
  StringPrototypeToLowerCase,
  Symbol,
} = primordials;

const {
  BlockList: BlockListHandle,
  AF_INET,
  AF_INET6,
} = internalBinding('block_list');

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  SocketAddress,
  kHandle: kSocketAddressHandle,
} = require('internal/socketaddress');

const {
  markTransferMode,
  kClone,
  kDeserialize,
} = require('internal/worker/js_transferable');

const { inspect } = require('internal/util/inspect');

const kHandle = Symbol('kHandle');
const { owner_symbol } = internalBinding('symbols');

const {
  ERR_INVALID_ARG_VALUE,
  ERR_INVALID_ARG_TYPE,
} = require('internal/errors').codes;

const { validateArray, validateInt32, validateString } = require('internal/validators');

function parseCIDR(cidr) {
  validateString(cidr, 'cidr');
  const slash = StringPrototypeLastIndexOf(cidr, '/');
  if (slash === -1) {
    throw new ERR_INVALID_ARG_VALUE('cidr', cidr, 'must contain a prefix length (e.g. "10.0.0.0/8")');
  }
  const address = StringPrototypeSlice(cidr, 0, slash);
  const prefixStr = StringPrototypeSlice(cidr, slash + 1);
  const prefix = NumberParseInt(prefixStr, 10);
  if (NumberIsNaN(prefix) || `${prefix}` !== prefixStr) {
    throw new ERR_INVALID_ARG_VALUE('cidr', cidr, 'prefix length must be a valid integer');
  }
  const family = StringPrototypeIncludes(address, ':') ? 'ipv6' : 'ipv4';
  return { address, prefix, family };
}

class BlockList {
  constructor() {
    markTransferMode(this, true, false);
    this[kHandle] = new BlockListHandle();
    this[kHandle][owner_symbol] = this;
  }

  /**
   * Returns true if the value is a BlockList
   * @param {any} value
   * @returns {boolean}
   */
  static isBlockList(value) {
    return value?.[kHandle] !== undefined;
  }

  static PRIVATE_RANGES = ObjectFreeze([
    // RFC 1918 - Private IPv4
    '10.0.0.0/8',
    '172.16.0.0/12',
    '192.168.0.0/16',
    // Loopback
    '127.0.0.0/8',
    '::1/128',
    // Link-local
    '169.254.0.0/16',
    'fe80::/10',
    // Unique local (ULA)
    'fc00::/7',
  ]);

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `BlockList ${inspect({
      rules: this.rules,
    }, opts)}`;
  }

  /**
   * @param {string|SocketAddress} address
   * @param {string} [family]
   */
  addAddress(address, family = 'ipv4') {
    if (!SocketAddress.isSocketAddress(address)) {
      validateString(address, 'address');
      validateString(family, 'family');
      address = new SocketAddress({
        address,
        family,
      });
    }
    this[kHandle].addAddress(address[kSocketAddressHandle]);
  }

  /**
   *
   * @param {(string|SocketAddress)[]} addresses
   * @param {string} [family]
   */
  addAddresses(addresses, family = 'ipv4') {
    validateArray(addresses, 'addresses');
    validateString(family, 'family');
    const handles = [];
    for (let i = 0; i < addresses.length; i++) {
      let address = addresses[i];
      if (!SocketAddress.isSocketAddress(address)) {
        validateString(address, `addresses[${i}]`);
        address = new SocketAddress({ address, family });
      }
      handles.push(address[kSocketAddressHandle]);
    }
    this[kHandle].addAddresses(handles);
  }

  /**
   * @param {string|SocketAddress} start
   * @param {string|SocketAddress} end
   * @param {string} [family]
   */
  addRange(start, end, family = 'ipv4') {
    if (!SocketAddress.isSocketAddress(start)) {
      validateString(start, 'start');
      validateString(family, 'family');
      start = new SocketAddress({
        address: start,
        family,
      });
    }
    if (!SocketAddress.isSocketAddress(end)) {
      validateString(end, 'end');
      validateString(family, 'family');
      end = new SocketAddress({
        address: end,
        family,
      });
    }
    const ret = this[kHandle].addRange(
      start[kSocketAddressHandle],
      end[kSocketAddressHandle]);
    if (ret === false)
      throw new ERR_INVALID_ARG_VALUE('start', start, 'must come before end');
  }

  /**
   * @param {string|SocketAddress} network
   * @param {number} prefix
   * @param {string} [family]
   */
  addSubnet(network, prefix, family = 'ipv4') {
    if (!SocketAddress.isSocketAddress(network)) {
      validateString(network, 'network');
      validateString(family, 'family');
      network = new SocketAddress({
        address: network,
        family,
      });
    }
    switch (network.family) {
      case 'ipv4':
        validateInt32(prefix, 'prefix', 0, 32);
        break;
      case 'ipv6':
        validateInt32(prefix, 'prefix', 0, 128);
        break;
    }
    // Coerce -0 to +0.
    prefix += 0;
    this[kHandle].addSubnet(network[kSocketAddressHandle], prefix);
  }

  /**
   * @param {string} cidr
   */
  addCIDR(cidr) {
    const { address, prefix, family } = parseCIDR(cidr);
    this.addSubnet(address, prefix, family);
  }

  /**
   * @param {string[]} cidrs
   */
  addCIDRs(cidrs) {
    validateArray(cidrs, 'cidrs');
    // Validate and parse all entries first so that an exception mid-array
    // does not leave the blocklist half-modified.
    const parsed = [];
    for (let i = 0; i < cidrs.length; i++) {
      validateString(cidrs[i], `cidrs[${i}]`);
      parsed.push(parseCIDR(cidrs[i]));
    }
    for (let i = 0; i < parsed.length; i++) {
      const { address, prefix, family } = parsed[i];
      this.addSubnet(address, prefix, family);
    }
  }

  /**
   * @param {string|SocketAddress} address
   * @param {string} [family]
   */
  removeAddress(address, family = 'ipv4') {
    if (!SocketAddress.isSocketAddress(address)) {
      validateString(address, 'address');
      validateString(family, 'family');
      address = new SocketAddress({
        address,
        family,
      });
    }
    this[kHandle].removeAddress(address[kSocketAddressHandle]);
  }

  /**
   * @param {string|SocketAddress} start
   * @param {string|SocketAddress} end
   * @param {string} [family]
   */
  removeRange(start, end, family = 'ipv4') {
    if (!SocketAddress.isSocketAddress(start)) {
      validateString(start, 'start');
      validateString(family, 'family');
      start = new SocketAddress({
        address: start,
        family,
      });
    }
    if (!SocketAddress.isSocketAddress(end)) {
      validateString(end, 'end');
      validateString(family, 'family');
      end = new SocketAddress({
        address: end,
        family,
      });
    }
    this[kHandle].removeRange(
      start[kSocketAddressHandle],
      end[kSocketAddressHandle]);
  }

  /**
   * @param {string|SocketAddress} network
   * @param {number} prefix
   * @param {string} [family]
   */
  removeSubnet(network, prefix, family = 'ipv4') {
    if (!SocketAddress.isSocketAddress(network)) {
      validateString(network, 'network');
      validateString(family, 'family');
      network = new SocketAddress({
        address: network,
        family,
      });
    }
    switch (network.family) {
      case 'ipv4':
        validateInt32(prefix, 'prefix', 0, 32);
        break;
      case 'ipv6':
        validateInt32(prefix, 'prefix', 0, 128);
        break;
    }
    prefix += 0;
    this[kHandle].removeSubnet(network[kSocketAddressHandle], prefix);
  }

  /**
   * @param {string} cidr
   */
  removeCIDR(cidr) {
    const { address, prefix, family } = parseCIDR(cidr);
    this.removeSubnet(address, prefix, family);
  }

  /**
   * @param {string|SocketAddress} address
   * @param {string} [family]
   * @returns {boolean}
   */
  check(address, family = 'ipv4') {
    if (!SocketAddress.isSocketAddress(address)) {
      validateString(address, 'address');
      validateString(family, 'family');
      // Fast path: pass the string directly to C++ which does
      // inet_pton + Apply() without allocating a JS SocketAddress wrapper.
      const af = StringPrototypeToLowerCase(family) === 'ipv4' ?
        AF_INET : AF_INET6;
      return this[kHandle].checkString(address, af);
    }
    return Boolean(this[kHandle].check(address[kSocketAddressHandle]));
  }

  /**
   * Removes all rules from the block list.
   */
  clear() {
    this[kHandle].clear();
  }

  /*
  * @param {string[]} data
  * @example
  * const data = [
  *   // IPv4 examples
  *   'Subnet: IPv4 192.168.1.0/24',
  *   'Address: IPv4 10.0.0.5',
  *   'Range: IPv4 192.168.2.1-192.168.2.10',
  *   'Range: IPv4 10.0.0.1-10.0.0.10',
  *
  *   // IPv6 examples
  *   'Subnet: IPv6 2001:0db8:85a3:0000:0000:8a2e:0370:7334/64',
  *   'Address: IPv6 2001:0db8:85a3:0000:0000:8a2e:0370:7334',
  *   'Range: IPv6 2001:0db8:85a3:0000:0000:8a2e:0370:7334-2001:0db8:85a3:0000:0000:8a2e:0370:7335',
  *   'Subnet: IPv6 2001:db8:1234::/48',
  *   'Address: IPv6 2001:db8:1234::1',
  *   'Range: IPv6 2001:db8:1234::1-2001:db8:1234::10'
  * ];
  */
  #parseIPInfo(data) {
    for (const item of data) {
      if (item.includes('IPv4')) {
        const subnetMatch = item.match(
          /Subnet: IPv4 (\d{1,3}(?:\.\d{1,3}){3})\/(\d{1,2})/,
        );
        if (subnetMatch) {
          const { 1: network, 2: prefix } = subnetMatch;
          this.addSubnet(network, NumberParseInt(prefix));
          continue;
        }
        const addressMatch = item.match(/Address: IPv4 (\d{1,3}(?:\.\d{1,3}){3})/);
        if (addressMatch) {
          const { 1: address } = addressMatch;
          this.addAddress(address);
          continue;
        }

        const rangeMatch = item.match(
          /Range: IPv4 (\d{1,3}(?:\.\d{1,3}){3})-(\d{1,3}(?:\.\d{1,3}){3})/,
        );
        if (rangeMatch) {
          const { 1: start, 2: end } = rangeMatch;
          this.addRange(start, end);
          continue;
        }
      }
      // IPv6 parsing with support for compressed addresses
      if (item.includes('IPv6')) {
        // IPv6 subnet pattern: supports both full and compressed formats
        // Examples:
        // - 2001:0db8:85a3:0000:0000:8a2e:0370:7334/64 (full)
        // - 2001:db8:85a3::8a2e:370:7334/64 (compressed)
        // - 2001:db8:85a3::192.0.2.128/64 (mixed)
        const ipv6SubnetMatch = item.match(
          /Subnet: IPv6 ([0-9a-fA-F:]{1,39})\/([0-9]{1,3})/i,
        );
        if (ipv6SubnetMatch) {
          const { 1: network, 2: prefix } = ipv6SubnetMatch;
          this.addSubnet(network, NumberParseInt(prefix), 'ipv6');
          continue;
        }

        // IPv6 address pattern: supports both full and compressed formats
        // Examples:
        // - 2001:0db8:85a3:0000:0000:8a2e:0370:7334 (full)
        // - 2001:db8:85a3::8a2e:370:7334 (compressed)
        // - 2001:db8:85a3::192.0.2.128 (mixed)
        const ipv6AddressMatch = item.match(/Address: IPv6 ([0-9a-fA-F:]{1,39})/i);
        if (ipv6AddressMatch) {
          const { 1: address } = ipv6AddressMatch;
          this.addAddress(address, 'ipv6');
          continue;
        }

        // IPv6 range pattern: supports both full and compressed formats
        // Examples:
        // - 2001:0db8:85a3:0000:0000:8a2e:0370:7334-2001:0db8:85a3:0000:0000:8a2e:0370:7335 (full)
        // - 2001:db8:85a3::8a2e:370:7334-2001:db8:85a3::8a2e:370:7335 (compressed)
        // - 2001:db8:85a3::192.0.2.128-2001:db8:85a3::192.0.2.129 (mixed)
        const ipv6RangeMatch = item.match(/Range: IPv6 ([0-9a-fA-F:]{1,39})-([0-9a-fA-F:]{1,39})/i);
        if (ipv6RangeMatch) {
          const { 1: start, 2: end } = ipv6RangeMatch;
          this.addRange(start, end, 'ipv6');
          continue;
        }
      }
    }
  }


  toJSON() {
    return this.rules;
  }

  fromJSON(data) {
    // The data argument must be a string, or an array of strings that
    // is JSON parseable.
    if (ArrayIsArray(data)) {
      for (const n of data) {
        if (typeof n !== 'string') {
          throw new ERR_INVALID_ARG_TYPE('data', ['string', 'string[]'], data);
        }
      }
    } else if (typeof data !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('data', ['string', 'string[]'], data);
    } else {
      data = JSONParse(data);
      if (!ArrayIsArray(data)) {
        throw new ERR_INVALID_ARG_TYPE('data', ['string', 'string[]'], data);
      }
      for (const n of data) {
        if (typeof n !== 'string') {
          throw new ERR_INVALID_ARG_TYPE('data', ['string', 'string[]'], data);
        }
      }
    }

    this.#parseIPInfo(data);
  }


  get rules() {
    return this[kHandle].getRules();
  }

  get size() {
    return this[kHandle].getSize();
  }

  [kClone]() {
    const handle = this[kHandle];
    return {
      data: { handle },
      deserializeInfo: 'internal/blocklist:InternalBlockList',
    };
  }

  [kDeserialize]({ handle }) {
    this[kHandle] = handle;
    this[kHandle][owner_symbol] = this;
  }
}

class InternalBlockList {
  constructor(handle) {
    markTransferMode(this, true, false);
    this[kHandle] = handle;
    if (handle !== undefined)
      handle[owner_symbol] = this;
  }
}

InternalBlockList.prototype.constructor = BlockList.prototype.constructor;
ObjectSetPrototypeOf(InternalBlockList.prototype, BlockList.prototype);

module.exports = {
  BlockList,
  InternalBlockList,
  kHandle,
};
