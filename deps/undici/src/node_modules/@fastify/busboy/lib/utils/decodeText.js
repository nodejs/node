'use strict'

// Node has always utf-8
const utf8Decoder = new TextDecoder('utf-8')
const textDecoders = new Map([
  ['utf-8', utf8Decoder],
  ['utf8', utf8Decoder]
])

function decodeText (text, textEncoding, destEncoding) {
  if (text) {
    if (textDecoders.has(destEncoding)) {
      try {
        return textDecoders.get(destEncoding).decode(Buffer.from(text, textEncoding))
      } catch (e) { }
    } else {
      try {
        textDecoders.set(destEncoding, new TextDecoder(destEncoding))
        return textDecoders.get(destEncoding).decode(Buffer.from(text, textEncoding))
      } catch (e) { }
    }
  }
  return text
}

module.exports = decodeText
