'use strict';

const {
  Boolean,
  Symbol
} = primordials;

const {
  BlockList: BlockListHandle,
  AF_INET,
  AF_INET6,
} = internalBinding('block_list');

const {
  customInspectSymbol: kInspect,
} = require('internal/util');
const { inspect } = require('internal/util/inspect');

const kHandle = Symbol('kHandle');
const { owner_symbol } = internalBinding('symbols');

const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
} = require('internal/errors').codes;

const { validateInt32, validateString } = require('internal/validators');

class BlockList {
  constructor(handle = new BlockListHandle()) {
    // The handle argument is an intentionally undocumented
    // internal API. User code will not be able to create
    // a BlockListHandle object directly.
    if (!(handle instanceof BlockListHandle))
      throw new ERR_INVALID_ARG_TYPE('handle', 'BlockListHandle', handle);
    this[kHandle] = handle;
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
    validateString(address, 'address');
    validateString(family, 'family');
    family = family.toLowerCase();
    if (family !== 'ipv4' && family !== 'ipv6')
      throw new ERR_INVALID_ARG_VALUE('family', family);
    const type = family === 'ipv4' ? AF_INET : AF_INET6;
    this[kHandle].addAddress(address, type);
  }

  addRange(start, end, family = 'ipv4') {
    validateString(start, 'start');
    validateString(end, 'end');
    validateString(family, 'family');
    family = family.toLowerCase();
    if (family !== 'ipv4' && family !== 'ipv6')
      throw new ERR_INVALID_ARG_VALUE('family', family);
    const type = family === 'ipv4' ? AF_INET : AF_INET6;
    const ret = this[kHandle].addRange(start, end, type);
    if (ret === false)
      throw new ERR_INVALID_ARG_VALUE('start', start, 'must come before end');
  }

  addSubnet(network, prefix, family = 'ipv4') {
    validateString(network, 'network');
    validateString(family, 'family');
    family = family.toLowerCase();
    let type;
    switch (family) {
      case 'ipv4':
        type = AF_INET;
        validateInt32(prefix, 'prefix', 0, 32);
        break;
      case 'ipv6':
        type = AF_INET6;
        validateInt32(prefix, 'prefix', 0, 128);
        break;
      default:
        throw new ERR_INVALID_ARG_VALUE('family', family);
    }
    this[kHandle].addSubnet(network, type, prefix);
  }

  check(address, family = 'ipv4') {
    validateString(address, 'address');
    validateString(family, 'family');
    family = family.toLowerCase();
    if (family !== 'ipv4' && family !== 'ipv6')
      throw new ERR_INVALID_ARG_VALUE('family', family);
    const type = family === 'ipv4' ? AF_INET : AF_INET6;
    return Boolean(this[kHandle].check(address, type));
  }

  get rules() {
    return this[kHandle].getRules();
  }
}

module.exports = BlockList;
