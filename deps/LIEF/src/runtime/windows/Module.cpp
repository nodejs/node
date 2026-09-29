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

#include "LIEF/PE/ParserConfig.hpp"
#include "LIEF/PE/Parser.hpp"
#include "LIEF/PE/Binary.hpp"
#include "LIEF/runtime/windows/Module.hpp"

#include "logging.hpp"

#include <Windows.h>
#include <psapi.h>
#include <errhandlingapi.h>
#include <libloaderapi.h>

#include "LIEF/utils.hpp"

namespace LIEF::runtime {

namespace details {
// The concrete implementation of a Linux module
class Module {
  public:
  virtual uint64_t imagebase() const = 0;
  virtual std::string name() const = 0;
  virtual std::string path() const = 0;
  virtual size_t size() const = 0;

  virtual std::unique_ptr<Module> clone() const = 0;

  virtual void* handle() const {
    return nullptr;
  }

  virtual ~Module() = default;
};


class ModuleIt {
  public:
  virtual bool eq(const ModuleIt& other) const = 0;
  virtual std::unique_ptr<ModuleIt> clone() const = 0;
  virtual std::unique_ptr<runtime::Module> get() const = 0;
  virtual void increment() = 0;

  virtual ~ModuleIt() = default;
};

class DefaultModule : public Module {
  public:
  static std::unique_ptr<DefaultModule> create(HMODULE module,
                                               HANDLE current_proc) {
    std::array<wchar_t, MAX_PATH> tmp_buffer{};

    std::string path;
    std::string name;
    MODULEINFO module_info;

    std::memset(&module_info, 0, sizeof(MODULEINFO));

    if (GetModuleFileNameW(module, tmp_buffer.data(), tmp_buffer.size()) != 0) {
      std::u16string u16 = reinterpret_cast<const char16_t*>(tmp_buffer.data());
      path = u16tou8(u16);
    } else {
      LIEF_DEBUG("Error: {}:{} ({})", __FUNCTION__, __LINE__, (int)GetLastError());
    }

    tmp_buffer.fill(0);

    if (GetModuleBaseNameW(current_proc, module, tmp_buffer.data(),
                           tmp_buffer.size()) != 0)
    {
      std::u16string u16 = reinterpret_cast<const char16_t*>(tmp_buffer.data());
      name = u16tou8(u16);
    } else {
      LIEF_DEBUG("Error: {}:{} ({})", __FUNCTION__, __LINE__, (int)GetLastError());
    }

    if (GetModuleInformation(current_proc, module, &module_info,
                             sizeof(MODULEINFO)) == 0)

    {
      LIEF_DEBUG("Error: {}:{} ({})", __FUNCTION__, __LINE__, (int)GetLastError());
    }

    return std::make_unique<DefaultModule>(module, std::move(name),
                                           std::move(path), module_info);
  }

  DefaultModule() = default;
  DefaultModule(HMODULE M, std::string name, std::string path,
                const MODULEINFO& info) :
    impl_(M),
    info_(info),
    name_(std::move(name)),
    path_(std::move(path)) {}

  DefaultModule(const DefaultModule&) = default;
  DefaultModule& operator=(const DefaultModule&) = default;

  DefaultModule(DefaultModule&&) noexcept = default;
  DefaultModule& operator=(DefaultModule&&) noexcept = default;

  std::unique_ptr<Module> clone() const override {
    return std::make_unique<DefaultModule>(*this);
  }

  std::string name() const override {
    return name_;
  }

  std::string path() const override {
    return path_;
  }

  uint64_t imagebase() const override {
    return reinterpret_cast<uint64_t>(info_.lpBaseOfDll);
  }

  size_t size() const override {
    return info_.SizeOfImage;
  }

  void* handle() const override {
    return impl_;
  }

  private:
  HMODULE impl_;
  MODULEINFO info_;
  std::string name_;
  std::string path_;
};

using default_modules_t = std::vector<std::unique_ptr<DefaultModule>>;

class DefaultModuleIt : public ModuleIt {
  public:
  DefaultModuleIt() = delete;
  DefaultModuleIt(const DefaultModuleIt&) = default;
  DefaultModuleIt& operator=(const DefaultModuleIt&) = default;

  DefaultModuleIt(DefaultModuleIt&&) = default;
  DefaultModuleIt& operator=(DefaultModuleIt&&) = default;

  DefaultModuleIt(default_modules_t& modules, size_t pos) :
    pos_(pos),
    modules_(&modules) {}

  DefaultModuleIt(default_modules_t& modules) :
    DefaultModuleIt(modules, 0) {}

  bool eq(const ModuleIt& other) const override {
    return pos_ == static_cast<const DefaultModuleIt&>(other).pos_;
  }

  std::unique_ptr<ModuleIt> clone() const override {
    return std::make_unique<DefaultModuleIt>(*this);
  }

