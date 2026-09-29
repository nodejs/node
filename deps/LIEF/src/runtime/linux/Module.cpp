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
#include "LIEF/BinaryStream/MemoryStream.hpp"
#include "LIEF/BinaryStream/FileStream.hpp"
#include "LIEF/runtime/linux/Module.hpp"
#include "LIEF/ELF/utils.hpp"
#include "LIEF/ELF/ParserConfig.hpp"
#include "LIEF/ELF/Parser.hpp"
#include "LIEF/ELF/Binary.hpp"

#include "ELF/elf_utils.hpp"

#include <elf.h>
#include <unistd.h>
#include <climits>
#include <link.h>
#include <cstring>
#include <dlfcn.h>

#include "logging.hpp"

namespace LIEF::runtime {

namespace details {
struct link_map {
  uintptr_t l_addr = 0;
  const char* l_name = nullptr;
  uintptr_t l_ld = 0;
  link_map* l_next = nullptr;
  link_map* l_prev = nullptr;
};


// The concrete implementation of a Linux module
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
    phdr_info_{},
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
    if (dlpi_name_.find('/') == std::string::npos) {
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
    return std::make_unique<Linux::Module>(modules_->at(pos_)->clone());
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

namespace Linux {

struct elf_file_info_t {
  uintptr_t imagebase = -1llu;
  uint64_t end_address = 0;
  uintptr_t phdr_off = 0;
  uintptr_t phnum = 0;
  uintptr_t vsize() const {
    assert(end_address >= imagebase);
    return end_address - imagebase;
  }

  std::string to_string() const {
    return fmt::format("imagebase={:#010x}, end_address={:#010x}, "
                       "phdr_off={:#06x} virtual_size={:#06x}",
                       imagebase, end_address, phdr_off, vsize());
  }
};

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


std::unique_ptr<Module> dlopen(const std::string& name) {
  void* hdl = ::dlopen(name.empty() ? nullptr : name.c_str(), RTLD_NOW);
  if (hdl == nullptr) {
    return nullptr;
  }
  return Module::from_handle(hdl);
}

std::unique_ptr<Module> Module::from_handle(void* H) {
  // The handle `H` is an opaque pointer to the internal structure link_map
  // that is defined in include/link.h.
  //
  // According to the comments of this structure, the first attributes are
  // stable:
  // > glibc 2.43
  // > These data structures exist in space used by the run-time dynamic linker;
  // > modifying them may have disastrous results.
  if (H == nullptr) {
    return nullptr;
  }

  const auto* lmap = reinterpret_cast<const details::link_map*>(H);
  dl_phdr_info phdr_info{};
  std::memset(&phdr_info, 0, sizeof(dl_phdr_info));
  phdr_info.dlpi_name = lmap->l_name;

  // l_addr is NOT the based address of the underlying object but the
  // *Difference between the address in the ELF file and the addresses in memory.*
  phdr_info.dlpi_addr = lmap->l_addr;
  LIEF_DEBUG("link_map.l_addr: {:#08x}", lmap->l_addr);
  LIEF_DEBUG("link_map.l_name: {}", lmap->l_name);

  std::string filepath = "";
  if (lmap->l_name != nullptr) {
    filepath = lmap->l_name;
  }

  if (filepath.empty()) {
    LIEF_ERR("Module has an empty file path: can't resolve it from a file");
    return nullptr;
  }

  auto stream = LIEF::FileStream::from_file(filepath);
  if (!stream) {
    LIEF_ERR("Can't open '{}'", filepath);
    return nullptr;
  }

  if (!LIEF::ELF::is_elf(*stream)) {
    LIEF_ERR("{} is not a valid ELF", filepath);
    return nullptr;
  }

  auto info = ELF::get_info(*stream);
  if (!info) {
    return nullptr;
  }

  LIEF_DEBUG("{}: {} (l_addr={:#010x})", filepath, info->to_string(),
             lmap->l_addr);
  uintptr_t abs_addr = lmap->l_addr + info->imagebase;
  LIEF::MemoryStream memstream(abs_addr, 0x100);
  if (!LIEF::ELF::is_elf(memstream)) {
    LIEF_ERR("{:#010x} is not a valid ELF", abs_addr);
    return nullptr;
  }

  phdr_info.dlpi_addr = abs_addr;
  phdr_info.dlpi_phnum = info->phnum;
  phdr_info.dlpi_name = lmap->l_name;
  uintptr_t dlpi_phdr = info->phdr_off;
  if (info->phdr_off <= abs_addr) {
    dlpi_phdr += abs_addr;
  }
  phdr_info.dlpi_phdr = reinterpret_cast<const ElfW(Phdr)*>(dlpi_phdr);

  return std::make_unique<Module>(
      std::make_unique<details::LinkerModule>(phdr_info, sizeof(phdr_info))
  );
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
  return std::make_unique<Linux::Module>(impl_->clone());
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
  return fmt::format("{:#08x}: {}", imagebase(), name());
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

}
