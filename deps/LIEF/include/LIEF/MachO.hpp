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
#ifndef LIEF_MACHO_H
#define LIEF_MACHO_H

#include "LIEF/config.h"

#if defined(LIEF_MACHO_SUPPORT)

#include "LIEF/MachO/AtomInfo.hpp"
#include "LIEF/MachO/Binary.hpp"
#include "LIEF/MachO/BinaryParser.hpp"
#include "LIEF/MachO/BindingInfo.hpp"
#include "LIEF/MachO/BindingInfoIterator.hpp"
#include "LIEF/MachO/BuildToolVersion.hpp"
#include "LIEF/MachO/BuildVersion.hpp"
#include "LIEF/MachO/Builder.hpp"
#include "LIEF/MachO/ChainedBindingInfo.hpp"
#include "LIEF/MachO/CodeSignature.hpp"
#include "LIEF/MachO/CodeSignatureDir.hpp"
#include "LIEF/MachO/DataCodeEntry.hpp"
#include "LIEF/MachO/DataInCode.hpp"
#include "LIEF/MachO/DyldBindingInfo.hpp"
#include "LIEF/MachO/DyldChainedFixups.hpp"
#include "LIEF/MachO/DyldChainedFixupsCreator.hpp"
#include "LIEF/MachO/DyldEnvironment.hpp"
#include "LIEF/MachO/DyldExportsTrie.hpp"
#include "LIEF/MachO/DyldInfo.hpp"
#include "LIEF/MachO/DylibCommand.hpp"
#include "LIEF/MachO/DylinkerCommand.hpp"
#include "LIEF/MachO/DynamicSymbolCommand.hpp"
#include "LIEF/MachO/EncryptionInfo.hpp"
#include "LIEF/MachO/EnumToString.hpp"
#include "LIEF/MachO/ExportInfo.hpp"
#include "LIEF/MachO/FatBinary.hpp"
#include "LIEF/MachO/FilesetCommand.hpp"
#include "LIEF/MachO/FunctionStarts.hpp"
#include "LIEF/MachO/FunctionVariants.hpp"
#include "LIEF/MachO/FunctionVariantFixups.hpp"
#include "LIEF/MachO/Header.hpp"
#include "LIEF/MachO/IndirectBindingInfo.hpp"
#include "LIEF/MachO/LinkEdit.hpp"
#include "LIEF/MachO/LinkerOptHint.hpp"
#include "LIEF/MachO/LoadCommand.hpp"
#include "LIEF/MachO/MainCommand.hpp"
#include "LIEF/MachO/NoteCommand.hpp"
#include "LIEF/MachO/Parser.hpp"
#include "LIEF/MachO/ParserConfig.hpp"
#include "LIEF/MachO/RPathCommand.hpp"
#include "LIEF/MachO/Relocation.hpp"
#include "LIEF/MachO/RelocationDyld.hpp"
#include "LIEF/MachO/RelocationFixup.hpp"
#include "LIEF/MachO/RelocationObject.hpp"
#include "LIEF/MachO/Routine.hpp"
#include "LIEF/MachO/Section.hpp"
#include "LIEF/MachO/SegmentCommand.hpp"
#include "LIEF/MachO/SegmentSplitInfo.hpp"
#include "LIEF/MachO/SourceVersion.hpp"
#include "LIEF/MachO/Stub.hpp"
#include "LIEF/MachO/SubClient.hpp"
#include "LIEF/MachO/SubFramework.hpp"
#include "LIEF/MachO/Symbol.hpp"
#include "LIEF/MachO/SymbolCommand.hpp"
#include "LIEF/MachO/ThreadCommand.hpp"
#include "LIEF/MachO/TwoLevelHints.hpp"
#include "LIEF/MachO/UUIDCommand.hpp"
#include "LIEF/MachO/UnknownCommand.hpp"
#include "LIEF/MachO/VersionMin.hpp"
#include "LIEF/MachO/enums.hpp"
#include "LIEF/MachO/hash.hpp"
#include "LIEF/MachO/json.hpp"
#include "LIEF/MachO/type_traits.hpp"
#include "LIEF/MachO/utils.hpp"

#include "LIEF/ObjC/Metadata.hpp"

#endif
#endif
