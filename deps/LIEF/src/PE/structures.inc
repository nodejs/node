/// The maximum number of sections that a COFF object can have (inclusive).
static const int32_t MaxNumberOfSections16 = 65279;

/// The PE signature bytes that follows the DOS stub header.
static const char PE_Magic[] = { 'P', 'E', '\0', '\0' };

static const char BigObjMagic[] = {
  '\xc7', '\xa1', '\xba', '\xd1', '\xee', '\xba', '\xa9', '\x4b',
  '\xaf', '\x20', '\xfa', '\xf6', '\x6a', '\xa4', '\xdc', '\xb8',
};

static const uint8_t DEFAULT_NUMBER_DATA_DIRECTORIES = 16;

#pragma pack(push,1)
struct pe_header {
  char     signature[sizeof(PE_Magic)];
  uint16_t Machine;
  uint16_t NumberOfSections;
  uint32_t TimeDateStamp;
  uint32_t PointerToSymbolTable;
  uint32_t NumberOfSymbols;
  uint16_t SizeOfOptionalHeader;
  uint16_t Characteristics;
};


struct pe_relocation {
  uint32_t VirtualAddress;
  uint32_t SymbolTableIndex;
  uint16_t Type;
};

struct pe_base_relocation_block {
  uint32_t PageRVA;
  uint32_t BlockSize;
};


struct pe_symbol {
  union {
    char ShortName[8];
    struct
    {
      uint32_t Zeroes;
      uint32_t Offset;
    } Name;
  } Name;
  uint32_t Value;
  int16_t  SectionNumber;
  uint16_t Type;
  uint8_t  StorageClass;
  uint8_t  NumberOfAuxSymbols;
};


struct pe_section {
  char     Name[8];
  uint32_t VirtualSize;
  uint32_t VirtualAddress;
  uint32_t SizeOfRawData;
  uint32_t PointerToRawData;
  uint32_t PointerToRelocations;
  uint32_t PointerToLineNumbers;
  uint16_t NumberOfRelocations;
  uint16_t NumberOfLineNumbers;
  uint32_t Characteristics;
};

struct AuxiliaryFunctionDefinition {
  uint32_t TagIndex;
  uint32_t TotalSize;
  uint32_t PointerToLinenumber;
  uint32_t PointerToNextFunction;
  char     unused[2];
};

struct AuxiliarybfAndefSymbol {
  uint8_t  unused1[4];
  uint16_t Linenumber;
  uint8_t  unused2[6];
  uint32_t PointerToNextFunction;
  uint8_t  unused3[2];
};

struct AuxiliaryWeakExternal {
  uint32_t TagIndex;
  uint32_t Characteristics;
  uint8_t  unused[10];
};


struct AuxiliarySectionDefinition {
  uint32_t Length;
  uint16_t NumberOfRelocations;
  uint16_t NumberOfLinenumbers;
  uint32_t CheckSum;
  uint32_t Number;
  uint8_t  Selection;
  char     unused;
};

struct AuxiliaryCLRToken {
  uint8_t  AuxType;
  uint8_t  unused1;
  uint32_t SymbolTableIndex;
  char     unused2[12];
};

union Auxiliary {
  AuxiliaryFunctionDefinition FunctionDefinition;
  AuxiliarybfAndefSymbol      bfAndefSymbol;
  AuxiliaryWeakExternal       WeakExternal;
  AuxiliarySectionDefinition  SectionDefinition;
};


/// The Import Directory Table.
///
/// There is a single array of these and one entry per imported DLL.
struct pe_import {
  uint32_t ImportLookupTableRVA;
  uint32_t TimeDateStamp;
  uint32_t ForwarderChain;
  uint32_t NameRVA;
  uint32_t ImportAddressTableRVA;
};


struct ImportLookupTableEntry32 {
  uint32_t data;
};

struct ImportLookupTableEntry64 {
  uint64_t data;
};


