/* Copyright 2017 - 2025 R. Thomas
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
#ifndef LIEF_MACHO_SUB_CLIENT_H
#define LIEF_MACHO_SUB_CLIENT_H
#include <string>
#include <ostream>

#include "LIEF/visibility.h"

#include "LIEF/MachO/LoadCommand.hpp"

namespace LIEF {
namespace MachO {

class BinaryParser;

namespace details {
struct sub_client_command;
}

/// Class that represents the SubClient command.
/// Accodring to the Mach-O `loader.h` documentation:
///
/// > For dynamically linked shared libraries that are subframework of an umbrella
/// > framework they can allow clients other than the umbrella framework or other
/// > subframeworks in the same umbrella framework.  To do this the subframework
/// > is built with "-allowable_client client_name" and an LC_SUB_CLIENT load
/// > command is created for each -allowable_client flag.  The client_name is
/// > usually a framework name.  It can also be a name used for bundles clients
/// > where the bundle is built with "-client_name client_name".
class LIEF_API SubClient : public LoadCommand {
  friend class BinaryParser;
  public:
  SubClient() = default;
  SubClient(const details::sub_client_command& cmd);

  SubClient& operator=(const SubClient& copy) = default;
  SubClient(const SubClient& copy) = default;

  std::unique_ptr<LoadCommand> clone() const override {
    return std::unique_ptr<SubClient>(new SubClient(*this));
  }

  /// Name of the client
  const std::string& client() const {
    return client_;
  }

  void client(std::string u) {
    client_ = std::move(u);
  }

  ~SubClient() override = default;

  void accept(Visitor& visitor) const override;

  std::ostream& print(std::ostream& os) const override;

  static bool classof(const LoadCommand* cmd) {
    return cmd->command() == LoadCommand::TYPE::SUB_CLIENT;
  }

  private:
  std::string client_;
};

}
}
#endif
