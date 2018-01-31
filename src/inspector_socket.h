#ifndef SRC_INSPECTOR_SOCKET_H_
#define SRC_INSPECTOR_SOCKET_H_

#include "util-inl.h"
#include "uv.h"

#include <string>
#include <vector>

namespace node {
namespace inspector {

class ProtocolHandler;

// HTTP Wrapper around a uv_tcp_t
class InspectorSocket {
 public:
  class Delegate {
   public:
    virtual void OnHttpGet(const std::string& path) = 0;
    virtual void OnSocketUpgrade(const std::string& path,
                                 const std::string& accept_key) = 0;
    virtual void OnWsFrame(const std::vector<char>& frame) = 0;
    virtual ~Delegate() {}
  };

  using DelegatePointer = std::unique_ptr<Delegate>;
  using Pointer = std::unique_ptr<InspectorSocket>;

  static Pointer Accept(uv_stream_t* server, DelegatePointer delegate);

  ~InspectorSocket();

  void AcceptUpgrade(const std::string& accept_key);
  void CancelHandshake();
  void Write(const char* data, size_t len);
  void SwitchProtocol(ProtocolHandler* handler);
  std::string GetHost();

 private:
  InspectorSocket();

  std::unique_ptr<ProtocolHandler, void(*)(ProtocolHandler*)> protocol_handler_;

  DISALLOW_COPY_AND_ASSIGN(InspectorSocket);
};


}  // namespace inspector
}  // namespace node


#endif  // SRC_INSPECTOR_SOCKET_H_
