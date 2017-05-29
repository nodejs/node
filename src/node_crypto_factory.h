#ifndef SRC_NODE_CRYPTO_FACTORY_H_
#define SRC_NODE_CRYPTO_FACTORY_H_
#include "node_crypto.h"
#include "env.h"
#include <string>
#include <map>

namespace node {
namespace crypto {

/**
 * A Factory responsible for registering, unregistering and creating
 * concrete Crypto instances.
 */
class CryptoFactory {
 public:
  typedef Crypto* (*CreateCallback)();
  /**
   * Registers a Crypto implementation for the specified version.
   */
  static void Register(const std::string& version, CreateCallback cb);
  /**
   * Unregisters a the Crypto implementation identified with version.
   */
  static void Unregister(const std::string& version);
  /**
   * Returns a crypto instance for the specified version.
   */
  static std::shared_ptr<Crypto> Get(const std::string& version,
                                     Environment* env = nullptr);
 private:
  typedef std::map<std::string, std::shared_ptr<Crypto>> CallbackMap;
  static CallbackMap crypto_libs;
};

}  //  namespace crypto
}  //  namespace node

#endif  //  SRC_NODE_CRYPTO_FACTORY_H_
