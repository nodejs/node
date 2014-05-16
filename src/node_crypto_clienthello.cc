// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node_crypto_clienthello.h"
#include "node_crypto_clienthello-inl.h"
#include "node_buffer.h"  // Buffer

namespace node {

void ClientHelloParser::Parse(const uint8_t* data, size_t avail) {
  switch (state_) {
    case kWaiting:
      if (!ParseRecordHeader(data, avail))
        break;
      // Fall through
    case kTLSHeader:
    case kSSL2Header:
      ParseHeader(data, avail);
      break;
    case kPaused:
      // Just nop
    case kEnded:
      // Already ended, just ignore it
      break;
    default:
      break;
  }
}


bool ClientHelloParser::ParseRecordHeader(const uint8_t* data, size_t avail) {
  // >= 5 bytes for header parsing
  if (avail < 5)
    return false;

  if (data[0] == kChangeCipherSpec ||
      data[0] == kAlert ||
      data[0] == kHandshake ||
      data[0] == kApplicationData) {
    frame_len_ = (data[3] << 8) + data[4];
    state_ = kTLSHeader;
    body_offset_ = 5;
  } else {
#ifdef OPENSSL_NO_SSL2
    frame_len_ = ((data[0] << 8) & kSSL2HeaderMask) + data[1];
    state_ = kSSL2Header;
    if (data[0] & kSSL2TwoByteHeaderBit) {
      // header without padding
      body_offset_ = 2;
    } else {
      // header with padding
      body_offset_ = 3;
    }
#else
    End();
    return false;
#endif  // OPENSSL_NO_SSL2
  }

  // Sanity check (too big frame, or too small)
  // Let OpenSSL handle it
  if (frame_len_ >= kMaxTLSFrameLen) {
    End();
    return false;
  }

  return true;
}

#ifdef OPENSSL_NO_SSL2
# define NODE_SSL2_VER_CHECK(buf) false
#else
# define NODE_SSL2_VER_CHECK(buf) ((buf)[0] == 0x00 && (buf)[1] == 0x02)
#endif  // OPENSSL_NO_SSL2


void ClientHelloParser::ParseHeader(const uint8_t* data, size_t avail) {
  ClientHello hello;

  // >= 5 + frame size bytes for frame parsing
  if (body_offset_ + frame_len_ > avail)
    return;

  // Skip unsupported frames and gather some data from frame
  // Check hello protocol version
  if (!(data[body_offset_ + 4] == 0x03 && data[body_offset_ + 5] <= 0x03) &&
      !NODE_SSL2_VER_CHECK(data + body_offset_ + 4)) {
    goto fail;
  }

  if (data[body_offset_] == kClientHello) {
    if (state_ == kTLSHeader) {
      if (!ParseTLSClientHello(data, avail))
        goto fail;
    } else if (state_ == kSSL2Header) {
#ifdef OPENSSL_NO_SSL2
      if (!ParseSSL2ClientHello(data, avail))
        goto fail;
#else
      abort();  // Unreachable
#endif  // OPENSSL_NO_SSL2
    } else {
      // We couldn't get here, but whatever
      goto fail;
    }

    // Check if we overflowed (do not reply with any private data)
    if (session_id_ == NULL ||
        session_size_ > 32 ||
        session_id_ + session_size_ > data + avail) {
      goto fail;
    }
  }

  state_ = kPaused;
  hello.session_id_ = session_id_;
  hello.session_size_ = session_size_;
  hello.has_ticket_ = tls_ticket_ != NULL && tls_ticket_size_ != 0;
  hello.ocsp_request_ = ocsp_request_;
  hello.servername_ = servername_;
  hello.servername_size_ = static_cast<uint8_t>(servername_size_);
  onhello_cb_(cb_arg_, hello);
  return;

 fail:
  return End();
}


#undef NODE_SSL2_VER_CHECK


void ClientHelloParser::ParseExtension(ClientHelloParser::ExtensionType type,
                                       const uint8_t* data,
                                       size_t len) {
  // NOTE: In case of anything we're just returning back, ignoring the problem.
  // That's because we're heavily relying on OpenSSL to solve any problem with
  // incoming data.
  switch (type) {
    case kServerName:
      {
        if (len < 2)
          return;
        uint32_t server_names_len = (data[0] << 8) + data[1];
        if (server_names_len + 2 > len)
          return;
        for (size_t offset = 2; offset < 2 + server_names_len; ) {
          if (offset + 3 > len)
            return;
          uint8_t name_type = data[offset];
          if (name_type != kServernameHostname)
            return;
          uint16_t name_len = (data[offset + 1] << 8) + data[offset + 2];
          offset += 3;
          if (offset + name_len > len)
            return;
          servername_ = data + offset;
          servername_size_ = name_len;
          offset += name_len;
        }
      }
      break;
    case kStatusRequest:
      // We are ignoring any data, just indicating the presence of extension
      if (len < kMinStatusRequestSize)
        return;

      // Unknown type, ignore it
      if (data[0] != kStatusRequestOCSP)
        break;

      // Ignore extensions, they won't work with caching on backend anyway
      ocsp_request_ = 1;
      break;
    case kTLSSessionTicket:
      tls_ticket_size_ = len;
      tls_ticket_ = data + len;
      break;
    default:
      // Ignore
      break;
  }
}


bool ClientHelloParser::ParseTLSClientHello(const uint8_t* data, size_t avail) {
  const uint8_t* body;

  // Skip frame header, hello header, protocol version and random data
  size_t session_offset = body_offset_ + 4 + 2 + 32;

  if (session_offset + 1 >= avail)
    return false;

  body = data + session_offset;
  session_size_ = *body;
  session_id_ = body + 1;

  size_t cipher_offset = session_offset + 1 + session_size_;

  // Session OOB failure
  if (cipher_offset + 1 >= avail)
    return false;

  uint16_t cipher_len =
      (data[cipher_offset] << 8) + data[cipher_offset + 1];
  size_t comp_offset = cipher_offset + 2 + cipher_len;

  // Cipher OOB failure
  if (comp_offset >= avail)
    return false;

  uint8_t comp_len = data[comp_offset];
  size_t extension_offset = comp_offset + 1 + comp_len;

  // Compression OOB failure
  if (extension_offset > avail)
    return false;

  // No extensions present
  if (extension_offset == avail)
    return true;

  size_t ext_off = extension_offset + 2;

  // Parse known extensions
  while (ext_off < avail) {
    // Extension OOB
    if (ext_off + 4 > avail)
      return false;

    uint16_t ext_type = (data[ext_off] << 8) + data[ext_off + 1];
    uint16_t ext_len = (data[ext_off + 2] << 8) + data[ext_off + 3];
    ext_off += 4;

    // Extension OOB
    if (ext_off + ext_len > avail)
      return false;

    ParseExtension(static_cast<ExtensionType>(ext_type),
                   data + ext_off,
                   ext_len);

    ext_off += ext_len;
  }

  // Extensions OOB failure
  if (ext_off > avail)
    return false;

  return true;
}


#ifdef OPENSSL_NO_SSL2
bool ClientHelloParser::ParseSSL2ClientHello(const uint8_t* data,
                                             size_t avail) {
  const uint8_t* body;

  // Skip header, version
  size_t session_offset = body_offset_ + 3;

  if (session_offset + 4 < avail) {
    body = data + session_offset;

    uint16_t ciphers_size = (body[0] << 8) + body[1];

    if (body + 4 + ciphers_size < data + avail) {
      session_size_ = (body[2] << 8) + body[3];
      session_id_ = body + 4 + ciphers_size;
    }
  }

  return true;
}
#endif  // OPENSSL_NO_SSL2

}  // namespace node