struct pe32_tls {
  uint32_t RawDataStartVA;
  uint32_t RawDataEndVA;
  uint32_t AddressOfIndex;
  uint32_t AddressOfCallback;
  uint32_t SizeOfZeroFill;
  uint32_t Characteristics;
};


struct pe64_tls {
  uint64_t RawDataStartVA;
  uint64_t RawDataEndVA;
  uint64_t AddressOfIndex;
  uint64_t AddressOfCallback;
  uint32_t SizeOfZeroFill;
  uint32_t Characteristics;
};


/// The DOS compatible header at the front of all PEs.
struct pe_dos_header {
  uint16_t Magic;
  uint16_t UsedBytesInTheLastPage;
  uint16_t FileSizeInPages;
  uint16_t NumberOfRelocationItems;
  uint16_t HeaderSizeInParagraphs;
  uint16_t MinimumExtraParagraphs;
  uint16_t MaximumExtraParagraphs;
  uint16_t InitialRelativeSS;
  uint16_t InitialSP;
  uint16_t Checksum;
  uint16_t InitialIP;
  uint16_t InitialRelativeCS;
  uint16_t AddressOfRelocationTable;
  uint16_t OverlayNumber;
  uint16_t Reserved[4];
  uint16_t OEMid;
  uint16_t OEMinfo;
  uint16_t Reserved2[10];
  uint32_t AddressOfNewExeHeader;
};

struct pe64_optional_header {
  uint16_t Magic;
  uint8_t  MajorLinkerVersion;
  uint8_t  MinorLinkerVersion;
  uint32_t SizeOfCode;
  uint32_t SizeOfInitializedData;
  uint32_t SizeOfUninitializedData;
  uint32_t AddressOfEntryPoint; // RVA
  uint32_t BaseOfCode; // RVA
  //uint32_t BaseOfData; // RVA
  uint64_t ImageBase;
  uint32_t SectionAlignment;
  uint32_t FileAlignment;
  uint16_t MajorOperatingSystemVersion;
  uint16_t MinorOperatingSystemVersion;
  uint16_t MajorImageVersion;
  uint16_t MinorImageVersion;
  uint16_t MajorSubsystemVersion;
  uint16_t MinorSubsystemVersion;
  uint32_t Win32VersionValue;
  uint32_t SizeOfImage;
  uint32_t SizeOfHeaders;
  uint32_t CheckSum;
  uint16_t Subsystem;
  uint16_t DLLCharacteristics;
  uint64_t SizeOfStackReserve;
  uint64_t SizeOfStackCommit;
  uint64_t SizeOfHeapReserve;
  uint64_t SizeOfHeapCommit;
  uint32_t LoaderFlags;
  uint32_t NumberOfRvaAndSize;
};


struct pe32_optional_header {
  uint16_t Magic;
  uint8_t  MajorLinkerVersion;
  uint8_t  MinorLinkerVersion;
  uint32_t SizeOfCode;
  uint32_t SizeOfInitializedData;
  uint32_t SizeOfUninitializedData;
  uint32_t AddressOfEntryPoint; // RVA
  uint32_t BaseOfCode; // RVA
  uint32_t BaseOfData; // RVA
  uint32_t ImageBase;
  uint32_t SectionAlignment;
  uint32_t FileAlignment;
  uint16_t MajorOperatingSystemVersion;
  uint16_t MinorOperatingSystemVersion;
  uint16_t MajorImageVersion;
  uint16_t MinorImageVersion;
  uint16_t MajorSubsystemVersion;
  uint16_t MinorSubsystemVersion;
  uint32_t Win32VersionValue;
  uint32_t SizeOfImage;
  uint32_t SizeOfHeaders;
  uint32_t CheckSum;
  uint16_t Subsystem;
  uint16_t DLLCharacteristics;
  uint32_t SizeOfStackReserve;
  uint32_t SizeOfStackCommit;
  uint32_t SizeOfHeapReserve;
  uint32_t SizeOfHeapCommit;
  uint32_t LoaderFlags;
  uint32_t NumberOfRvaAndSize;
};


