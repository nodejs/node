

enum _LIEF_EN_2(MACHO_TYPES, uint32_t) {
  _LIEF_EI(MH_MAGIC)    = 0xFEEDFACEu, /**< 32-bit big-endian magic */
  _LIEF_EI(MH_CIGAM)    = 0xCEFAEDFEu, /**< 32-bit little-endian magic */
  _LIEF_EI(MH_MAGIC_64) = 0xFEEDFACFu, /**< 64-bit big-endian magic */
  _LIEF_EI(MH_CIGAM_64) = 0xCFFAEDFEu, /**< 64-bit little-endian magic */
  _LIEF_EI(FAT_MAGIC)   = 0xCAFEBABEu, /**< big-endian fat magic */
  _LIEF_EI(FAT_CIGAM)   = 0xBEBAFECAu  /**< little-endian fat magic */
};

enum _LIEF_EN(FILE_TYPES) {
  _LIEF_EI(MH_OBJECT)      = 0x1u,
  _LIEF_EI(MH_EXECUTE)     = 0x2u,
  _LIEF_EI(MH_FVMLIB)      = 0x3u,
  _LIEF_EI(MH_CORE)        = 0x4u,
  _LIEF_EI(MH_PRELOAD)     = 0x5u,
  _LIEF_EI(MH_DYLIB)       = 0x6u,
  _LIEF_EI(MH_DYLINKER)    = 0x7u,
  _LIEF_EI(MH_BUNDLE)      = 0x8u,
  _LIEF_EI(MH_DYLIB_STUB)  = 0x9u,
  _LIEF_EI(MH_DSYM)        = 0xAu,
  _LIEF_EI(MH_KEXT_BUNDLE) = 0xBu
};


enum _LIEF_EN(HEADER_FLAGS) {
  _LIEF_EI(MH_NOUNDEFS)                = 0x00000001u,
  _LIEF_EI(MH_INCRLINK)                = 0x00000002u,
  _LIEF_EI(MH_DYLDLINK)                = 0x00000004u,
  _LIEF_EI(MH_BINDATLOAD)              = 0x00000008u,
  _LIEF_EI(MH_PREBOUND)                = 0x00000010u,
  _LIEF_EI(MH_SPLIT_SEGS)              = 0x00000020u,
  _LIEF_EI(MH_LAZY_INIT)               = 0x00000040u,
  _LIEF_EI(MH_TWOLEVEL)                = 0x00000080u,
  _LIEF_EI(MH_FORCE_FLAT)              = 0x00000100u,
  _LIEF_EI(MH_NOMULTIDEFS)             = 0x00000200u,
  _LIEF_EI(MH_NOFIXPREBINDING)         = 0x00000400u,
  _LIEF_EI(MH_PREBINDABLE)             = 0x00000800u,
  _LIEF_EI(MH_ALLMODSBOUND)            = 0x00001000u,
  _LIEF_EI(MH_SUBSECTIONS_VIA_SYMBOLS) = 0x00002000u,
  _LIEF_EI(MH_CANONICAL)               = 0x00004000u,
  _LIEF_EI(MH_WEAK_DEFINES)            = 0x00008000u,
  _LIEF_EI(MH_BINDS_TO_WEAK)           = 0x00010000u,
  _LIEF_EI(MH_ALLOW_STACK_EXECUTION)   = 0x00020000u,
  _LIEF_EI(MH_ROOT_SAFE)               = 0x00040000u,
  _LIEF_EI(MH_SETUID_SAFE)             = 0x00080000u,
  _LIEF_EI(MH_NO_REEXPORTED_DYLIBS)    = 0x00100000u,
  _LIEF_EI(MH_PIE)                     = 0x00200000u,
  _LIEF_EI(MH_DEAD_STRIPPABLE_DYLIB)   = 0x00400000u,
  _LIEF_EI(MH_HAS_TLV_DESCRIPTORS)     = 0x00800000u,
  _LIEF_EI(MH_NO_HEAP_EXECUTION)       = 0x01000000u,
  _LIEF_EI(MH_APP_EXTENSION_SAFE)      = 0x02000000u
};


