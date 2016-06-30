#ifndef SRC_NODE_CRYPTO_CLIENTHELLO_H_
#define SRC_NODE_CRYPTO_CLIENTHELLO_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"

#include <stddef.h>  // size_t
#include <stdlib.h>  // nullptr

namespace node {

class ClientHelloParser {
 public:
  ClientHelloParser() : state_(kEnded),
                        onhello_cb_(nullptr),
                        onend_cb_(nullptr),
                        cb_arg_(nullptr),
                        session_size_(0),
                        session_id_(nullptr),
                        servername_size_(0),
                        servername_(nullptr),
                        ocsp_request_(0),
                        tls_ticket_size_(0),
                        tls_ticket_(nullptr) {
    Reset();
  }

  class ClientHello {
   public:
    inline uint8_t session_size() const { return session_size_; }
    inline const uint8_t* session_id() const { return session_id_; }
    inline bool has_ticket() const { return has_ticket_; }
    inline uint8_t servername_size() const { return servername_size_; }
    inline const uint8_t* servername() const { return servername_; }
    inline int ocsp_request() const { return ocsp_request_; }

   private:
    uint8_t session_size_;
    const uint8_t* session_id_;
    bool has_ticket_;
    uint8_t servername_size_;
    const uint8_t* servername_;
    int ocsp_request_;

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
  static const uint8_t kStatusRequestOCSP = 1;
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
    kStatusRequest = 5,
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
  size_t frame_len_;
  size_t body_offset_;
  size_t extension_offset_;
  uint8_t session_size_;
  const uint8_t* session_id_;
  uint16_t servername_size_;
  const uint8_t* servername_;
  uint8_t ocsp_request_;
  uint16_t tls_ticket_size_;
  const uint8_t* tls_ticket_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CRYPTO_CLIENTHELLO_H_
