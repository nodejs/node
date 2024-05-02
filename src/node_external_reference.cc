#include "node_external_reference.h"
#include <cinttypes>
#include <vector>
#include "base_object-inl.h"
#include "util.h"

namespace node {

const std::vector<intptr_t>& ExternalReferenceRegistry::external_references() {
  if (!is_finalized_) {
    external_references_.push_back(reinterpret_cast<intptr_t>(nullptr));
    is_finalized_ = true;
  }

  return external_references_;
}

ExternalReferenceRegistry::ExternalReferenceRegistry() {
  this->Register(BaseObject::LazilyInitializedJSTemplateConstructor);

#define V(modname) _register_external_reference_##modname(this);
  EXTERNAL_REFERENCE_BINDING_LIST(V)
#undef V
  // TODO(joyeecheung): collect more external references here.
}

}  // namespace node