enum _LIEF_EN(LOAD_COMMAND_TYPES) {
  /* Constants for the "cmd" field in llvm::MachO::load_command */
  _LIEF_EI(LC_SEGMENT)                  = 0x00000001u,
  _LIEF_EI(LC_SYMTAB)                   = 0x00000002u,
  _LIEF_EI(LC_SYMSEG)                   = 0x00000003u,
  _LIEF_EI(LC_THREAD)                   = 0x00000004u,
  _LIEF_EI(LC_UNIXTHREAD)               = 0x00000005u,
  _LIEF_EI(LC_LOADFVMLIB)               = 0x00000006u,
  _LIEF_EI(LC_IDFVMLIB)                 = 0x00000007u,
  _LIEF_EI(LC_IDENT)                    = 0x00000008u,
  _LIEF_EI(LC_FVMFILE)                  = 0x00000009u,
  _LIEF_EI(LC_PREPAGE)                  = 0x0000000Au,
  _LIEF_EI(LC_DYSYMTAB)                 = 0x0000000Bu,
  _LIEF_EI(LC_LOAD_DYLIB)               = 0x0000000Cu,
  _LIEF_EI(LC_ID_DYLIB)                 = 0x0000000Du,
  _LIEF_EI(LC_LOAD_DYLINKER)            = 0x0000000Eu,
  _LIEF_EI(LC_ID_DYLINKER)              = 0x0000000Fu,
  _LIEF_EI(LC_PREBOUND_DYLIB)           = 0x00000010u,
  _LIEF_EI(LC_ROUTINES)                 = 0x00000011u,
  _LIEF_EI(LC_SUB_FRAMEWORK)            = 0x00000012u,
  _LIEF_EI(LC_SUB_UMBRELLA)             = 0x00000013u,
  _LIEF_EI(LC_SUB_CLIENT)               = 0x00000014u,
  _LIEF_EI(LC_SUB_LIBRARY)              = 0x00000015u,
  _LIEF_EI(LC_TWOLEVEL_HINTS)           = 0x00000016u,
  _LIEF_EI(LC_PREBIND_CKSUM)            = 0x00000017u,
  _LIEF_EI(LC_LOAD_WEAK_DYLIB)          = 0x80000018u,
  _LIEF_EI(LC_SEGMENT_64)               = 0x00000019u,
  _LIEF_EI(LC_ROUTINES_64)              = 0x0000001Au,
  _LIEF_EI(LC_UUID)                     = 0x0000001Bu,
  _LIEF_EI(LC_RPATH)                    = 0x8000001Cu,
  _LIEF_EI(LC_CODE_SIGNATURE)           = 0x0000001Du,
  _LIEF_EI(LC_SEGMENT_SPLIT_INFO)       = 0x0000001Eu,
  _LIEF_EI(LC_REEXPORT_DYLIB)           = 0x8000001Fu,
  _LIEF_EI(LC_LAZY_LOAD_DYLIB)          = 0x00000020u,
  _LIEF_EI(LC_ENCRYPTION_INFO)          = 0x00000021u,
  _LIEF_EI(LC_DYLD_INFO)                = 0x00000022u,
  _LIEF_EI(LC_DYLD_INFO_ONLY)           = 0x80000022u,
  _LIEF_EI(LC_LOAD_UPWARD_DYLIB)        = 0x80000023u,
  _LIEF_EI(LC_VERSION_MIN_MACOSX)       = 0x00000024u,
  _LIEF_EI(LC_VERSION_MIN_IPHONEOS)     = 0x00000025u,
  _LIEF_EI(LC_FUNCTION_STARTS)          = 0x00000026u,
  _LIEF_EI(LC_DYLD_ENVIRONMENT)         = 0x00000027u,
  _LIEF_EI(LC_MAIN)                     = 0x80000028u,
  _LIEF_EI(LC_DATA_IN_CODE)             = 0x00000029u,
  _LIEF_EI(LC_SOURCE_VERSION)           = 0x0000002Au,
  _LIEF_EI(LC_DYLIB_CODE_SIGN_DRS)      = 0x0000002Bu,
  _LIEF_EI(LC_ENCRYPTION_INFO_64)       = 0x0000002Cu,
  _LIEF_EI(LC_LINKER_OPTION)            = 0x0000002Du,
  _LIEF_EI(LC_LINKER_OPTIMIZATION_HINT) = 0x0000002Eu,
  _LIEF_EI(LC_VERSION_MIN_TVOS)         = 0x0000002Fu,
  _LIEF_EI(LC_VERSION_MIN_WATCHOS)      = 0x00000030u,
  _LIEF_EI(LC_NOTE)                     = 0x00000031u,
  _LIEF_EI(LC_BUILD_VERSION)            = 0x00000032u,
  _LIEF_EI(LC_DYLD_EXPORTS_TRIE)        = 0x80000033u,
  _LIEF_EI(LC_DYLD_CHAINED_FIXUPS)      = 0x80000034u,
  _LIEF_EI(LC_FILESET_ENTRY)            = 0x80000035u,
  _LIEF_EI(LC_ATOM_INFO)                = 0x00000036u,
};


enum {
    /* Flags for the "cmd" field in llvm::MachO::load_command */
    _LIEF_EI(LC_REQ_DYLD)    = 0x80000000u
  };


enum _LIEF_EN(MACHO_SEGMENTS_FLAGS) {
  /* Constant bits for the "flags" field in llvm::MachO::segment_command */
  _LIEF_EI(SG_HIGHVM)              = 0x1u, /**< The file contents for this segment are for the high part of the virtual memory space; the low part is zero filled (for stacks in core files). */
  _LIEF_EI(SG_FVMLIB)              = 0x2u, /**< this segment is the VM that is allocated by a fixed VM library, for overlap checking in the link editor. */
  _LIEF_EI(SG_NORELOC)             = 0x4u, /**< This segment has nothing that was relocated in it and nothing relocated to it. It may be safely replaced without relocation. */
  _LIEF_EI(SG_PROTECTED_VERSION_1) = 0x8u,
};

enum _LIEF_EN(SECTION_FLAGS_HELPER)  {
  /* Constant masks for the "flags" field in llvm::MachO::section and */
  /* llvm::MachO::section_64 */
  _LIEF_EI(SECTION_TYPE_MASK)      = 0x000000ffu, /* SECTION_TYPE */
  _LIEF_EI(SECTION_FLAGS_MASK)     = 0xffffff00u, /* SECTION_ATTRIBUTES */
  _LIEF_EI(SECTION_FLAGS_USR_MASK) = 0xff000000u, /* SECTION_ATTRIBUTES_USR */
  _LIEF_EI(SECTION_FLAGS_SYS_MASK) = 0x00ffff00u  /* SECTION_ATTRIBUTES_SYS */
};

/**
 * @brief These are the section type and attributes fields. A MachO section can
 * have only one Type, but can have any of the attributes specified.
 */