struct pe_data_directory {
  uint32_t RelativeVirtualAddress;
  uint32_t Size;
};


struct pe_debug {
  uint32_t Characteristics;
  uint32_t TimeDateStamp;
  uint16_t MajorVersion;
  uint16_t MinorVersion;
  uint32_t Type;
  uint32_t SizeOfData;
  uint32_t AddressOfRawData;
  uint32_t PointerToRawData;
};


struct pe_pdb_70 {
  uint32_t cv_signature;
  uint8_t  signature[16];
  uint32_t age;
  //char*    filename;
};

struct pe_pdb_20 {
  uint32_t cv_signature;
  uint32_t offset;
  uint32_t signature;
  uint32_t age;
  //char*    filename;
};

struct pe_pogo {
  uint32_t start_rva;
  uint32_t size;
  //char     name[1];
};


struct pe_resource_directory_table {
  uint32_t Characteristics;
  uint32_t TimeDateStamp;
  uint16_t MajorVersion;
  uint16_t MinorVersion;
  uint16_t NumberOfNameEntries;
  uint16_t NumberOfIDEntries;
};

struct pe_resource_directory_entries {
  union {
    uint32_t NameRVA;
    uint32_t IntegerID;
  } NameID;
  uint32_t RVA;
};

struct pe_resource_data_entry {
  uint32_t DataRVA;
  uint32_t Size;
  uint32_t Codepage;
  uint32_t Reserved;
};

struct pe_resource_string {
  int16_t Length;
  uint16_t Name[1];
};

struct pe_resource_acceltableentry {
  int16_t fFlags;
  int16_t wAnsi;
  int16_t wId;
  int16_t padding;
};

//
// Export structures
//
struct pe_export_directory_table {
  uint32_t ExportFlags;           ///< Reserverd must be 0
  uint32_t Timestamp;             ///< The time and date that the export data was created
  uint16_t MajorVersion;          ///< The Major version number
  uint16_t MinorVersion;          ///< The Minor version number
  uint32_t NameRVA;               ///< The address of the ASCII DLL's name (RVA)
  uint32_t OrdinalBase;           ///< The starting ordinal number for exports. (Usually 1)
  uint32_t AddressTableEntries;   ///< Number of entries in the export address table
  uint32_t NumberOfNamePointers;  ///< Number of entries in the name pointer table
  uint32_t ExportAddressTableRVA; ///< Address of the export address table (RVA)
  uint32_t NamePointerRVA;        ///< Address of the name pointer table (RVA)
  uint32_t OrdinalTableRVA;       ///< Address of the ordinal table (RVA)
};


struct pe_resource_fixed_file_info {
  uint32_t signature;          // e.g.  0xfeef04bd
  uint32_t struct_version;      // e.g.  0x00000042 = "0.42"
  uint32_t file_version_MS;    // e.g.  0x00030075 = "3.75"
  uint32_t file_version_LS;    // e.g.  0x00000031 = "0.31"
  uint32_t product_version_MS; // e.g.  0x00030010 = "3.10"
  uint32_t product_version_LS; // e.g.  0x00000031 = "0.31"
  uint32_t file_flags_mask;    // = 0x3F for version "0.42"
  uint32_t file_flags;         // e.g.  VFF_DEBUG | VFF_PRERELEASE
  uint32_t file_OS;            // e.g.  VOS_DOS_WINDOWS16
  uint32_t file_type;          // e.g.  VFT_DRIVER
  uint32_t file_subtype;       // e.g.  VFT2_DRV_KEYBOARD
  uint32_t file_date_MS;       // e.g.  0
  uint32_t file_date_LS;       // e.g.  0
};


