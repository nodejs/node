/* Copyright 2017 - 2026 R. Thomas
 * Copyright 2017 - 2026 Quarkslab
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
#include "LIEF/runtime/android/Module.hpp"
#include "LIEF/ELF/ParserConfig.hpp"
#include "LIEF/ELF/Parser.hpp"
#include "LIEF/ELF/Binary.hpp"

#include <elf.h>
#include <unistd.h>
#include <climits>
#include <link.h>
#include <cstring>
#include <dlfcn.h>

#include "logging.hpp"

namespace LIEF::runtime {

namespace details {
// The concrete implementation of a Android module
class Module {
  public:
  virtual uint64_t imagebase() const = 0;
  virtual std::string name() const = 0;
  virtual std::string path() const = 0;
  virtual std::unique_ptr<Module> clone() const = 0;

  virtual size_t size() const = 0;

  virtual void* handle() const {
    return nullptr;
  }

  virtual void* dlsym(const std::string& /*name*/) const {
    return nullptr;
  }

  virtual ~Module() = default;
};

class LinkerModule : public Module {
  public:
  LinkerModule() = default;
  LinkerModule(const dl_phdr_info& info, size_t size) :
    dlpi_name_(info.dlpi_name == nullptr ? "" : info.dlpi_name) {
    std::memset(&phdr_info_, 0, sizeof(dl_phdr_info));
    if (size <= sizeof(dl_phdr_info)) {
      std::memcpy(&phdr_info_, &info, size);
    }

    if (dlpi_name_.empty() && imagebase() > 0) {
      std::array<char, PATH_MAX> exe_path = {0};
      ssize_t nb_bytes =
          readlink("/proc/self/exe", exe_path.data(), exe_path.size());
      if (nb_bytes >= 0) {
        dlpi_name_ = std::string(exe_path.data(), exe_path.data() + nb_bytes);
      } else {
        LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
      }
    }
  }

  LinkerModule(const LinkerModule&) = default;
  LinkerModule& operator=(const LinkerModule&) = default;

  LinkerModule(LinkerModule&&) noexcept = default;
  LinkerModule& operator=(LinkerModule&&) noexcept = default;

  std::unique_ptr<Module> clone() const override {
    return std::make_unique<LinkerModule>(*this);
  }

  std::string path() const override {
    if (dlpi_name_.empty()) {
      return "";
    }
    size_t pos = dlpi_name_.find('/');
    if (pos == std::string::npos) {
      return "";
    }
    return dlpi_name_;
  }

  std::string name() const override {
    if (dlpi_name_.empty()) {
      return "";
    }
    size_t pos = dlpi_name_.rfind('/');
    if (pos == std::string::npos) {
      return dlpi_name_;
    }
    return dlpi_name_.substr(pos + 1);
  }

  uint64_t imagebase() const override {
    return phdr_info_.dlpi_addr;
  }

  size_t size() const override {
    if (size_ > 0) {
      return size_;
    }
    uint64_t va_start = -1llu;
    uint64_t va_end = 0;
    for (size_t i = 0; i < phdr_info_.dlpi_phnum; ++i) {
      const auto& phdr = phdr_info_.dlpi_phdr[i];
      if (phdr.p_type != PT_LOAD) {
        continue;
      }
      va_end = std::max<uint64_t>(phdr.p_vaddr + phdr.p_memsz, va_end);
      va_start = std::min<uint64_t>(phdr.p_vaddr, va_start);
    }
    if (va_end > va_start) {
      size_ = va_end - va_start;
    }
    return size_;
  }

  void* handle() const override {
    if (handle_ != nullptr) {
      return handle_;
    }

    // To avoid the error "error: linker cannot load itself"
    if (this->name() == "linker64") {
      return nullptr;
    }

    handle_ = ::dlopen(dlpi_name_.c_str(), RTLD_NOW);
    return handle_;
  }

  void* dlsym(const std::string& name) const override {
    void* H = handle();
    if (H == nullptr) {
      return nullptr;
    }
    return ::dlsym(H, name.c_str());
  }

  ~LinkerModule() override = default;

  private:
  dl_phdr_info phdr_info_{};
  std::string dlpi_name_;
  mutable size_t size_ = 0;
  mutable void* handle_ = nullptr;
};

class ModuleIt {
  public:
  virtual bool eq(const ModuleIt& other) const = 0;
  virtual std::unique_ptr<ModuleIt> clone() const = 0;
  virtual std::unique_ptr<runtime::Module> get() const = 0;
  virtual void increment() = 0;

  virtual ~ModuleIt() = default;
};

using linker_modules_t = std::vector<std::unique_ptr<LinkerModule>>;

class LinkerModuleIt : public ModuleIt {
  public:
  LinkerModuleIt() = delete;
  LinkerModuleIt(const LinkerModuleIt&) = default;
  LinkerModuleIt& operator=(const LinkerModuleIt&) = default;

  LinkerModuleIt(LinkerModuleIt&&) = default;
  LinkerModuleIt& operator=(LinkerModuleIt&&) = default;

  LinkerModuleIt(linker_modules_t& modules, size_t pos) :
    pos_(pos),
    modules_(&modules) {}

  LinkerModuleIt(linker_modules_t& modules) :
    LinkerModuleIt(modules, 0) {}

  bool eq(const ModuleIt& other) const override {
    return pos_ == static_cast<const LinkerModuleIt&>(other).pos_;
  }