enum _LIEF_EN(MACHO_SECTION_TYPES) {
  /* Constant masks for the "flags[7:0]" field in llvm::MachO::section and */
  /* llvm::MachO::section_64 (mask "flags" with SECTION_TYPE) */
  _LIEF_EI(S_REGULAR)                             = 0x00u, /**< Regular section. */
  _LIEF_EI(S_ZEROFILL)                            = 0x01u, /**< Zero fill on demand section. */
  _LIEF_EI(S_CSTRING_LITERALS)                    = 0x02u, /**< Section with literal C strings. */
  _LIEF_EI(S_4BYTE_LITERALS)                      = 0x03u, /**< Section with 4 byte literals. */
  _LIEF_EI(S_8BYTE_LITERALS)                      = 0x04u, /**< Section with 8 byte literals. */
  _LIEF_EI(S_LITERAL_POINTERS)                    = 0x05u, /**< Section with pointers to literals. */
  _LIEF_EI(S_NON_LAZY_SYMBOL_POINTERS)            = 0x06u, /**< Section with non-lazy symbol pointers. */
  _LIEF_EI(S_LAZY_SYMBOL_POINTERS)                = 0x07u, /**< Section with lazy symbol pointers. */
  _LIEF_EI(S_SYMBOL_STUBS)                        = 0x08u, /**< Section with symbol stubs, byte size of stub in the Reserved2 field. */
  _LIEF_EI(S_MOD_INIT_FUNC_POINTERS)              = 0x09u, /**< Section with only function pointers for initialization. */
  _LIEF_EI(S_MOD_TERM_FUNC_POINTERS)              = 0x0au, /**< Section with only function pointers for termination. */
  _LIEF_EI(S_COALESCED)                           = 0x0bu, /**< Section contains symbols that are to be coalesced. */
  _LIEF_EI(S_GB_ZEROFILL)                         = 0x0cu, /**< Zero fill on demand section (that can be larger than 4 gigabytes). */
  _LIEF_EI(S_INTERPOSING)                         = 0x0du, /**< Section with only pairs of function pointers for interposing. */
  _LIEF_EI(S_16BYTE_LITERALS)                     = 0x0eu, /**< Section with only 16 byte literals. */
  _LIEF_EI(S_DTRACE_DOF)                          = 0x0fu, /**< Section contains DTrace Object Format. */
  _LIEF_EI(S_LAZY_DYLIB_SYMBOL_POINTERS)          = 0x10u, /**< Section with lazy symbol pointers to lazy loaded dylibs. */
  _LIEF_EI(S_THREAD_LOCAL_REGULAR)                = 0x11u, /**< Thread local data section. */
  _LIEF_EI(S_THREAD_LOCAL_ZEROFILL)               = 0x12u, /**< Thread local zerofill section. */
  _LIEF_EI(S_THREAD_LOCAL_VARIABLES)              = 0x13u, /**< Section with thread local variable structure data. */
  _LIEF_EI(S_THREAD_LOCAL_VARIABLE_POINTERS)      = 0x14u, /**< Section with pointers to thread local structures. */
  _LIEF_EI(S_THREAD_LOCAL_INIT_FUNCTION_POINTERS) = 0x15u, /**< Section with thread local variable initialization pointers to functions. */

  _LIEF_EI(LAST_KNOWN_SECTION_TYPE) = 0x15u, /* S_THREAD_LOCAL_INIT_FUNCTION_POINTERS */
};

enum _LIEF_EN(MACHO_SECTION_FLAGS) {

  /* Constant masks for the "flags[31:24]" field in llvm::MachO::section and */
  /* llvm::MachO::section_64 (mask "flags" with SECTION_ATTRIBUTES_USR) */
  _LIEF_EI(S_ATTR_PURE_INSTRUCTIONS)   = 0x80000000u, /**< Section contains only true machine instructions */
  _LIEF_EI(S_ATTR_NO_TOC)              = 0x40000000u, /**< Section contains coalesced symbols that are not to be in a ranlib table of contents. */
  _LIEF_EI(S_ATTR_STRIP_STATIC_SYMS)   = 0x20000000u, /**< Ok to strip static symbols in this section in files with the MY_DYLDLINK flag. */
  _LIEF_EI(S_ATTR_NO_DEAD_STRIP)       = 0x10000000u, /**< No dead stripping. */
  _LIEF_EI(S_ATTR_LIVE_SUPPORT)        = 0x08000000u, /**< Blocks are live if they reference live blocks. */
  _LIEF_EI(S_ATTR_SELF_MODIFYING_CODE) = 0x04000000u, /**< Used with i386 code stubs written on by dyld */
  _LIEF_EI(S_ATTR_DEBUG)               = 0x02000000u, /**< A debug section. */

  /* Constant masks for the "flags[23:8]" field in llvm::MachO::section and */
  /* llvm::MachO::section_64 (mask "flags" with SECTION_ATTRIBUTES_SYS) */
  _LIEF_EI(S_ATTR_SOME_INSTRUCTIONS)   = 0x00000400u, /**< Section contains some machine instructions. */
  _LIEF_EI(S_ATTR_EXT_RELOC)           = 0x00000200u, /**< Section has external relocation entries. */
  _LIEF_EI(S_ATTR_LOC_RELOC)           = 0x00000100u, /**< Section has local relocation entries. */

  /* Constant masks for the value of an indirect symbol in an indirect */
  /* symbol table */
  _LIEF_EI(INDIRECT_SYMBOL_LOCAL) = 0x80000000u,
  _LIEF_EI(INDIRECT_SYMBOL_ABS)   = 0x40000000u
};


enum _LIEF_EN(DataRegionType) {
  /* Constants for the "kind" field in a data_in_code_entry structure */
  _LIEF_EI(DICE_KIND_DATA)             = 1u,
  _LIEF_EI(DICE_KIND_JUMP_TABLE8)      = 2u,
  _LIEF_EI(DICE_KIND_JUMP_TABLE16)     = 3u,
  _LIEF_EI(DICE_KIND_JUMP_TABLE32)     = 4u,
  _LIEF_EI(DICE_KIND_ABS_JUMP_TABLE32) = 5u
};

enum _LIEF_EN(REBASE_TYPES) {
  _LIEF_EI(REBASE_TYPE_POINTER)         = 1u,
  _LIEF_EI(REBASE_TYPE_TEXT_ABSOLUTE32) = 2u,
  _LIEF_EI(REBASE_TYPE_TEXT_PCREL32)    = 3u,
  _LIEF_EI(REBASE_TYPE_THREADED)        = 102u,
};