  std::unique_ptr<runtime::Module> get() const override {
    assert(pos_ < modules_->size());
    return std::make_unique<windows::Module>((*modules_)[pos_]->clone());
  }

  void increment() override {
    ++pos_;
  }

  ~DefaultModuleIt() override = default;

  private:
  size_t pos_ = 0;
  default_modules_t* modules_ = nullptr;
};

default_modules_t& default_modules() {
  static default_modules_t modules;
  modules.clear();

  HANDLE hProcess = GetCurrentProcess();
  HMODULE lphModule = nullptr;
  DWORD size = 0;

  if (!EnumProcessModules(hProcess, &lphModule, sizeof(HMODULE), &size)) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return modules;
  }

  assert(size % sizeof(HMODULE) == 0);

  std::vector<HMODULE> hModules;
  hModules.resize(size / sizeof(HMODULE));

  if (!EnumProcessModules(hProcess, hModules.data(), size, &size)) {
    LIEF_DEBUG("Error: {}:{}", __FUNCTION__, __LINE__);
    return modules;
  }

  modules.reserve(hModules.size());

  for (size_t i = 0; i < hModules.size(); ++i) {
    if (auto mod = DefaultModule::create(hModules[i], hProcess)) {
      modules.push_back(std::move(mod));
    }
  }

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
  return std::make_unique<windows::Module>(impl_->clone());
}

uint64_t Module::imagebase() const {
  return impl_->imagebase();
}

uint64_t Module::size() const {
  return impl_->size();
}

std::string Module::name() const {
  return impl_->name();
}

std::string Module::path() const {
  return impl_->path();
}

namespace windows {

std::unique_ptr<Module> Module::from_handle(void* H) {
  HANDLE hProcess = GetCurrentProcess();
  auto impl =
      details::DefaultModule::create(reinterpret_cast<HMODULE>(H), hProcess);
  return std::make_unique<windows::Module>(std::move(impl));
}

std::unique_ptr<PE::Binary> Module::parse_from_path() const {
  return parse_from_path(PE::ParserConfig::default_conf());
}

std::unique_ptr<PE::Binary>
    Module::parse_from_path(const PE::ParserConfig& config) const {
  const std::string& path = this->path();
  if (path.empty()) {
    LIEF_ERR("Can't parse '{}': Path is empty", name());
    return nullptr;
  }
  return PE::Parser::parse(path, config);
}

std::unique_ptr<PE::Binary> Module::parse_from_memory() const {
  return Module::parse_from_memory(PE::ParserConfig::default_conf());
}

std::unique_ptr<PE::Binary>
    Module::parse_from_memory(const PE::ParserConfig& config) const {
  if (imagebase() == 0) {
    LIEF_ERR("Can't parse {}: imagebase is null", path());
    return nullptr;
  }

  LIEF_DEBUG("Trying to parse {} from memory: [{:#010x}, {:#010x}]", path(),
             imagebase(), imagebase() + size());

  return LIEF::PE::Parser::parse_from_memory(imagebase(), config);
}

void* Module::handle() const {
  return impl_->handle();
}

std::unique_ptr<Module> dlopen(const std::string& name) {
  // HMODULE LoadLibraryExW(
  //   [in] LPCWSTR lpLibFileName,
  //        HANDLE  hFile,
  //   [in] DWORD   dwFlags
  // );
  std::u16string u16name = u8tou16(name).value_or(u"");
  if (u16name.empty()) {
    return nullptr;
  }

  HMODULE M = LoadLibraryExW(reinterpret_cast<const wchar_t*>(u16name.c_str()),
                             /*hFile=*/nullptr,
                             /*dwFlags=*/0);
  if (M == nullptr) {
    return nullptr;
  }
  return Module::from_handle(M);
}


std::unique_ptr<Module> find_module(const std::string& name) {
  std::u16string u16name = u8tou16(name).value_or(u"");
  if (u16name.empty()) {
    return nullptr;
  }
  HMODULE M = GetModuleHandleW(reinterpret_cast<const wchar_t*>(u16name.c_str()));
  if (M == nullptr) {
    return nullptr;
  }
  return Module::from_handle(M);
}

void* Module::dlsym(const std::string& name) const {
  return reinterpret_cast<void*>(
      GetProcAddress(reinterpret_cast<HMODULE>(handle()), name.c_str())
  );
}

}

std::string Module::to_string() const {
  return fmt::format("0x{:08x}: {}", imagebase(), name());
}

modules_t modules() {
  using namespace details;
  default_modules_t& default_mods = details::default_modules();

  auto begin =
      runtime::Module::Iterator(std::make_unique<DefaultModuleIt>(default_mods));

  auto end = runtime::Module::Iterator(
      std::make_unique<DefaultModuleIt>(default_mods, default_mods.size())
  );

  return make_range(std::move(begin), std::move(end));
}


}
