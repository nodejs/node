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
#include "LIEF/runtime/osx/Module.hpp"
#include "LIEF/runtime/osx/Host.hpp"
#include "LIEF/range.hpp"
#include "LIEF/MachO/utils.hpp"
#include "LIEF/MachO/Parser.hpp"
#include "LIEF/MachO/ParserConfig.hpp"
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/FatBinary.hpp"
#include "LIEF/BinaryStream/MemoryStream.hpp"

#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/getsect.h>
#include <sys/sysctl.h>
#include <dlfcn.h>

#include <libkern/OSCacheControl.h>

#include "logging.hpp"

extern "C" kern_return_t mach_vm_read_overwrite(vm_map_read_t target_task,
                                                mach_vm_address_t address,
                                                mach_vm_size_t size,
                                                mach_vm_address_t data,
                                                mach_vm_size_t* outsize);

namespace LIEF::runtime {

namespace details {


static const char* dyld_image_path_containing_address(const void* addr) {
  using namespace LIEF::runtime::osx;
  using fn_t = const char* (*)(const void*);
  static fn_t fn = [] {
    auto F = reinterpret_cast<fn_t>(dlsym(RTLD_DEFAULT,
                                          "dyld_image_path_containing_address"));
    if (F == nullptr) {
      LIEF_ERR("dlsym err: {} ({})", dlerror(), osx::Host::os_version_name());
    }
    return F;
  }();

  if (fn == nullptr) {
    return nullptr;
  }

  return fn(addr);
}


static const struct mach_header* dyld_get_dlopen_image_header(void* handle) {
  using namespace LIEF::runtime::osx;
  using fn_t = const struct mach_header* (*)(void*);
  if (Host::os_version() < Host::version_t::Ventura()) {
    // According to dyld/DyldAPIs.h _dyld_get_dlopen_image_header is only
    // available from iOS 16 / macOS 13
    return nullptr;
  }

  static fn_t fn = [] {
    auto F = reinterpret_cast<fn_t>(dlsym(RTLD_DEFAULT,
                                          "_dyld_get_dlopen_image_header"));
    if (F == nullptr) {
      LIEF_ERR("dlsym err: {} ({})", dlerror(), osx::Host::os_version_name());
    }
    return F;
  }();

  if (fn == nullptr) {
    return nullptr;
  }
  return fn(handle);
}

struct module_info_t {
  uintptr_t start = 0;
  uintptr_t end = 0;
  std::string path;
};

struct DyldInfoLegacy {
  uint32_t all_image_info_addr;
};

struct DyldInfo32 {
  uint32_t all_image_info_addr;
  uint32_t all_image_info_size;
  int32_t all_image_info_format;
};

struct DyldInfo64 {
  uint64_t all_image_info_addr;
  uint64_t all_image_info_size;
  int32_t all_image_info_format;
};

union DyldInfo {
  DyldInfoLegacy info_legacy;
  DyldInfo32 info_32;
  DyldInfo64 info_64;
};


struct DyldAllImageInfos32 {
  uint32_t version;
  uint32_t info_array_count;
  uint32_t info_array;
  uint32_t notification;
  uint8_t process_detached_from_shared_region;
  uint8_t libsystem_initialized;
  uint32_t dyld_image_load_address;
};

struct DyldAllImageInfos64 {
  uint32_t version;
  uint32_t info_array_count;
  uint64_t info_array;
  uint64_t notification;
  uint8_t process_detached_from_shared_region;
  uint8_t libsystem_initialized;
  uint32_t padding;
  uint64_t dyld_image_load_address;
};

struct DyldImageInfo32 {
  uint32_t image_load_address;
  uint32_t image_file_path;
  uint32_t image_file_mod_date;
};

struct DyldImageInfo64 {
  uint64_t image_load_address;
  uint64_t image_file_path;
  uint64_t image_file_mod_date;
};


using cbk = bool (*)(const module_info_t*, void* data);


uint8_t* darwin_read(mach_port_t task, uintptr_t address, size_t len,
                     size_t* n_bytes_read) {
  size_t page_size = getpagesize();
  uint8_t* result;
  size_t offset;
  [[maybe_unused]] mach_port_t self;
  kern_return_t kr;

  result = (uint8_t*)malloc(len);
  offset = 0;

  self = mach_task_self();

  while (offset != len) {
    uintptr_t chunk_address, page_address;
    size_t chunk_size, page_offset;

    chunk_address = address + offset;
    page_address = chunk_address & ~(uintptr_t)(page_size - 1);
    page_offset = chunk_address - page_address;
    chunk_size = std::min<uintptr_t>(len - offset, page_size - page_offset);

    mach_vm_size_t n_bytes_read;

    /* mach_vm_read corrupts memory on iOS */
    kr = mach_vm_read_overwrite(task, chunk_address, chunk_size,
                                (vm_address_t)(result + offset), &n_bytes_read);
    if (kr != KERN_SUCCESS) {
      break;
    }

    offset += chunk_size;
  }

  if (offset == 0) {
    free(result);
    result = NULL;
  }

  if (n_bytes_read != NULL) {
    *n_bytes_read = offset;
  }

  return result;
}

// This function is taken from frida-gum (gum_process_darwin.c:
// gum_darwin_enumerate_modules)
template<class F>
void enumerate_modules(mach_port_t task, F&& func) {
  static constexpr size_t DYLD_INFO_LEGACY_COUNT = 1;
  static constexpr size_t DYLD_INFO_32_COUNT = 3;

  static constexpr size_t DYLD_INFO_COUNT = 5;
  static constexpr size_t DYLD_INFO_64_COUNT = 5;

  static constexpr size_t DYLD_IMAGE_INFO_32_SIZE = 12;
  static constexpr size_t DYLD_IMAGE_INFO_64_SIZE = 24;

  uint32_t count = 0;
  kern_return_t kr;
  task_dyld_info info;
  [[maybe_unused]] size_t info_array_count, info_array_size;
  uintptr_t info_array_address, dyld_image_load_address;
  [[maybe_unused]] void* header_data_end = NULL;
  const uint32_t header_data_initial_size = 4096;
  bool carry_on = true;
  DyldInfo info_raw;
  count = DYLD_INFO_COUNT;

  kr = task_info(task, TASK_DYLD_INFO, reinterpret_cast<task_info_t>(&info_raw),
                 &count);

  if (kr != KERN_SUCCESS) {
    // QBDI_ERROR("task_info error {}:{}", __PRETTY_FUNCTION__, __LINE__);
    return;
  }
  switch (count) {
    case DYLD_INFO_LEGACY_COUNT:
    {
      info.all_image_info_addr = info_raw.info_legacy.all_image_info_addr;
      info.all_image_info_size = 0;
      info.all_image_info_format = TASK_DYLD_ALL_IMAGE_INFO_32;
      break;
    }

    case DYLD_INFO_32_COUNT:
    {
      info.all_image_info_addr = info_raw.info_32.all_image_info_addr;
      info.all_image_info_size = info_raw.info_32.all_image_info_size;
      info.all_image_info_format = info_raw.info_32.all_image_info_format;
      break;
    }

    case DYLD_INFO_64_COUNT:
    {
      info.all_image_info_addr = info_raw.info_64.all_image_info_addr;
      info.all_image_info_size = info_raw.info_64.all_image_info_size;
      info.all_image_info_format = info_raw.info_64.all_image_info_format;
      break;
    }

    default: LIEF_ERR("task_info error: invalid count ({})", count); return;
  }

  if (info.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_64) {
    DyldAllImageInfos64* all_info;

    all_info = (DyldAllImageInfos64*)info.all_image_info_addr;
    if (all_info == NULL) {
      return;
    }

    info_array_count = all_info->info_array_count;
    info_array_size = info_array_count * DYLD_IMAGE_INFO_64_SIZE;
    info_array_address = all_info->info_array;
    dyld_image_load_address = all_info->dyld_image_load_address;

  } else {
    DyldAllImageInfos32* all_info;

    all_info = (DyldAllImageInfos32*)info.all_image_info_addr;
    if (all_info == NULL) {
      return;
    }

    info_array_count = all_info->info_array_count;
    info_array_size = info_array_count * DYLD_IMAGE_INFO_32_SIZE;
    info_array_address = all_info->info_array;
    dyld_image_load_address = all_info->dyld_image_load_address;
  }

  if (info_array_address == 0) {
    return;
  }

  for (size_t i = 0; i != info_array_count + 1 && carry_on; ++i) {
    uintptr_t load_address = 0;
    const char* module_path = nullptr;
    module_info_t details;

    if (i != info_array_count) {
      uintptr_t file_path_address;

      if (info.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_64) {
        const auto* info = reinterpret_cast<const DyldImageInfo64*>(
            info_array_address + (i * DYLD_IMAGE_INFO_64_SIZE)
        );
        load_address = info->image_load_address;
        file_path_address = info->image_file_path;
      } else {
        const auto* info = reinterpret_cast<const DyldImageInfo32*>(
            info_array_address + (i * DYLD_IMAGE_INFO_32_SIZE)
        );
        load_address = info->image_load_address;
        file_path_address = info->image_file_path;
      }

      module_path = reinterpret_cast<const char*>(file_path_address);

      if (load_address == 0) {
        return;
      }
    } else {
      load_address = dyld_image_load_address;

      if (load_address == 0) {
        return;
      }

      module_path = "/usr/lib/dyld";
    }

    header_data_end = (void*)(load_address + header_data_initial_size);

    uint64_t max_address = load_address;
    LIEF::MemoryStream stream(load_address);
    LIEF::MachO::foreach_segment(
        stream, [&](const std::string& /*name*/, uint64_t /*offset*/,
                    uint64_t /*size*/, uint64_t addr, uint64_t vsize) {
          uintptr_t seg_max = load_address + addr + vsize;
          max_address = std::max<uint64_t>(seg_max, max_address);
        }
    );

    carry_on = func(load_address, max_address, module_path);
  }
  return;
}

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

