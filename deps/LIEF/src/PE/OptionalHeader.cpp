/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
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
#include <algorithm>

#include "frozen.hpp"

#include "LIEF/PE/hash.hpp"
#include "LIEF/PE/OptionalHeader.hpp"
#include "LIEF/PE/EnumToString.hpp"
#include "PE/Structures.hpp"

#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ranges.h>

namespace LIEF {
namespace PE {

static constexpr std::array DLL_CHARACTERISTICS_LIST = {
  OptionalHeader::DLL_CHARACTERISTICS::HIGH_ENTROPY_VA,
  OptionalHeader::DLL_CHARACTERISTICS::DYNAMIC_BASE,
  OptionalHeader::DLL_CHARACTERISTICS::FORCE_INTEGRITY,
  OptionalHeader::DLL_CHARACTERISTICS::NX_COMPAT,
  OptionalHeader::DLL_CHARACTERISTICS::NO_ISOLATION,
  OptionalHeader::DLL_CHARACTERISTICS::NO_SEH,
  OptionalHeader::DLL_CHARACTERISTICS::NO_BIND,
  OptionalHeader::DLL_CHARACTERISTICS::APPCONTAINER,
  OptionalHeader::DLL_CHARACTERISTICS::WDM_DRIVER,
  OptionalHeader::DLL_CHARACTERISTICS::GUARD_CF,
  OptionalHeader::DLL_CHARACTERISTICS::TERMINAL_SERVER_AWARE,
};

OptionalHeader::OptionalHeader(const details::pe32_optional_header& header):
  magic_(static_cast<PE_TYPE>(header.Magic)),
  major_linker_version_(header.MajorLinkerVersion),
  minor_linker_version_(header.MinorLinkerVersion),
  sizeof_code_(header.SizeOfCode),
  sizeof_initialized_data_(header.SizeOfInitializedData),
  sizeof_uninitialized_data_(header.SizeOfUninitializedData),
  entrypoint_(header.AddressOfEntryPoint),
  baseof_code_(header.BaseOfCode),
  baseof_data_(header.BaseOfData),
  imagebase_(header.ImageBase),
  section_align_(header.SectionAlignment),
  file_align_(header.FileAlignment),
  major_os_version_(header.MajorOperatingSystemVersion),
  minor_os_version_(header.MinorOperatingSystemVersion),
  major_image_version_(header.MajorImageVersion),
  minor_image_version_(header.MinorImageVersion),
  major_subsys_version_(header.MajorSubsystemVersion),
  minor_subsys_version_(header.MinorSubsystemVersion),
  win32_version_value_(header.Win32VersionValue),
  sizeof_image_(header.SizeOfImage),
  sizeof_headers_(header.SizeOfHeaders),
  checksum_(header.CheckSum),
  subsystem_(static_cast<SUBSYSTEM>(header.Subsystem)),
  dll_characteristics_(header.DLLCharacteristics),
  sizeof_stack_reserve_(header.SizeOfStackReserve),
  sizeof_stack_commit_(header.SizeOfStackCommit),
  sizeof_heap_reserve_(header.SizeOfHeapReserve),
  sizeof_heap_commit_(header.SizeOfHeapCommit),
  loader_flags_(header.LoaderFlags),
  nb_rva_size_(header.NumberOfRvaAndSize)
{}

OptionalHeader::OptionalHeader(const details::pe64_optional_header& header):
  magic_(static_cast<PE_TYPE>(header.Magic)),
  major_linker_version_(header.MajorLinkerVersion),
  minor_linker_version_(header.MinorLinkerVersion),
  sizeof_code_(header.SizeOfCode),
  sizeof_initialized_data_(header.SizeOfInitializedData),
  sizeof_uninitialized_data_(header.SizeOfUninitializedData),
  entrypoint_(header.AddressOfEntryPoint),
  baseof_code_(header.BaseOfCode),
  baseof_data_(0),
  imagebase_(header.ImageBase),
  section_align_(header.SectionAlignment),
  file_align_(header.FileAlignment),
  major_os_version_(header.MajorOperatingSystemVersion),
  minor_os_version_(header.MinorOperatingSystemVersion),
  major_image_version_(header.MajorImageVersion),
  minor_image_version_(header.MinorImageVersion),
  major_subsys_version_(header.MajorSubsystemVersion),
  minor_subsys_version_(header.MinorSubsystemVersion),
  win32_version_value_(header.Win32VersionValue),
  sizeof_image_(header.SizeOfImage),
  sizeof_headers_(header.SizeOfHeaders),
  checksum_(header.CheckSum),
  subsystem_(static_cast<SUBSYSTEM>(header.Subsystem)),
  dll_characteristics_(header.DLLCharacteristics),
  sizeof_stack_reserve_(header.SizeOfStackReserve),
  sizeof_stack_commit_(header.SizeOfStackCommit),
  sizeof_heap_reserve_(header.SizeOfHeapReserve),
  sizeof_heap_commit_(header.SizeOfHeapCommit),
  loader_flags_(header.LoaderFlags),
  nb_rva_size_(header.NumberOfRvaAndSize)
{}


OptionalHeader OptionalHeader::create(PE_TYPE type) {
  OptionalHeader header;
  header.magic(type);
  header.section_alignment(0x1000);
  header.file_alignment(0x200);
  header.numberof_rva_and_size(16);
  header.subsystem(SUBSYSTEM::WINDOWS_CUI);
  header.major_linker_version(9);
  header.major_operating_system_version(6);
  header.major_subsystem_version(6);
  header.sizeof_stack_reserve(0x100000);
  header.sizeof_stack_commit(0x1000);
  header.sizeof_heap_reserve(0x100000);
  header.sizeof_heap_commit(0x1000);
  header.sizeof_headers(0x400);

  header.imagebase(type == PE_TYPE::PE32 ? 0x400000 : 0x140000000);
  return header;
}

std::vector<OptionalHeader::DLL_CHARACTERISTICS> OptionalHeader::dll_characteristics_list() const {
  std::vector<DLL_CHARACTERISTICS> characteristics;
  characteristics.reserve(3);
  std::copy_if(DLL_CHARACTERISTICS_LIST.begin(), DLL_CHARACTERISTICS_LIST.end(),
               std::back_inserter(characteristics),
               [this] (DLL_CHARACTERISTICS f) { return has(f); });

  return characteristics;
}

void OptionalHeader::accept(LIEF::Visitor& visitor) const {
  visitor.visit(*this);
}

std::ostream& operator<<(std::ostream& os, const OptionalHeader& entry) {
  const std::vector<OptionalHeader::DLL_CHARACTERISTICS> list = entry.dll_characteristics_list();
  std::vector<std::string> list_str;
  list_str.reserve(list.size());
  std::transform(list.begin(), list.end(), std::back_inserter(list_str),
                 [] (const auto c) { return to_string(c); });

  os << fmt::format("Magic:                      0x{:04x} ({})\n", (uint32_t)entry.magic(), to_string(entry.magic()))
     << fmt::format("Linker version:             {}.{}\n", entry.major_linker_version(), entry.minor_linker_version())
     << fmt::format("Size of code:               0x{:x}\n", entry.sizeof_code())
     << fmt::format("Size of initialized data:   0x{:x}\n", entry.sizeof_initialized_data())
     << fmt::format("Size of uninitialized data: 0x{:x}\n", entry.sizeof_uninitialized_data())
     << fmt::format("Entrypoint:                 0x{:x}\n", entry.addressof_entrypoint())
     << fmt::format("Base of code:               0x{:x}\n", entry.baseof_code())
     << fmt::format("Base of data:               0x{:x}\n", entry.baseof_data())
     << fmt::format("Imagebase:                  0x{:x}\n", entry.imagebase())
     << fmt::format("Section alignment:          0x{:x}\n", entry.section_alignment())
     << fmt::format("File alignment:             0x{:x}\n", entry.file_alignment())
     << fmt::format("Operating system version:   {}.{}\n", entry.major_operating_system_version(), entry.minor_operating_system_version())
     << fmt::format("Image version:              {}.{}\n", entry.major_image_version(), entry.minor_image_version())
     << fmt::format("Subsystem version:          {}.{}\n", entry.major_subsystem_version(), entry.minor_subsystem_version())
     << fmt::format("Win32 Version Value:        0x{:x}\n", entry.win32_version_value())
     << fmt::format("Size of image:              0x{:x}\n", entry.sizeof_image())
     << fmt::format("Size of headers:            0x{:x}\n", entry.sizeof_headers())
     << fmt::format("Checksum:                   0x{:x}\n", entry.checksum())
     << fmt::format("Subsystem:                  {}\n", to_string(entry.subsystem()))
     << fmt::format("Dll characteristics:        {}\n", fmt::join(list_str, ", "))
     << fmt::format("Size of stack reserve:      0x{:x}\n", entry.sizeof_stack_reserve())
     << fmt::format("Size of stack commit:       0x{:x}\n", entry.sizeof_stack_commit())
     << fmt::format("Size of heap reserve:       0x{:x}\n", entry.sizeof_heap_reserve())
     << fmt::format("Size of heap commit:        0x{:x}\n", entry.sizeof_heap_commit())
     << fmt::format("Loader flags:               0x{:x}\n", entry.loader_flags())
     << fmt::format("Number of directories:      {}\n", entry.numberof_rva_and_size());
  return os;
}

const char* to_string(OptionalHeader::DLL_CHARACTERISTICS e) {
  CONST_MAP(OptionalHeader::DLL_CHARACTERISTICS, const char*, 11) enumStrings {
    { OptionalHeader::DLL_CHARACTERISTICS::HIGH_ENTROPY_VA,       "HIGH_ENTROPY_VA" },
    { OptionalHeader::DLL_CHARACTERISTICS::DYNAMIC_BASE,          "DYNAMIC_BASE" },
    { OptionalHeader::DLL_CHARACTERISTICS::FORCE_INTEGRITY,       "FORCE_INTEGRITY" },
    { OptionalHeader::DLL_CHARACTERISTICS::NX_COMPAT,             "NX_COMPAT" },
    { OptionalHeader::DLL_CHARACTERISTICS::NO_ISOLATION,          "NO_ISOLATION" },
    { OptionalHeader::DLL_CHARACTERISTICS::NO_SEH,                "NO_SEH" },
    { OptionalHeader::DLL_CHARACTERISTICS::NO_BIND,               "NO_BIND" },
    { OptionalHeader::DLL_CHARACTERISTICS::APPCONTAINER,          "APPCONTAINER" },
    { OptionalHeader::DLL_CHARACTERISTICS::WDM_DRIVER,            "WDM_DRIVER" },
    { OptionalHeader::DLL_CHARACTERISTICS::GUARD_CF,              "GUARD_CF" },
    { OptionalHeader::DLL_CHARACTERISTICS::TERMINAL_SERVER_AWARE, "TERMINAL_SERVER_AWARE" },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNKNOWN" : it->second;
}

const char* to_string(OptionalHeader::SUBSYSTEM e) {
  CONST_MAP(OptionalHeader::SUBSYSTEM, const char*, 14) enumStrings {
    { OptionalHeader::SUBSYSTEM::UNKNOWN,                  "UNKNOWN" },
    { OptionalHeader::SUBSYSTEM::NATIVE,                   "NATIVE" },
    { OptionalHeader::SUBSYSTEM::WINDOWS_GUI,              "WINDOWS_GUI" },
    { OptionalHeader::SUBSYSTEM::WINDOWS_CUI,              "WINDOWS_CUI" },
    { OptionalHeader::SUBSYSTEM::OS2_CUI,                  "OS2_CUI" },
    { OptionalHeader::SUBSYSTEM::POSIX_CUI,                "POSIX_CUI" },
    { OptionalHeader::SUBSYSTEM::NATIVE_WINDOWS,           "NATIVE_WINDOWS" },
    { OptionalHeader::SUBSYSTEM::WINDOWS_CE_GUI,           "WINDOWS_CE_GUI" },
    { OptionalHeader::SUBSYSTEM::EFI_APPLICATION,          "EFI_APPLICATION" },
    { OptionalHeader::SUBSYSTEM::EFI_BOOT_SERVICE_DRIVER,  "EFI_BOOT_SERVICE_DRIVER" },
    { OptionalHeader::SUBSYSTEM::EFI_RUNTIME_DRIVER,       "EFI_RUNTIME_DRIVER" },
    { OptionalHeader::SUBSYSTEM::EFI_ROM,                  "EFI_ROM" },
    { OptionalHeader::SUBSYSTEM::XBOX,                     "XBOX" },
    { OptionalHeader::SUBSYSTEM::WINDOWS_BOOT_APPLICATION, "WINDOWS_BOOT_APPLICATION" },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNKNOWN" : it->second;
}

}
}