enum {
  _LIEF_EI(REBASE_OPCODE_MASK)    = 0xF0u,
  _LIEF_EI(REBASE_IMMEDIATE_MASK) = 0x0Fu
};


/** @brief Opcodes used by Dyld info to rebase a Mach-O image */
enum _LIEF_EN_2(REBASE_OPCODES, uint8_t) {
  _LIEF_EI(REBASE_OPCODE_DONE)                               = 0x00u, /**< It's finished */
  _LIEF_EI(REBASE_OPCODE_SET_TYPE_IMM)                       = 0x10u, /**< Set type to immediate (lower 4-bits). Used for ordinal numbers from 0-15 */
  _LIEF_EI(REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB)        = 0x20u, /**< Set segment's index to immediate (lower 4-bits) and segment's offset to following ULEB128 encoding. */
  _LIEF_EI(REBASE_OPCODE_ADD_ADDR_ULEB)                      = 0x30u, /**< Add segment's offset with the following ULEB128 encoding. */
  _LIEF_EI(REBASE_OPCODE_ADD_ADDR_IMM_SCALED)                = 0x40u, /**< Add segment's offset with immediate scaling */
  _LIEF_EI(REBASE_OPCODE_DO_REBASE_IMM_TIMES)                = 0x50u, /**< Rebase in the range of ``[segment's offset; segment's offset + immediate * sizeof(ptr)]`` */
  _LIEF_EI(REBASE_OPCODE_DO_REBASE_ULEB_TIMES)               = 0x60u, /**< Same as REBASE_OPCODE_DO_REBASE_IMM_TIMES but *immediate* is replaced with ULEB128 value */
  _LIEF_EI(REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB)            = 0x70u, /**< Rebase and increment segment's offset with following ULEB128 encoding + pointer's size */
  _LIEF_EI(REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB) = 0x80u  /**< Rebase and skip several bytes */
};


/**
 * Within the dyld_info_command there are
 * differents area of binding. These enums tag these areas.
 */
enum _LIEF_EN(BINDING_CLASS) {
  _LIEF_EI(BIND_CLASS_WEAK)     = 1u,
  _LIEF_EI(BIND_CLASS_LAZY)     = 2u,
  _LIEF_EI(BIND_CLASS_STANDARD) = 3u,
  _LIEF_EI(BIND_CLASS_THREADED) = 100u
};


enum _LIEF_EN(BIND_TYPES) {
  _LIEF_EI(BIND_TYPE_POINTER)         = 1u,
  _LIEF_EI(BIND_TYPE_TEXT_ABSOLUTE32) = 2u,
  _LIEF_EI(BIND_TYPE_TEXT_PCREL32)    = 3u
};

enum _LIEF_EN_2(BIND_SPECIAL_DYLIB, int) {
  _LIEF_EI(BIND_SPECIAL_DYLIB_SELF)            =  0,
  _LIEF_EI(BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE) = -1,
  _LIEF_EI(BIND_SPECIAL_DYLIB_FLAT_LOOKUP)     = -2
};

enum {
  _LIEF_EI(BIND_SYMBOL_FLAGS_WEAK_IMPORT)         = 0x1u,
  _LIEF_EI(BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION) = 0x8u,

  _LIEF_EI(BIND_OPCODE_MASK)                      = 0xF0u,
  _LIEF_EI(BIND_IMMEDIATE_MASK)                   = 0x0Fu
};

/** Opcodes used by Dyld info to bind symbols */
enum _LIEF_EN_2(BIND_OPCODES, uint8_t) {
  _LIEF_EI(BIND_OPCODE_DONE)                             = 0x00u, /**< It's finished */
  _LIEF_EI(BIND_OPCODE_SET_DYLIB_ORDINAL_IMM)            = 0x10u, /**< Set ordinal to immediate (lower 4-bits). Used for ordinal numbers from 0-15 */
  _LIEF_EI(BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB)           = 0x20u, /**< Set ordinal to following ULEB128 encoding. Used for ordinal numbers from 16+ */
  _LIEF_EI(BIND_OPCODE_SET_DYLIB_SPECIAL_IMM)            = 0x30u, /**< Set ordinal, with 0 or negative number as immediate. the value is sign extended. */
  _LIEF_EI(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM)    = 0x40u, /**< Set the following symbol (NULL-terminated char*). */
  _LIEF_EI(BIND_OPCODE_SET_TYPE_IMM)                     = 0x50u, /**< Set the type to immediate (lower 4-bits). See BIND_TYPES */
  _LIEF_EI(BIND_OPCODE_SET_ADDEND_SLEB)                  = 0x60u, /**< Set the addend field to the following SLEB128 encoding. */
  _LIEF_EI(BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB)      = 0x70u, /**< Set Segment to immediate value, and address to the following SLEB128 encoding */
  _LIEF_EI(BIND_OPCODE_ADD_ADDR_ULEB)                    = 0x80u, /**< Set the address field to the following SLEB128 encoding. */
  _LIEF_EI(BIND_OPCODE_DO_BIND)                          = 0x90u, /**< Perform binding of current table row */
  _LIEF_EI(BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB)            = 0xA0u, /**< Perform binding, also add following ULEB128 as address */
  _LIEF_EI(BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED)      = 0xB0u, /**< Perform binding, also add immediate (lower 4-bits) using scaling */
  _LIEF_EI(BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB) = 0xC0u, /**< Perform binding for several symbols (as following ULEB128), and skip several bytes. */
  _LIEF_EI(BIND_OPCODE_THREADED)                         = 0xD0u,

  _LIEF_EI(BIND_OPCODE_THREADED_APPLY)                            = 0xD0u | 0x01u,
  _LIEF_EI(BIND_OPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB) = 0xD0u | 0x00u,
};

