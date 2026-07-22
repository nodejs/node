'use strict'

const { Buffer } = require('node:buffer')
const net = require('node:net')
const { InvalidArgumentError } = require('./errors')

/**
 * Parse an address and determine its type
 * @param {string} address - The address to parse
 * @returns {{type: number, buffer: Buffer}} Address type and buffer
 */
function parseAddress (address) {
  // Check if it's an IPv4 address
  if (net.isIPv4(address)) {
    const parts = address.split('.').map(Number)
    return {
      type: 0x01, // IPv4
      buffer: Buffer.from(parts)
    }
  }

  // Check if it's an IPv6 address
  if (net.isIPv6(address)) {
    return {
      type: 0x04, // IPv6
      buffer: parseIPv6(address)
    }
  }

  // Otherwise, treat as domain name
  const domainBuffer = Buffer.from(address, 'utf8')
  if (domainBuffer.length > 255) {
    throw new InvalidArgumentError('Domain name too long (max 255 bytes)')
  }

  return {
    type: 0x03, // Domain
    buffer: Buffer.concat([Buffer.from([domainBuffer.length]), domainBuffer])
  }
}

/**
 * Parse IPv6 address to buffer
 * @param {string} address - IPv6 address string
 * @returns {Buffer} 16-byte buffer
 */
function parseIPv6 (address) {
  const buffer = Buffer.alloc(16)
  let normalizedAddress = address

  // Expand an embedded IPv4 tail into the last two IPv6 groups.
  if (address.includes('.')) {
    const lastColonIndex = address.lastIndexOf(':')
    const ipv4Part = address.slice(lastColonIndex + 1)

    if (net.isIPv4(ipv4Part)) {
      const octets = ipv4Part.split('.').map(Number)
      const high = ((octets[0] << 8) | octets[1]).toString(16)
      const low = ((octets[2] << 8) | octets[3]).toString(16)
      normalizedAddress = `${address.slice(0, lastColonIndex)}:${high}:${low}`
    }
  }

  // Handle compressed notation (::)
  const doubleColonIndex = normalizedAddress.indexOf('::')
  if (doubleColonIndex !== -1) {
    const before = normalizedAddress.slice(0, doubleColonIndex)
    const after = normalizedAddress.slice(doubleColonIndex + 2)
    const beforeParts = before === '' ? [] : before.split(':')
    const afterParts = after === '' ? [] : after.split(':')

    let bufferIndex = 0
    for (const part of beforeParts) {
      buffer.writeUInt16BE(parseInt(part, 16), bufferIndex)
      bufferIndex += 2
    }
    bufferIndex = 16 - afterParts.length * 2
    for (const part of afterParts) {
      buffer.writeUInt16BE(parseInt(part, 16), bufferIndex)
      bufferIndex += 2
    }
  } else {
    const parts = normalizedAddress.split(':')
    for (let i = 0; i < parts.length; i++) {
      buffer.writeUInt16BE(parseInt(parts[i], 16), i * 2)
    }
  }

  return buffer
}

/**
 * Build a SOCKS5 address buffer
 * @param {number} type - Address type (1=IPv4, 3=Domain, 4=IPv6)
 * @param {Buffer} addressBuffer - The address data
 * @param {number} port - Port number
 * @returns {Buffer} Complete address buffer including type, address, and port
 */
function buildAddressBuffer (type, addressBuffer, port) {
  const portBuffer = Buffer.allocUnsafe(2)
  portBuffer.writeUInt16BE(port, 0)

  return Buffer.concat([
    Buffer.from([type]),
    addressBuffer,
    portBuffer
  ])
}

/**
 * Parse address from SOCKS5 response
 * @param {Buffer} buffer - Buffer containing the address
 * @param {number} offset - Starting offset in buffer
 * @returns {{address: string, port: number, bytesRead: number}}
 */
function parseResponseAddress (buffer, offset = 0) {
  if (buffer.length < offset + 1) {
    throw new InvalidArgumentError('Buffer too small to contain address type')
  }

  const addressType = buffer[offset]
  let address
  let currentOffset = offset + 1

  switch (addressType) {
    case 0x01: { // IPv4
      if (buffer.length < currentOffset + 6) {
        throw new InvalidArgumentError('Buffer too small for IPv4 address')
      }
      address = Array.from(buffer.subarray(currentOffset, currentOffset + 4)).join('.')
      currentOffset += 4
      break
    }

    case 0x03: { // Domain
      if (buffer.length < currentOffset + 1) {
        throw new InvalidArgumentError('Buffer too small for domain length')
      }
      const domainLength = buffer[currentOffset]
      currentOffset += 1

      if (buffer.length < currentOffset + domainLength + 2) {
        throw new InvalidArgumentError('Buffer too small for domain address')
      }
      address = buffer.subarray(currentOffset, currentOffset + domainLength).toString('utf8')
      currentOffset += domainLength
      break
    }

    case 0x04: { // IPv6
      if (buffer.length < currentOffset + 18) {
        throw new InvalidArgumentError('Buffer too small for IPv6 address')
      }
      // Convert buffer to IPv6 string
      const parts = []
      for (let i = 0; i < 8; i++) {
        const value = buffer.readUInt16BE(currentOffset + i * 2)
        parts.push(value.toString(16))
      }
      address = parts.join(':')
      currentOffset += 16
      break
    }

    default:
      throw new InvalidArgumentError(`Invalid address type: ${addressType}`)
  }

  // Parse port
  if (buffer.length < currentOffset + 2) {
    throw new InvalidArgumentError('Buffer too small for port')
  }
  const port = buffer.readUInt16BE(currentOffset)
  currentOffset += 2

  return {
    address,
    port,
    bytesRead: currentOffset - offset
  }
}

/**
 * Create error for SOCKS5 reply code
 * @param {number} replyCode - SOCKS5 reply code
 * @returns {Error} Appropriate error object
 */
function createReplyError (replyCode) {
  const messages = {
    0x01: 'General SOCKS server failure',
    0x02: 'Connection not allowed by ruleset',
    0x03: 'Network unreachable',
    0x04: 'Host unreachable',
    0x05: 'Connection refused',
    0x06: 'TTL expired',
    0x07: 'Command not supported',
    0x08: 'Address type not supported'
  }

  const message = messages[replyCode] || `Unknown SOCKS5 error code: ${replyCode}`
  const error = new Error(message)
  error.code = `SOCKS5_${replyCode}`
  return error
}

module.exports = {
  parseAddress,
  parseIPv6,
  buildAddressBuffer,
  parseResponseAddress,
  createReplyError
}
