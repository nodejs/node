/* Copyright 2022 - 2025 R. Thomas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_OBJC_METADATA_H
#define LIEF_OBJC_METADATA_H
#include <LIEF/visibility.h>
#include <LIEF/ObjC/Class.hpp>
#include <LIEF/ObjC/Protocol.hpp>

#include <LIEF/iterators.hpp>
#include <LIEF/ObjC/DeclOpt.hpp>

#include <memory>

namespace LIEF {
/// Namespace related to ObjC metadata
namespace objc {

namespace details {
class Metadata;
}

/// This class is the main interface to inspect Objective-C metadata
///
/// It can be instantiated using the function LIEF::MachO::Binary::objc_metadata
class LIEF_API Metadata {
  public:
  using classes_it = iterator_range<Class::Iterator>;
  using protocols_it = iterator_range<Protocol::Iterator>;

  Metadata(std::unique_ptr<details::Metadata> impl);

  /// Return an iterator over the different Objective-C classes (`@interface`)
  classes_it classes() const;

  /// Return an iterator over the Objective-C protocols declared in this binary (`@protocol`).
  protocols_it protocols() const;

  /// Try to find the Objective-C class with the given **mangled** name
  std::unique_ptr<Class> get_class(const std::string& name) const;

  /// Try to find the Objective-C protocol with the given **mangled** name
  std::unique_ptr<Protocol> get_protocol(const std::string& name) const;

  /// Generate a header-like of all the Objective-C metadata identified in the
  /// binary.
  /// The generated output can be configured with the DeclOpt
  std::string to_decl(const DeclOpt& opt = DeclOpt()) const;

  ~Metadata();
  private:
  std::unique_ptr<details::Metadata> impl_;
};

}
}
#endif