enum _LIEF_EN_2(BIND_SUBOPCODE_THREADED, uint8_t) {
  _LIEF_EI(BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB) = 0x00u,
  _LIEF_EI(BIND_SUBOPCODE_THREADED_APPLY)                            = 0x01u,
};

enum _LIEF_EN(EXPORT_SYMBOL_FLAGS) {
  _LIEF_EI(EXPORT_SYMBOL_FLAGS_KIND_MASK)           = 0x03u, /**< Mask to access to EXPORT_SYMBOL_KINDS */
  _LIEF_EI(EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION)     = 0x04u,
  _LIEF_EI(EXPORT_SYMBOL_FLAGS_REEXPORT)            = 0x08u,
  _LIEF_EI(EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER)   = 0x10u
};

enum _LIEF_EN(EXPORT_SYMBOL_KINDS) {
  _LIEF_EI(EXPORT_SYMBOL_FLAGS_KIND_REGULAR)        = 0x00u,
  _LIEF_EI(EXPORT_SYMBOL_FLAGS_KIND_THREAD_LOCAL)   = 0x01u,
  _LIEF_EI(EXPORT_SYMBOL_FLAGS_KIND_ABSOLUTE)       = 0x02u
};

enum _LIEF_EN(MACHO_SYMBOL_TYPES) {
  _LIEF_EI(N_STAB) = 0xe0, /**< The symbol is symbolic debugging table */
  _LIEF_EI(N_PEXT) = 0x10, /**< Limited global scope symbol. */
  _LIEF_EI(N_TYPE) = 0x0e, /**< Define the type of the symbol. @see N_LIST_TYPES */
  _LIEF_EI(N_EXT)  = 0x01  /**< External symbol, a symbol that is either defined outside this file or that is defined in this file but can be referenced by other files. */
};

/** Constants for the "n_type & SYMBOL_TYPES::N_TYPE" in nlist. @see nlist */
enum _LIEF_EN(N_LIST_TYPES) {
  _LIEF_EI(N_UNDF) = 0x0u, /**< The symbol is undefined. It is referenced in a different module. */
  _LIEF_EI(N_ABS)  = 0x2u, /**< The symbol is absolute. The linker doesn't update his value. */
  _LIEF_EI(N_SECT) = 0xeu, /**< The symbol is defined in the section number given in nlist_base.n_sect . */
  _LIEF_EI(N_PBUD) = 0xcu, /**< The symbol is undefined and the image is using a prebound value for the symbol. Set the n_sect field to NO_SECT . */
  _LIEF_EI(N_INDR) = 0xau  /**< The symbol is defined to be the same as another symbol. The n_value field is an index into the string table specifying the name of the other symbol. When that symbol is linked, both this and the other symbol point to the same defined type and value. */
};

enum _LIEF_EN(SectionOrdinal) {
  /* Constants for the "n_sect" field in llvm::MachO::nlist and */
  /* llvm::MachO::nlist_64 */
  _LIEF_EI(NO_SECT)  = 0u,
  _LIEF_EI(MAX_SECT) = 0xffu
};

enum _LIEF_EN(SYMBOL_DESCRIPTIONS) {

  /* The low 3 bits are the for the REFERENCE_TYPE. */
  _LIEF_EI(REFERENCE_TYPE)                            = 0x7,
  _LIEF_EI(REFERENCE_FLAG_UNDEFINED_NON_LAZY)         = 0, /**< Reference to an external non-lazy (data) symbol. */
  _LIEF_EI(REFERENCE_FLAG_UNDEFINED_LAZY)             = 1, /**< Reference to an external lazy symbol - that is, to a function call. */
  _LIEF_EI(REFERENCE_FLAG_DEFINED)                    = 2, /**< Defined in this module. */
  _LIEF_EI(REFERENCE_FLAG_PRIVATE_DEFINED)            = 3, /**< Defined in this module and is visible only to modules within this shared library. */
  _LIEF_EI(REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY) = 4, /**< Defined in another module in this file, is a non-lazy (data) symbol, and is visible only to modules within this shared library. */
  _LIEF_EI(REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY)     = 5, /**< Defined in another module in this file, is a lazy (function) symbol, and is visible only to modules within this shared library. */

  /* Flag bits (some overlap with the library ordinal bits). */
  _LIEF_EI(N_ARM_THUMB_DEF)        = 0x0008u,
  _LIEF_EI(REFERENCED_DYNAMICALLY) = 0x0010u,
  _LIEF_EI(N_NO_DEAD_STRIP)        = 0x0020u,
  _LIEF_EI(N_WEAK_REF)             = 0x0040u,
  _LIEF_EI(N_WEAK_DEF)             = 0x0080u,
  _LIEF_EI(N_SYMBOL_RESOLVER)      = 0x0100u,
  _LIEF_EI(N_ALT_ENTRY)            = 0x0200u,

  /* For undefined symbols coming from libraries, see GET_LIBRARY_ORDINAL() */
  /* as these are in the top 8 bits. */
  _LIEF_EI(SELF_LIBRARY_ORDINAL)   = 0x0,
  _LIEF_EI(MAX_LIBRARY_ORDINAL)    = 0xfd,
  _LIEF_EI(DYNAMIC_LOOKUP_ORDINAL) = 0xfe,
  _LIEF_EI(EXECUTABLE_ORDINAL)     = 0xff
};

