#include <Windows.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

// This executable takes a Windows DLL and uses it to generate
// a module-definition file [1] which forwards all the exported
// symbols from the DLL and redirects them back to the DLL.
// This allows node.exe to export the same symbols as libnode.dll
// when building Node.js as a shared library. This is conceptually
// similar to the create_expfile.sh script used on AIX.
//
// Generating this .def file requires parsing data out of the
// PE32/PE32+ file format. Helper structs are defined in <Windows.h>
// hence why this is an executable and not a script. See [2] for
// details on the PE format.
//
// [1]: https://docs.microsoft.com/en-us/cpp/build/reference/module-definition-dot-def-files
// [2]: https://docs.microsoft.com/en-us/windows/win32/debug/pe-format

// The PE32 format encodes pointers as Relative Virtual Addresses
// which are 32 bit offsets from the start of the image. This helper
// class hides the mess of the pointer arithmetic
struct RelativeAddress {
  uintptr_t root;
  uintptr_t offset = 0;

  explicit RelativeAddress(HMODULE handle) noexcept
      : RelativeAddress(handle, 0) {}

  // LoadLibraryEx with LOAD_LIBRARY_AS_IMAGE_RESOURCE tags the returned
  // handle by setting one of its two lowest bits. Mask them off to recover
  // the actual base address of the mapping.
  RelativeAddress(HMODULE handle, uintptr_t offset) noexcept
      : root(reinterpret_cast<uintptr_t>(handle) &
             ~static_cast<uintptr_t>(3)),
        offset(offset) {}

  RelativeAddress(uintptr_t root, uintptr_t offset) noexcept
      : root(root), offset(offset) {}

  template <typename T>
  const T* AsPtrTo() const noexcept {
    return reinterpret_cast<const T*>(root + offset);
  }

  template <typename T>
  T Read() const noexcept {
    return *AsPtrTo<T>();
  }

  RelativeAddress AtOffset(uintptr_t amount) const noexcept {
    return {root, offset + amount};
  }

  RelativeAddress operator+(uintptr_t amount) const noexcept {
    return {root, offset + amount};
  }

  RelativeAddress ReadRelativeAddress() const noexcept {
    return {root, Read<uint32_t>()};
  }
};

struct Symbol {
  std::string name;
  uint32_t rva;
};

// A wrapper around a memory-mapped Windows DLL image. The DLL is mapped as
// an image resource (laid out as if loaded, but never executed), so its
// architecture does not need to match ours; this allows generating the
// .def file for a cross-compiled DLL. This steps through the PE file
// structure to find the export directory and pulls out a list of all the
// exported symbols.
struct Library {
  HMODULE library;
  std::string libraryName;
  std::vector<IMAGE_SECTION_HEADER> sections;
  std::vector<Symbol> exportedSymbols;

  // Location of the export directory itself, used to detect forwarders.
  uint32_t exportDirStart;
  uint32_t exportDirSize;

