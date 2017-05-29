#ifndef SRC_NODE_CRYPTO_H_
#define SRC_NODE_CRYPTO_H_
#include <string>
#include "v8.h"  // NOLINT(build/include_order)

namespace node {
namespace crypto {

class Crypto {
 public:
  virtual ~Crypto() {}
  __attribute__((constructor)) virtual void RegisterCrypto();
  __attribute__((destructor)) virtual void UnregisterCrypto();
  /**
   * Allows a crypto implementation to do any additional initializations
   * required.
   */
  virtual void Init() {}
  /**
   */
  virtual void UseExtraCaCerts(const std::string& file) = 0;
  /**
   */
  virtual v8::EntropySource GetEntropySource() = 0;
  /**
   * The version of the underlying Crypto library
   */
  virtual const std::string Version() = 0;
  /**
   * The name of this crypto implemenetion.
   * This is currently used as the attribute name on the process object.
   */
  virtual const std::string Name() = 0;
  /**
   * Returns true if TLS Name Server Indication (SNI) is supported.
   */
  virtual bool HasSNI() = 0;
  /**
   * Returns true if the TLS Next Protocol Negotiation extension is supprted.
   */
  virtual bool HasNPN() = 0;
  /**
   * Returns true if the TLS Application-Layer Protocol Negotiation extension
   * is supprted.
   */
  virtual bool HasALPN() = 0;
  /**
   * Returns true if Online Certificate Status Protocol is supported.
   */
  virtual bool HasOCSP() = 0;
};

}  //  namespace crypto
}  //  namespace node

#endif  //  SRC_NODE_CRYPTO_H_