enum _LIEF_EN(StabType) {
  /* Constant values for the "n_type" field in llvm::MachO::nlist and */
  /* llvm::MachO::nlist_64 when "(n_type & N_STAB) != 0" */
  _LIEF_EI(N_GSYM)    = 0x20u,
  _LIEF_EI(N_FNAME)   = 0x22u,
  _LIEF_EI(N_FUN)     = 0x24u,
  _LIEF_EI(N_STSYM)   = 0x26u,
  _LIEF_EI(N_LCSYM)   = 0x28u,
  _LIEF_EI(N_BNSYM)   = 0x2Eu,
  _LIEF_EI(N_PC)      = 0x30u,
  _LIEF_EI(N_AST)     = 0x32u,
  _LIEF_EI(N_OPT)     = 0x3Cu,
  _LIEF_EI(N_RSYM)    = 0x40u,
  _LIEF_EI(N_SLINE)   = 0x44u,
  _LIEF_EI(N_ENSYM)   = 0x4Eu,
  _LIEF_EI(N_SSYM)    = 0x60u,
  _LIEF_EI(N_SO)      = 0x64u,
  _LIEF_EI(N_OSO)     = 0x66u,
  _LIEF_EI(N_LSYM)    = 0x80u,
  _LIEF_EI(N_BINCL)   = 0x82u,
  _LIEF_EI(N_SOL)     = 0x84u,
  _LIEF_EI(N_PARAMS)  = 0x86u,
  _LIEF_EI(N_VERSION) = 0x88u,
  _LIEF_EI(N_OLEVEL)  = 0x8Au,
  _LIEF_EI(N_PSYM)    = 0xA0u,
  _LIEF_EI(N_EINCL)   = 0xA2u,
  _LIEF_EI(N_ENTRY)   = 0xA4u,
  _LIEF_EI(N_LBRAC)   = 0xC0u,
  _LIEF_EI(N_EXCL)    = 0xC2u,
  _LIEF_EI(N_RBRAC)   = 0xE0u,
  _LIEF_EI(N_BCOMM)   = 0xE2u,
  _LIEF_EI(N_ECOMM)   = 0xE4u,
  _LIEF_EI(N_ECOML)   = 0xE8u,
  _LIEF_EI(N_LENG)    = 0xFEu
};


enum {
  /* Constant values for the r_symbolnum field in an */
  /* llvm::MachO::relocation_info structure when r_extern is 0. */
  _LIEF_EI(R_ABS) = 0,

  /* Constant bits for the r_address field in an */
  /* llvm::MachO::relocation_info structure. */
  _LIEF_EI(R_SCATTERED) = 0x80000000
};

enum _LIEF_EN(RELOCATION_ORIGINS) {
  _LIEF_EI(ORIGIN_UNKNOWN)     = 0,
  _LIEF_EI(ORIGIN_DYLDINFO)    = 1,
  _LIEF_EI(ORIGIN_RELOC_TABLE) = 2,
};


enum _LIEF_EN(SYMBOL_ORIGINS) {
  _LIEF_EI(SYM_ORIGIN_UNKNOWN)     = 0,
  _LIEF_EI(SYM_ORIGIN_DYLD_EXPORT) = 1,
  _LIEF_EI(SYM_ORIGIN_DYLD_BIND)   = 2, /**< The symbol comes from the binding opcodes */
  _LIEF_EI(SYM_ORIGIN_LC_SYMTAB)   = 3,
};

enum _LIEF_EN(X86_RELOCATION) {
  _LIEF_EI(GENERIC_RELOC_VANILLA)        = 0, /**< A generic relocation entry for both addresses contained in data and addresses contained in CPU instructions. */
  _LIEF_EI(GENERIC_RELOC_PAIR)           = 1, /**< The second relocation entry of a pair. */
  _LIEF_EI(GENERIC_RELOC_SECTDIFF)       = 2, /**< A relocation entry for an item that contains the difference of two section addresses. This is generally used for position-independent code generation. */
  _LIEF_EI(GENERIC_RELOC_PB_LA_PTR)      = 3, /**< contains the address from which to subtract; it must be followed by a X86_RELOCATION::GENERIC_RELOC_PAIR containing the address to subtract.*/
  _LIEF_EI(GENERIC_RELOC_LOCAL_SECTDIFF) = 4, /**< Similar to X86_RELOCATION::GENERIC_RELOC_SECTDIFF except that this entry refers specifically to the address in this item. If the address is that of a globally visible coalesced symbol, this relocation entry does not change if the symbol is overridden. This is used to associate stack unwinding information with the object code this relocation entry describes.*/
  _LIEF_EI(GENERIC_RELOC_TLV)            = 5, /**< A relocation entry for a prebound lazy pointer. This is always a scattered relocation entry. The MachO::Relocation::value field contains the non-prebound value of the lazy pointer.*/
};


enum _LIEF_EN(X86_64_RELOCATION) {
  _LIEF_EI(X86_64_RELOC_UNSIGNED)        = 0, /**< A CALL/JMP instruction with 32-bit displacement. */
  _LIEF_EI(X86_64_RELOC_SIGNED)          = 1, /**< A MOVQ load of a GOT entry. */
  _LIEF_EI(X86_64_RELOC_BRANCH)          = 2, /**< Other GOT references. */
  _LIEF_EI(X86_64_RELOC_GOT_LOAD)        = 3, /**< Signed 32-bit displacement. */
  _LIEF_EI(X86_64_RELOC_GOT)             = 4, /**< Absolute address. */
  _LIEF_EI(X86_64_RELOC_SUBTRACTOR)      = 5, /**< Must be followed by a X86_64_RELOCATION::X86_64_RELOC_UNSIGNED relocation. */
  _LIEF_EI(X86_64_RELOC_SIGNED_1)        = 6, /**< */
  _LIEF_EI(X86_64_RELOC_SIGNED_2)        = 7, /**< */
  _LIEF_EI(X86_64_RELOC_SIGNED_4)        = 8, /**< */
  _LIEF_EI(X86_64_RELOC_TLV)             = 9, /**< */
};