  explicit Library(HMODULE library) : library(library) {
    auto libnode = RelativeAddress(library);

    // At relative offset 0x3C is a 32 bit offset to the COFF signature, 4 bytes
    // after that is the start of the COFF header.
    auto coffHeaderPtr =
        libnode.AtOffset(0x3C).ReadRelativeAddress().AtOffset(4);
    auto coffHeader = coffHeaderPtr.AsPtrTo<IMAGE_FILE_HEADER>();

    // After the coff header is the Optional Header (which is not optional). We
    // don't know what type of optional header we have without examining the
    // magic number
    auto optionalHeaderPtr = coffHeaderPtr.AtOffset(sizeof(IMAGE_FILE_HEADER));
    auto optionalHeader = optionalHeaderPtr.AsPtrTo<IMAGE_OPTIONAL_HEADER>();

    // The section table starts right after the optional header.
    auto sectionTablePtr =
        optionalHeaderPtr.AtOffset(coffHeader->SizeOfOptionalHeader);
    const IMAGE_SECTION_HEADER* firstSection =
        sectionTablePtr.AsPtrTo<IMAGE_SECTION_HEADER>();
    sections.assign(firstSection, firstSection + coffHeader->NumberOfSections);

    auto exportDirectory =
        (optionalHeader->Magic == 0x20b)
            ? optionalHeaderPtr.AsPtrTo<IMAGE_OPTIONAL_HEADER64>()
                  ->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
            : optionalHeaderPtr.AsPtrTo<IMAGE_OPTIONAL_HEADER32>()
                  ->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    exportDirStart = exportDirectory.VirtualAddress;
    exportDirSize = exportDirectory.Size;

    auto exportTable = libnode.AtOffset(exportDirectory.VirtualAddress)
            .AsPtrTo<IMAGE_EXPORT_DIRECTORY>();

    // This is the name of the library without the suffix, this is more robust
    // than parsing the filename as this is what the linker uses.
    libraryName = libnode.AtOffset(exportTable->Name).AsPtrTo<char>();
    libraryName = libraryName.substr(0, libraryName.size() - 4);

    const uint32_t* functionNameTable =
        libnode.AtOffset(exportTable->AddressOfNames).AsPtrTo<uint32_t>();
    const uint32_t* functionLocations =
        libnode.AtOffset(exportTable->AddressOfFunctions).AsPtrTo<uint32_t>();
    const uint16_t* functionOrdinals =
        libnode.AtOffset(exportTable->AddressOfNameOrdinals)
            .AsPtrTo<uint16_t>();

    // Given an RVA, parse it as a std::string. The resulting string is empty
    // if the symbol does not have a name (i.e. it is ordinal only).
    auto nameRvaToName = [&](uint32_t rva) -> std::string {
      auto namePtr = libnode.AtOffset(rva).AsPtrTo<char>();
      if (namePtr == nullptr) return {};
      return {namePtr};
    };
    for (uint32_t i = 0; i < exportTable->NumberOfNames; ++i) {
      exportedSymbols.push_back({nameRvaToName(functionNameTable[i]),
                                 functionLocations[functionOrdinals[i]]});
    }
  }

  ~Library() { FreeLibrary(library); }

  bool IsRvaExecutable(uint32_t rva) const {
    for (const auto& s : sections) {
      if (rva >= s.VirtualAddress &&
          rva < s.VirtualAddress + s.Misc.VirtualSize) {
        return (s.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
      }
    }
    return true;
  }

  bool IsForwarderRva(uint32_t rva) const {
    return rva >= exportDirStart && rva < exportDirStart + exportDirSize;
  }
};

Library LoadLibraryOrExit(const char* dllPath) {
  auto library =
      LoadLibraryEx(dllPath, nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  if (library != nullptr) return Library(library);

  auto error = GetLastError();
  std::cerr << "ERROR: Failed to load " << dllPath << std::endl;
  LPCSTR buffer = nullptr;
  auto rc = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      nullptr,
      error,
      LANG_USER_DEFAULT,
      (LPSTR)&buffer,
      0,
      nullptr);
  if (rc != 0) {
    std::cerr << buffer << std::endl;
    LocalFree((HLOCAL)buffer);
  }
  exit(1);
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0]
              << " path\\to\\libnode.dll path\\to\\node.def" << std::endl;
    return 1;
  }

  auto libnode = LoadLibraryOrExit(argv[1]);
  auto defFile = std::ofstream(argv[2]);
  defFile << "EXPORTS" << std::endl;

  for (const Symbol& symbol : libnode.exportedSymbols) {
    // If a symbol doesn't have a name then it has been exported as an
    // ordinal only. We assume that only named symbols are exported.
    if (symbol.name.empty()) continue;

    if (symbol.rva == 0) {
      std::cerr << "WARNING: " << symbol.name
                << " appears in export table but is not a valid symbol"
                << std::endl;
      continue;
    }

    defFile << "    " << symbol.name << " = " << libnode.libraryName << "."
            << symbol.name;

    // Nothing distinguishes exported global data from exported functions
    // with C linkage. If we do not specify the DATA keyword for such symbols
    // then consumers of the .def file will get a linker error. This manifests
    // as nodedbg_ symbols not being found. We assert that if the symbol's
    // RVA falls in a section with the IMAGE_SCN_MEM_EXECUTE characteristic
    // then it is a function, not data.
    //
    // A forwarder export is the exception: its RVA points back inside the
    // export directory, at a redirect string like "NTDLL.RtlAllocateHeap",
    // rather than at code or data. The export directory lives in a
    // non-executable section, but forwarders resolve to functions, so they
    // must not be marked DATA.
    if (!libnode.IsForwarderRva(symbol.rva) &&
        !libnode.IsRvaExecutable(symbol.rva)) {
      defFile << " DATA";
    }
    defFile << std::endl;
  }

  return 0;
}
