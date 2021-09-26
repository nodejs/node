'use strict';

const {
  Boolean,
  ObjectSetPrototypeOf,
  Symbol
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
  JSTransferable,
  kClone,
  kDeserialize,
} = require('internal/worker/js_transferable');

const { inspect } = require('internal/util/inspect');

const kHandle = Symbol('kHandle');
const { owner_symbol } = internalBinding('symbols');

const {
  ERR_INVALID_ARG_VALUE,
} = require('internal/errors').codes;

const { validateInt32, validateString } = require('internal/validators');

const validateAddress = (address, family, name = 'address') => {
  if (!SocketAddress.isSocketAddress(address)) {
    validateString(address, name);
    validateString(family, 'family');
    address = new SocketAddress({
      address,
      family,
    });
  }

  return address;
};

const validateSubnet = (network, prefix) => {
  switch (network.family) {
    case 'ipv4':
      validateInt32(prefix, 'prefix', 0, 32);
      break;
    case 'ipv6':
      validateInt32(prefix, 'prefix', 0, 128);
      break;
  }
};

class BlockList extends JSTransferable {
  constructor() {
    super();
    this[kHandle] = new BlockListHandle();
    this[kHandle][owner_symbol] = this;
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `BlockList ${inspect({
      rules: this.rules
    }, opts)}`;
  }

  addAddress(address, family = 'ipv4') {
    address = validateAddress(address, family);
    this[kHandle].addAddress(address[kSocketAddressHandle]);
  }

  removeAddress(address, family = 'ipv4') {
    address = validateAddress(address, family);
    this[kHandle].removeAddress(address[kSocketAddressHandle]);
  }

  addRange(start, end, family = 'ipv4') {
    start = validateAddress(start, family, 'start');
    end = validateAddress(end, family, 'end');
    const ret = this[kHandle].addRange(
      start[kSocketAddressHandle],
      end[kSocketAddressHandle]);
    if (ret === false)
      throw new ERR_INVALID_ARG_VALUE('start', start, 'must come before end');
  }

  removeRange(start, end, family = 'ipv4') {
    start = validateAddress(start, family, 'start');
    end = validateAddress(end, family, 'end');
    const ret = this[kHandle].removeRange(
      start[kSocketAddressHandle],
      end[kSocketAddressHandle]);
    if (ret === false)
      throw new ERR_INVALID_ARG_VALUE('start', start, 'must come before end');
  }

  addSubnet(network, prefix, family = 'ipv4') {
    network = validateAddress(network, family);
    validateSubnet(network, prefix);
    this[kHandle].addSubnet(network[kSocketAddressHandle], prefix);
  }

  removeSubnet(network, prefix, family = 'ipv4') {
    network = validateAddress(network, family);
    validateSubnet(network, prefix);
    this[kHandle].removeSubnet(network[kSocketAddressHandle], prefix);
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

class InternalBlockList extends JSTransferable {
  constructor(handle) {
    super();
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