struct pe_resource_version_info {
  uint16_t length;
  uint16_t sizeof_value;
  uint16_t type;
  char16_t key[16];
  // uint16_t padding;
  //
  // uint16_t padding;
  // uint16_t children
};

/// Resource icons directory structure
/// Based on https://docs.microsoft.com/en-us/windows/win32/menurc/resources-reference
///
/// This is the begining of the RESOURCE_TYPES::GROUP_ICON content
struct pe_resource_icon_dir {
  uint16_t reserved; ///< Reserved
  uint16_t type;     ///< Resource type (1 for icons)
  uint16_t count;    ///< Number of icons
  // GRPICONDIRENTRY idEntries[];
};


/// Structure that follows pe_resource_icon_dir in a resource entry
struct pe_resource_icon_group {
  uint8_t width;        ///< Width, in pixels, of the image
  uint8_t height;       ///< Height, in pixels, of the image
  uint8_t color_count;  ///< Number of colors in image (0 if >=8bpp)
  uint8_t reserved;     ///< Reserved (must be 0)
  uint16_t planes;      ///< Color Planes
  uint16_t bit_count;   ///< Bits per pixel
  uint32_t size;        ///< Size of the image in bytes
  uint16_t ID;          ///< The associated ID
};

/// Structure that follows pe_resource_icon_dir in a icon **file**
struct pe_icon_header {
  uint8_t width;        ///< Width, in pixels, of the image
  uint8_t height;       ///< Height, in pixels, of the image
  uint8_t color_count;  ///< Number of colors in image (0 if >=8bpp)
  uint8_t reserved;     ///< Reserved (must be 0)
  uint16_t planes;      ///< Color Planes
  uint16_t bit_count;   ///< Bits per pixel
  uint32_t size;        ///< Size of the image in bytes
  uint32_t offset;      ///< Offset to the pixels
};

/// Extended dialog box template
struct pe_dialog_template_ext {
  uint16_t version;
  uint16_t signature;
  uint32_t help_id;
  uint32_t ext_style;
  uint32_t style;
  uint16_t nbof_items;
  int16_t x;
  int16_t y;
  int16_t cx;
  int16_t cy;
  // sz_Or_Ord menu;
  // sz_Or_Ord windowClass;
  // char16_t  title[titleLen];
  // uint16_t  pointsize;
  // uint16_t  weight;
  // uint8_t   italic;
  // uint8_t   charset;
  // char16_t  typeface[stringLen];
};

/// Dialog box template
struct pe_dialog_template {
  uint32_t style;
  uint32_t ext_style;
  uint16_t nbof_items;
  int16_t x;
  int16_t y;
  int16_t cx;
  int16_t cy;
};


/// Extended dialog box template item
struct pe_dialog_item_template_ext {
  uint32_t help_id;
  uint32_t ext_style;
  uint32_t style;
  int16_t x;
  int16_t y;
  int16_t cx;
  int16_t cy;
  uint32_t id;
  // sz_Or_Ord windowClass;
  // sz_Or_Ord title;
  // uint16_t extra_count;
};


/// Dialog box template item
struct pe_dialog_item_template {
  uint32_t style;
  uint32_t ext_style;
  int16_t x;
  int16_t y;
  int16_t cx;
  int16_t cy;
  uint16_t id;
};

struct pe_code_integrity {
  uint16_t Flags;
  uint16_t Catalog;
  uint32_t CatalogOffset;
  uint32_t Reserved;
};

struct pe_exception_entry_x64 {
  uint32_t address_start_rva;
  uint32_t address_end_rva;
  uint32_t unwind_info_rva;
};


struct pe_exception_entry_mips {
  uint32_t address_start_va;
  uint32_t address_end_va;
  uint32_t exception_handler;
  uint32_t handler_data;
  uint32_t prolog_end_address;
};

struct pe_exception_entry_arm {
  uint32_t address_start_va;
  uint32_t data;
};

#pragma pack(pop)