enum _LIEF_EN(PPC_RELOCATION) {
  _LIEF_EI(PPC_RELOC_VANILLA)            = 0,
  _LIEF_EI(PPC_RELOC_PAIR)               = 1,
  _LIEF_EI(PPC_RELOC_BR14)               = 2,
  _LIEF_EI(PPC_RELOC_BR24)               = 3,
  _LIEF_EI(PPC_RELOC_HI16)               = 4,
  _LIEF_EI(PPC_RELOC_LO16)               = 5,
  _LIEF_EI(PPC_RELOC_HA16)               = 6,
  _LIEF_EI(PPC_RELOC_LO14)               = 7,
  _LIEF_EI(PPC_RELOC_SECTDIFF)           = 8,
  _LIEF_EI(PPC_RELOC_PB_LA_PTR)          = 9,
  _LIEF_EI(PPC_RELOC_HI16_SECTDIFF)      = 10,
  _LIEF_EI(PPC_RELOC_LO16_SECTDIFF)      = 11,
  _LIEF_EI(PPC_RELOC_HA16_SECTDIFF)      = 12,
  _LIEF_EI(PPC_RELOC_JBSR)               = 13,
  _LIEF_EI(PPC_RELOC_LO14_SECTDIFF)      = 14,
  _LIEF_EI(PPC_RELOC_LOCAL_SECTDIFF)     = 15,
};


enum _LIEF_EN(ARM_RELOCATION) {
  _LIEF_EI(ARM_RELOC_VANILLA)            = 0,
  _LIEF_EI(ARM_RELOC_PAIR)               = 1,
  _LIEF_EI(ARM_RELOC_SECTDIFF)           = 2,
  _LIEF_EI(ARM_RELOC_LOCAL_SECTDIFF)     = 3,
  _LIEF_EI(ARM_RELOC_PB_LA_PTR)          = 4,
  _LIEF_EI(ARM_RELOC_BR24)               = 5,
  _LIEF_EI(ARM_THUMB_RELOC_BR22)         = 6,
  _LIEF_EI(ARM_THUMB_32BIT_BRANCH)       = 7, /* obsolete */
  _LIEF_EI(ARM_RELOC_HALF)               = 8,
  _LIEF_EI(ARM_RELOC_HALF_SECTDIFF)      = 9,
};


enum _LIEF_EN(ARM64_RELOCATION) {
  _LIEF_EI(ARM64_RELOC_UNSIGNED)            = 0,  /**< For pointers. */
  _LIEF_EI(ARM64_RELOC_SUBTRACTOR)          = 1,  /**< Must be followed by an ARM64_RELOCATION::ARM64_RELOC_UNSIGNED */
  _LIEF_EI(ARM64_RELOC_BRANCH26)            = 2,  /**< A B/BL instruction with 26-bit displacement. */
  _LIEF_EI(ARM64_RELOC_PAGE21)              = 3,  /**< PC-rel distance to page of target. */
  _LIEF_EI(ARM64_RELOC_PAGEOFF12)           = 4,  /**< Offset within page, scaled by MachO::Relocation::size. */
  _LIEF_EI(ARM64_RELOC_GOT_LOAD_PAGE21)     = 5,  /**< PC-rel distance to page of GOT slot */
  _LIEF_EI(ARM64_RELOC_GOT_LOAD_PAGEOFF12)  = 6,  /**< Offset within page of GOT slot, scaled by MachO::Relocation::size. */
  _LIEF_EI(ARM64_RELOC_POINTER_TO_GOT)      = 7,  /**< For pointers to GOT slots. */
  _LIEF_EI(ARM64_RELOC_TLVP_LOAD_PAGE21)    = 8,  /**< PC-rel distance to page of TLVP slot. */
  _LIEF_EI(ARM64_RELOC_TLVP_LOAD_PAGEOFF12) = 9,  /**< Offset within page of TLVP slot, scaled by MachO::Relocation::size.*/
  _LIEF_EI(ARM64_RELOC_ADDEND)              = 10, /**< Must be followed by ARM64_RELOCATION::ARM64_RELOC_PAGE21 or ARM64_RELOCATION::ARM64_RELOC_PAGEOFF12. */
};

/** Values for segment_command.initprot (From <mach/vm_prot.h>). */
enum _LIEF_EN(VM_PROTECTIONS) {
  _LIEF_EI(VM_PROT_READ)    = 0x1, /**< Reading data within the segment is allowed */
  _LIEF_EI(VM_PROT_WRITE)   = 0x2, /**< Writing data within the segment is allowed */
  _LIEF_EI(VM_PROT_EXECUTE) = 0x4, /**< Executing data within the segment is allowed */
};


/* Enums from <mach/machine.h> */
enum {
  /* Capability bits used in the definition of cpu_type. */
  _LIEF_EI(CPU_ARCH_MASK)  = 0xff000000,   /* Mask for architecture bits */
  _LIEF_EI(CPU_ARCH_ABI64) = 0x01000000    /* 64 bit ABI */
};

/* Constants for the cputype field. */
enum _LIEF_EN_2(CPU_TYPES, int) {
  _LIEF_EI(CPU_TYPE_ANY)       = -1,
  _LIEF_EI(CPU_TYPE_X86)       = 7,
  _LIEF_EI(CPU_TYPE_I386)      = 7,              /* CPU_TYPE_X86, */
  _LIEF_EI(CPU_TYPE_X86_64)    = 7 | 0x01000000, /* CPU_TYPE_X86 | CPU_ARCH_ABI64, */
/* _LIEF_EI(CPU_TYPE_MIPS)      = 8, */
  _LIEF_EI(CPU_TYPE_MC98000)   = 10, /* Old Motorola PowerPC */
  _LIEF_EI(CPU_TYPE_ARM)       = 12,
  _LIEF_EI(CPU_TYPE_ARM64)     = 12 | 0x01000000, /* CPU_TYPE_ARM | CPU_ARCH_ABI64, */
  _LIEF_EI(CPU_TYPE_SPARC)     = 14,
  _LIEF_EI(CPU_TYPE_POWERPC)   = 18,
  _LIEF_EI(CPU_TYPE_POWERPC64) = 18 | 0x01000000, /* CPU_TYPE_POWERPC | CPU_ARCH_ABI64 */
};

