'use strict';

const {
  ArrayIsArray,
  Boolean,
  JSONParse,
  NumberParseInt,
  ObjectSetPrototypeOf,
  Symbol,
} = primordials;

const {
  BlockList: BlockListHandle,
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

const { validateInt32, validateString } = require('internal/validators');

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
    this[kHandle].addSubnet(network[kSocketAddressHandle], prefix);
  }

  check(address, family = 'ipv4') {
    if (!SocketAddress.isSocketAddress(address)) {
      validateString(address, 'address');
      validateString(family, 'family');
      try {
        address = new SocketAddress({
          address,
          family,
        });
      } catch {
        // Ignore the error. If it's not a valid address, return false.
        return false;
      }
    }
    return Boolean(this[kHandle].check(address[kSocketAddressHandle]));
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
    for (let i = 0; i < data.length; i++) {
      const item = data[i];
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
      for (let i = 0; i < data.length; i++) {
        const n = data[i];
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
      for (let i = 0; i < data.length; i++) {
        const n = data[i];
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
};