  virtual void* dlsym(const std::string& /*name*/) const {
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


class ModuleImpl : public Module {
  public:
  ModuleImpl() = default;
  ModuleImpl(uintptr_t start, uintptr_t end, const char* path,
             std::shared_ptr<void> handle) :
    start_(start),
    end_(end),
    path_(path),
    handle_(std::move(handle)) {}

  ModuleImpl(uintptr_t start, uintptr_t end, const char* path) :
    ModuleImpl(start, end, path, /*handle=*/nullptr) {}

  ModuleImpl(const ModuleImpl&) = default;
  ModuleImpl& operator=(const ModuleImpl&) = default;

  ModuleImpl(ModuleImpl&&) noexcept = default;
  ModuleImpl& operator=(ModuleImpl&&) noexcept = default;

  std::string path() const override {
    return path_;
  }

  std::string name() const override {
    if (path_.empty()) {
      return "";
    }
    size_t pos = path_.rfind('/');
    if (pos == std::string::npos) {
      return path_;
    }
    return path_.substr(pos + 1);
  }

  std::unique_ptr<Module> clone() const override {
    return std::make_unique<ModuleImpl>(*this);
  }

  uint64_t imagebase() const override {
    return start_;
  }

  size_t size() const override {
    return end_ - start_;
  }

  void* handle() const override {
    return handle_.get();
  }

  void* dlsym(const std::string& name) const override {
    if (handle_ == nullptr) {
      return nullptr;
    }
    return ::dlsym(handle_.get(), name.c_str());
  }

  ~ModuleImpl() override = default;

  private:
  uintptr_t start_ = 0;
  uintptr_t end_ = 0;
  std::string path_;
  std::shared_ptr<void> handle_;
};

using list_modules_t = std::vector<std::unique_ptr<ModuleImpl>>;

class ModuleImplIt : public ModuleIt {
  public:
  ModuleImplIt() = delete;
  ModuleImplIt(const ModuleImplIt&) = default;
  ModuleImplIt& operator=(const ModuleImplIt&) = default;

  ModuleImplIt(ModuleImplIt&&) = default;
  ModuleImplIt& operator=(ModuleImplIt&&) = default;

  ModuleImplIt(list_modules_t& modules, size_t pos) :
    pos_(pos),
    modules_(&modules) {}

  ModuleImplIt(list_modules_t& modules) :
    ModuleImplIt(modules, 0) {}

  bool eq(const ModuleIt& other) const override {
    return pos_ == static_cast<const ModuleImplIt&>(other).pos_;
  }

  std::unique_ptr<ModuleIt> clone() const override {
    return std::make_unique<ModuleImplIt>(*this);
  }

  std::unique_ptr<runtime::Module> get() const override {
    assert(pos_ < modules_->size());
    return std::make_unique<osx::Module>(modules_->at(pos_)->clone());
  }

  void increment() override {
    ++pos_;
  }

  ~ModuleImplIt() override = default;

  private:
  size_t pos_ = 0;
  list_modules_t* modules_ = nullptr;
};

inline std::shared_ptr<void> borrow_handle(void* handle) {
  return {handle, [](void*) {}};
}

list_modules_t& get_modules() {
  static thread_local list_modules_t modules;
  modules.clear();

  enumerate_modules(
      mach_task_self(),
      [&](uintptr_t start, uintptr_t end, const char* path) -> bool {
        std::shared_ptr<void> handle(::dlopen(path, RTLD_NOLOAD), [](void* h) {
          if (h != nullptr) {
            ::dlclose(h);
          }
        });
        if (handle == nullptr) {
          LIEF_DEBUG("{}", dlerror());
        }
        modules.push_back(std::make_unique<ModuleImpl>(start, end, path,
                                                       std::move(handle)));
        return /*continue*/ true;
      }
  );

  return modules;
}

} // namespace details

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
  return std::make_unique<osx::Module>(impl_->clone());
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

uint64_t Module::size() const {
  return impl_->size();
}

std::string Module::to_string() const {
  return fmt::format("{:#08x}: {} ([{:#010x}, {:#010x}])", imagebase(), name(),
                     imagebase(), imagebase() + size());
}

modules_t modules() {
  using namespace details;
  list_modules_t& mods = details::get_modules();

  auto begin = runtime::Module::Iterator(std::make_unique<ModuleImplIt>(mods));

  auto end =
      runtime::Module::Iterator(std::make_unique<ModuleImplIt>(mods, mods.size()));

  return make_range(std::move(begin), std::move(end));
}

namespace osx {

void* Module::handle() const {
  return impl_->handle();
}

void* Module::dlsym(const std::string& name) const {
  return impl_->dlsym(name);
}

std::unique_ptr<MachO::Binary> Module::parse_from_path() const {
  return parse_from_path(MachO::ParserConfig::deep());
}

std::unique_ptr<MachO::Binary>
    Module::parse_from_path(const MachO::ParserConfig& config) const {
  const std::string& p = path();
  if (p.empty()) {
    LIEF_ERR("Can't parse '{}': path is empty", name());
    return nullptr;
  }

  std::unique_ptr<MachO::FatBinary> fat = MachO::Parser::parse(p, config);
  if (fat == nullptr || fat->size() == 0) {
    return nullptr;
  }
  return fat->take(0);
}

std::unique_ptr<MachO::Binary> Module::parse_from_memory() const {
  return parse_from_memory(MachO::ParserConfig::deep());
}

std::unique_ptr<MachO::Binary>
    Module::parse_from_memory(const MachO::ParserConfig& config) const {
  if (imagebase() == 0) {
    LIEF_ERR("Can't parse {}: imagebase is null", path());
    return nullptr;
  }

  LIEF_DEBUG("Trying to parse {} from memory: [{:#010x}, {:#010x}]", path(),
             imagebase(), imagebase() + size());

  std::unique_ptr<MachO::FatBinary> fat =
      MachO::Parser::parse_from_memory(imagebase(), size(), config);
  if (fat == nullptr || fat->size() == 0) {
    return nullptr;
  }
  return fat->take(0);
}

std::unique_ptr<Module> Module::from_handle(void* handle) {
  if (handle == nullptr) {
    return nullptr;
  }

  const struct mach_header* header = details::dyld_get_dlopen_image_header(handle);
  if (header == nullptr) {
    auto impl =
        std::make_unique<details::ModuleImpl>(0, 0, "",
                                              details::borrow_handle(handle));
    return std::make_unique<Module>(std::move(impl));
  }

  range_t va_ranges{reinterpret_cast<uintptr_t>(header), 0};
  {
    LIEF::MemoryStream stream(va_ranges.low);
    LIEF::MachO::foreach_segment(
        stream, [&](const std::string& /*name*/, uint64_t /*offset*/,
                    uint64_t /*size*/, uint64_t addr, uint64_t vsize) {
          uintptr_t seg_max = va_ranges.low + addr + vsize;
          va_ranges.high = std::max<uint64_t>(seg_max, va_ranges.high);
        }
    );
  }

  const char* path = details::dyld_image_path_containing_address(
      reinterpret_cast<const void*>(va_ranges.low)
  );

  auto impl =
      std::make_unique<details::ModuleImpl>(va_ranges.low, va_ranges.high,
                                            path != nullptr ? path : "",
                                            details::borrow_handle(handle));
  return std::make_unique<Module>(std::move(impl));
}

std::unique_ptr<Module> dlopen(const std::string& name) {
  void* handle = ::dlopen(name.c_str(), RTLD_NOW);
  if (handle == nullptr) {
    LIEF_ERR("Can't load {}: {}", name, dlerror());
    return nullptr;
  }
  return Module::from_handle(handle);
}
}


}