enum {
  /* Capability bits used in the definition of cpusubtype. */
  _LIEF_EI(CPU_SUBTYPE_MASK)  = 0xff000000,   /* Mask for architecture bits */
  _LIEF_EI(CPU_SUBTYPE_LIB64) = 0x80000000,   /* 64 bit libraries */

  /* Special CPU subtype constants. */
  _LIEF_EI(CPU_SUBTYPE_MULTIPLE) = ~0u
};

/* Constants for the cpusubtype field. */
enum _LIEF_EN(CPU_SUBTYPES_X86) {
  _LIEF_EI(CPU_SUBTYPE_I386_ALL)       = 3,
  _LIEF_EI(CPU_SUBTYPE_386)            = 3,
  _LIEF_EI(CPU_SUBTYPE_486)            = 4,
  _LIEF_EI(CPU_SUBTYPE_486SX)          = 0x84,
  _LIEF_EI(CPU_SUBTYPE_586)            = 5,
  _LIEF_EI(CPU_SUBTYPE_PENT)           = 5, /* CPU_SUBTYPE_586, */
  _LIEF_EI(CPU_SUBTYPE_PENTPRO)        = 0x16,
  _LIEF_EI(CPU_SUBTYPE_PENTII_M3)      = 0x36,
  _LIEF_EI(CPU_SUBTYPE_PENTII_M5)      = 0x56,
  _LIEF_EI(CPU_SUBTYPE_CELERON)        = 0x67,
  _LIEF_EI(CPU_SUBTYPE_CELERON_MOBILE) = 0x77,
  _LIEF_EI(CPU_SUBTYPE_PENTIUM_3)      = 0x08,
  _LIEF_EI(CPU_SUBTYPE_PENTIUM_3_M)    = 0x18,
  _LIEF_EI(CPU_SUBTYPE_PENTIUM_3_XEON) = 0x28,
  _LIEF_EI(CPU_SUBTYPE_PENTIUM_M)      = 0x09,
  _LIEF_EI(CPU_SUBTYPE_PENTIUM_4)      = 0x0a,
  _LIEF_EI(CPU_SUBTYPE_PENTIUM_4_M)    = 0x1a,
  _LIEF_EI(CPU_SUBTYPE_ITANIUM)        = 0x0b,
  _LIEF_EI(CPU_SUBTYPE_ITANIUM_2)      = 0x1b,
  _LIEF_EI(CPU_SUBTYPE_XEON)           = 0x0c,
  _LIEF_EI(CPU_SUBTYPE_XEON_MP)        = 0x1c,

  _LIEF_EI(CPU_SUBTYPE_X86_ALL)     = 3,
  _LIEF_EI(CPU_SUBTYPE_X86_64_ALL)  = 3,
  _LIEF_EI(CPU_SUBTYPE_X86_ARCH1)   = 4,
  _LIEF_EI(CPU_SUBTYPE_X86_64_H)    = 8
};


enum {
  _LIEF_EI(CPU_SUBTYPE_INTEL_FAMILY_MAX) = 15,
  _LIEF_EI(CPU_SUBTYPE_INTEL_MODEL_ALL)  = 0
};

enum _LIEF_EN(CPU_SUBTYPES_ARM) {
  _LIEF_EI(CPU_SUBTYPE_ARM_ALL)     = 0,
  _LIEF_EI(CPU_SUBTYPE_ARM_V4T)     = 5,
  _LIEF_EI(CPU_SUBTYPE_ARM_V6)      = 6,
  _LIEF_EI(CPU_SUBTYPE_ARM_V5)      = 7,
  _LIEF_EI(CPU_SUBTYPE_ARM_V5TEJ)   = 7,
  _LIEF_EI(CPU_SUBTYPE_ARM_XSCALE)  = 8,
  _LIEF_EI(CPU_SUBTYPE_ARM_V7)      = 9,
  /*  unused  _LIEF_EI(ARM_V7F)     = 10, */
  _LIEF_EI(CPU_SUBTYPE_ARM_V7S)     = 11,
  _LIEF_EI(CPU_SUBTYPE_ARM_V7K)     = 12,
  _LIEF_EI(CPU_SUBTYPE_ARM_V6M)     = 14,
  _LIEF_EI(CPU_SUBTYPE_ARM_V7M)     = 15,
  _LIEF_EI(CPU_SUBTYPE_ARM_V7EM)    = 16
};

enum _LIEF_EN(CPU_SUBTYPES_ARM64) {
  _LIEF_EI(CPU_SUBTYPE_ARM64_ALL)   = 0
};

enum _LIEF_EN(CPU_SUBTYPES_SPARC) {
  _LIEF_EI(CPU_SUBTYPE_SPARC_ALL)   = 0
};

enum _LIEF_EN(CPU_SUBTYPES_POWERPC) {
  _LIEF_EI(CPU_SUBTYPE_POWERPC_ALL)   = 0,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_601)   = 1,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_602)   = 2,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_603)   = 3,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_603e)  = 4,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_603ev) = 5,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_604)   = 6,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_604e)  = 7,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_620)   = 8,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_750)   = 9,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_7400)  = 10,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_7450)  = 11,
  _LIEF_EI(CPU_SUBTYPE_POWERPC_970)   = 100,

  _LIEF_EI(CPU_SUBTYPE_MC980000_ALL)  = 0, /* CPU_SUBTYPE_POWERPC_ALL, */
  _LIEF_EI(CPU_SUBTYPE_MC98601)       = 1, /* CPU_SUBTYPE_POWERPC_601 */
};


