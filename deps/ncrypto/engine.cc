#include "ncrypto.h"

#if !defined(OPENSSL_NO_ENGINE) &&                                             \
    ((defined(NCRYPTO_ENGINE_COMPAT) && NCRYPTO_ENGINE_COMPAT) ||              \
     NCRYPTO_USE_LEGACY_OPENSSL)
#include <openssl/engine.h>
#endif

namespace ncrypto {

// ============================================================================
// Engine

#ifndef OPENSSL_NO_ENGINE
EnginePointer::EnginePointer(void* engine_, bool finish_on_exit_)
    : engine(engine_), finish_on_exit(finish_on_exit_) {}

EnginePointer::EnginePointer(EnginePointer&& other) noexcept
    : engine(other.engine), finish_on_exit(other.finish_on_exit) {
  other.release();
}

EnginePointer::~EnginePointer() {
  reset();
}

EnginePointer& EnginePointer::operator=(EnginePointer&& other) noexcept {
  if (this == &other) return *this;
  this->~EnginePointer();
  return *new (this) EnginePointer(std::move(other));
}

void EnginePointer::reset(void* engine_, bool finish_on_exit_) {
  if (engine != nullptr) {
    ENGINE* current = static_cast<ENGINE*>(engine);
    if (finish_on_exit) {
      // This also does the equivalent of ENGINE_free.
      ENGINE_finish(current);
    } else {
      ENGINE_free(current);
    }
  }
  engine = engine_;
  finish_on_exit = finish_on_exit_;
}

void* EnginePointer::release() {
  void* ret = engine;
  engine = nullptr;
  finish_on_exit = false;
  return ret;
}

EnginePointer EnginePointer::getEngineByName(const char* name,
                                             CryptoErrorList* errors) {
  MarkPopErrorOnReturn mark_pop_error_on_return(errors);
  EnginePointer engine(ENGINE_by_id(name));
  if (!engine) {
    // Engine not found, try loading dynamically.
    engine = EnginePointer(ENGINE_by_id("dynamic"));
    if (engine) {
      ENGINE* current = static_cast<ENGINE*>(engine.engine);
      if (!ENGINE_ctrl_cmd_string(current, "SO_PATH", name, 0) ||
          !ENGINE_ctrl_cmd_string(current, "LOAD", nullptr, 0)) {
        engine.reset();
      }
    }
  }
  return engine;
}

bool EnginePointer::setAsDefault(uint32_t flags, CryptoErrorList* errors) {
  if (engine == nullptr) return false;
  ClearErrorOnReturn clear_error_on_return(errors);
  return ENGINE_set_default(static_cast<ENGINE*>(engine), flags) != 0;
}

bool EnginePointer::init(bool finish_on_exit) {
  if (engine == nullptr) return false;
  if (finish_on_exit) setFinishOnExit();
  return ENGINE_init(static_cast<ENGINE*>(engine)) == 1;
}

EVPKeyPointer EnginePointer::loadPrivateKey(const char* key_name) {
  if (engine == nullptr) return EVPKeyPointer();
  return EVPKeyPointer(ENGINE_load_private_key(
      static_cast<ENGINE*>(engine), key_name, nullptr, nullptr));
}

bool EnginePointer::setClientCertEngine(SSL_CTX* ctx) {
  if (engine == nullptr || ctx == nullptr) return false;
  return SSL_CTX_set_client_cert_engine(ctx, static_cast<ENGINE*>(engine)) == 1;
}

void EnginePointer::initEnginesOnce() {
  static bool initialized = false;
  if (!initialized) {
    ENGINE_load_builtin_engines();
    ENGINE_register_all_complete();
    initialized = true;
  }
}

#endif  // OPENSSL_NO_ENGINE

}  // namespace ncrypto