  std::unique_ptr<ModuleIt> clone() const override {
    return std::make_unique<LinkerModuleIt>(*this);
  }

  std::unique_ptr<runtime::Module> get() const override {
    assert(pos_ < modules_->size());
    return std::make_unique<android::Module>(modules_->at(pos_)->clone());
  }

  void increment() override {
    ++pos_;
  }

  ~LinkerModuleIt() override = default;

  private:
  size_t pos_ = 0;
  linker_modules_t* modules_ = nullptr;
};


linker_modules_t& linker_modules() {
  static thread_local linker_modules_t modules;
  modules.clear();

  dl_iterate_phdr(
      [](struct dl_phdr_info* info, size_t size, void* data) {
        auto& modules = *reinterpret_cast<linker_modules_t*>(data);
        modules.push_back(std::make_unique<LinkerModule>(*info, size));
        return 0;
      },
      &modules
  );

  return modules;
}

}
Module::Iterator::Iterator() = default;

Module::Iterator::Iterator(const Iterator& other) :
  impl_(other.impl_->clone()) {}

Module::Iterator& Module::Iterator::operator=(const Iterator& other) {
  if (this != &other) {
    impl_ = other.impl_->clone();
    cached_.reset();
  }
  return *this;
}

Module::Iterator::Iterator(Iterator&&) noexcept = default;
Module::Iterator& Module::Iterator::operator=(Iterator&&) noexcept = default;

Module::Iterator::Iterator(std::unique_ptr<details::ModuleIt> impl) :
  impl_(std::move(impl)) {}

Module::Iterator::~Iterator() = default;

bool operator==(const Module::Iterator& LHS, const Module::Iterator& RHS) {
  return LHS.impl_->eq(*RHS.impl_);
}

Module::Iterator& Module::Iterator::operator++() {
  impl_->increment();
  cached_.reset();
  return *this;
}

void Module::Iterator::load() const {
  if (cached_ == nullptr) {
    cached_ = impl_->get();
  }
}

const Module& Module::Iterator::operator*() const {
  load();
  return *cached_;
}

const Module* Module::Iterator::operator->() const {
  load();
  return cached_.get();
}

std::unique_ptr<Module> Module::Iterator::yield() {
  load();
  return std::move(cached_);
}

Module::Module(std::unique_ptr<details::Module> impl) :
  impl_(std::move(impl)) {}

Module::Module(Module&&) noexcept = default;
Module& Module::operator=(Module&&) noexcept = default;

Module::~Module() = default;

std::unique_ptr<Module> Module::clone() const {
  return std::make_unique<android::Module>(impl_->clone());
}

uint64_t Module::imagebase() const {
  return impl_->imagebase();
}

std::string Module::name() const {
  return impl_->name();
}

std::string Module::path() const {
  return impl_->path();
}

size_t Module::size() const {
  return impl_->size();
}

std::string Module::to_string() const {
  return fmt::format("0x{:08x}: {}", imagebase(), name());
}

modules_t modules() {
  using namespace details;
  linker_modules_t& linker_mods = details::linker_modules();
  auto begin =
      runtime::Module::Iterator(std::make_unique<LinkerModuleIt>(linker_mods));

  auto end = runtime::Module::Iterator(
      std::make_unique<LinkerModuleIt>(linker_mods, linker_mods.size())
  );

  return make_range(std::move(begin), std::move(end));
}

namespace android {

std::unique_ptr<Module> dlopen(const std::string& name) {
  void* hdl = ::dlopen(name.empty() ? nullptr : name.c_str(), RTLD_NOW);
  if (hdl == nullptr) {
    return nullptr;
  }
  return Module::from_handle(hdl);
}

std::unique_ptr<Module> Module::from_handle(void* H) {
  auto range = modules();
  for (auto it = range.begin(); it != range.end(); ++it) {
    const auto* android_module = it->as<Module>();
    assert(android_module != nullptr);
    void* h = android_module->handle();
    if (h == H) {
      std::unique_ptr<runtime::Module> M = it.yield();
      return std::unique_ptr<Module>(M.release()->as<Module>());
    }
    if (h != nullptr) {
      ::dlclose(h);
    }
  }
  return nullptr;
}

void* Module::handle() const {
  return impl_->handle();
}

void* Module::dlsym(const std::string& name) const {
  return impl_->dlsym(name);
}

std::unique_ptr<ELF::Binary> Module::parse_from_path() const {
  return parse_from_path(ELF::ParserConfig::all());
}

std::unique_ptr<ELF::Binary>
    Module::parse_from_path(const ELF::ParserConfig& config) const {
  const std::string& path = this->path();
  if (path.empty()) {
    LIEF_ERR("Can't parse '{}': Path is empty", name());
    return nullptr;
  }
  return ELF::Parser::parse(path, config);
}

std::unique_ptr<ELF::Binary> Module::parse_from_memory() const {
  return Module::parse_from_memory(ELF::ParserConfig::all());
}

std::unique_ptr<ELF::Binary>
    Module::parse_from_memory(const ELF::ParserConfig& config) const {
  if (imagebase() == 0) {
    LIEF_ERR("Can't parse {}: imagebase is null", path());
    return nullptr;
  }

  LIEF_DEBUG("Trying to parse {} from memory: [{:#010x}, {:#010x}]", path(),
             imagebase(), imagebase() + size());
  return LIEF::ELF::Parser::parse_from_memory(imagebase(), size(), config);
}
}


}
