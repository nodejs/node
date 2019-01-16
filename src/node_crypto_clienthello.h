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

#ifndef SRC_NODE_CRYPTO_CLIENTHELLO_H_
#define SRC_NODE_CRYPTO_CLIENTHELLO_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <stddef.h>  // size_t
#include <stdint.h>

namespace node {
namespace crypto {

// Parse the client hello so we can do async session resumption. OpenSSL's
// session resumption uses synchronous callbacks, see SSL_CTX_sess_set_get_cb
// and get_session_cb.
class ClientHelloParser {
 public:
  inline ClientHelloParser();

  class ClientHello {
   public:
    inline uint8_t session_size() const { return session_size_; }
    inline const uint8_t* session_id() const { return session_id_; }
    inline bool has_ticket() const { return has_ticket_; }
    inline uint8_t servername_size() const { return servername_size_; }
    inline const uint8_t* servername() const { return servername_; }

   private:
    uint8_t session_size_;
    const uint8_t* session_id_;
    bool has_ticket_;
    uint8_t servername_size_;
    const uint8_t* servername_;

    friend class ClientHelloParser;
  };

  typedef void (*OnHelloCb)(void* arg, const ClientHello& hello);
  typedef void (*OnEndCb)(void* arg);

  void Parse(const uint8_t* data, size_t avail);

  inline void Reset();
  inline void Start(OnHelloCb onhello_cb, OnEndCb onend_cb, void* onend_arg);
  inline void End();
  inline bool IsPaused() const;
  inline bool IsEnded() const;

 private:
  static const size_t kMaxTLSFrameLen = 16 * 1024 + 5;
  static const size_t kMaxSSLExFrameLen = 32 * 1024;
  static const uint8_t kServernameHostname = 0;
  static const size_t kMinStatusRequestSize = 5;

  enum ParseState {
    kWaiting,
    kTLSHeader,
    kPaused,
    kEnded
  };

  enum FrameType {
    kChangeCipherSpec = 20,
    kAlert = 21,
    kHandshake = 22,
    kApplicationData = 23,
    kOther = 255
  };

  enum HandshakeType {
    kClientHello = 1
  };

  enum ExtensionType {
    kServerName = 0,
    kTLSSessionTicket = 35
  };

  bool ParseRecordHeader(const uint8_t* data, size_t avail);
  void ParseHeader(const uint8_t* data, size_t avail);
  void ParseExtension(const uint16_t type,
                      const uint8_t* data,
                      size_t len);
  bool ParseTLSClientHello(const uint8_t* data, size_t avail);

  ParseState state_;
  OnHelloCb onhello_cb_;
  OnEndCb onend_cb_;
  void* cb_arg_;
  size_t frame_len_ = 0;
  size_t body_offset_ = 0;
  size_t extension_offset_ = 0;
  uint8_t session_size_ = 0;
  const uint8_t* session_id_ = nullptr;
  uint16_t servername_size_ = 0;
  const uint8_t* servername_ = nullptr;
  uint16_t tls_ticket_size_ = -1;
  const uint8_t* tls_ticket_ = nullptr;
};

}  // namespace crypto
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CRYPTO_CLIENTHELLO_H_
