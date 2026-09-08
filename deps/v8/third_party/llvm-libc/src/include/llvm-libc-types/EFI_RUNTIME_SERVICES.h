//===-- Definition of EFI_RUNTIME_SERVICES type ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_RUNTIME_SERVICES_H
#define LLVM_LIBC_TYPES_EFI_RUNTIME_SERVICES_H

#include "../llvm-libc-macros/EFIAPI-macros.h"
#include "../llvm-libc-macros/stdint-macros.h"
#include "EFI_CAPSULE.h"
#include "EFI_MEMORY_DESCRIPTOR.h"
#include "EFI_PHYSICAL_ADDRESS.h"
#include "EFI_STATUS.h"
#include "EFI_TABLE_HEADER.h"
#include "EFI_TIME.h"
#include "char16_t.h"
#include "size_t.h"

#define EFI_RUNTIME_SERVICES_SIGNATURE 0x56524553544e5552
#define EFI_RUNTIME_SERVICES_REVISION EFI_SPECIFICATION_VERSION

#define EFI_VARIABLE_NON_VOLATILE 0x00000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS 0x00000002
#define EFI_VARIABLE_RUNTIME_ACCESS 0x00000004
#define EFI_VARIABLE_HARDWARE_ERROR_RECORD 0x00000008
// This attribute is identified by the mnemonic 'HR' elsewhere
// in this specification.
#define EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS 0x00000010
// NOTE: EFI_VARIABLE_AUTHENTICATED_WRITE_ACCESS is deprecated
// and should be considered reserved.
#define EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS 0x00000020
#define EFI_VARIABLE_APPEND_WRITE 0x00000040
#define EFI_VARIABLE_ENHANCED_AUTHENTICATED_ACCESS 0x00000080

typedef enum {
  EfiResetCold,
  EfiResetWarm,
  EfiResetShutdown,
  EfiResetPlatformSpecific,
} EFI_RESET_TYPE;

#define EFI_VARIABLE_AUTHENTICATION_3_CERT_ID_SHA256 1

typedef struct {
  uint8_t Type;
  uint32_t IdSize;
  // Value is defined as:
  // uint8_t Id[IdSize];
} EFI_VARIABLE_AUTHENTICATION_3_CERT_ID;

typedef EFI_STATUS(EFIAPI *EFI_GET_TIME)(EFI_TIME *Time,
                                         EFI_TIME_CAPABILITIES *Capabilities);
typedef EFI_STATUS(EFIAPI *EFI_SET_TIME)(EFI_TIME *Time);
typedef EFI_STATUS(EFIAPI *EFI_GET_WAKEUP_TIME)(bool *Enabled, bool *Pending,
                                                EFI_TIME *Time);
typedef EFI_STATUS(EFIAPI *EFI_SET_WAKEUP_TIME)(bool *Enabled, EFI_TIME *Time);

typedef EFI_STATUS(EFIAPI *EFI_SET_VIRTUAL_ADDRESS_MAP)(
    size_t MemoryMapSize, size_t DescriptorSize, uint32_t DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR *VirtualMap);
typedef EFI_STATUS(EFIAPI *EFI_CONVERT_POINTER)(size_t DebugDisposition,
                                                void **Address);

typedef EFI_STATUS(EFIAPI *EFI_GET_VARIABLE)(char16_t *VariableName,
                                             EFI_GUID *VendorGuid,
                                             uint32_t *Attributes,
                                             size_t *DataSize, void *Data);
typedef EFI_STATUS(EFIAPI *EFI_GET_NEXT_VARIABLE_NAME)(size_t *VariableNameSize,
                                                       char16_t *VariableName,
                                                       EFI_GUID *VendorGuid);
typedef EFI_STATUS(EFIAPI *EFI_SET_VARIABLE)(char16_t *VariableName,
                                             EFI_GUID *VendorGuid,
                                             uint32_t Attributes,
                                             size_t DataSize, void *Data);

typedef EFI_STATUS(EFIAPI *EFI_GET_NEXT_HIGH_MONO_COUNT)(uint32_t *HighCount);
typedef void(EFIAPI *EFI_RESET_SYSTEM)(EFI_RESET_TYPE ResetType,
                                       EFI_STATUS ResetStatus, size_t DataSize,
                                       void *ResetData);

typedef EFI_STATUS(EFIAPI *EFI_UPDATE_CAPSULE)(
    EFI_CAPSULE_HEADER **CapsuleHeaderArray, size_t CapsuleCount,
    EFI_PHYSICAL_ADDRESS ScatterGatherList);
typedef EFI_STATUS(EFIAPI *EFI_QUERY_CAPSULE_CAPABILITIES)(
    EFI_CAPSULE_HEADER **CapsuleHeaderArray, size_t CapsuleCount,
    uint64_t *MaximumCapsuleSize, EFI_RESET_TYPE ResetType);

typedef EFI_STATUS(EFIAPI *EFI_QUERY_VARIABLE_INFO)(
    uint32_t Attributes, uint64_t *MaximumVariableStorageSize,
    uint64_t *RemainingVariableStorageSize, uint64_t *MaximumVariableSize);

typedef struct {
  EFI_TABLE_HEADER Hdr;

  ///
  /// Time Services
  EFI_GET_TIME GetTime;
  EFI_SET_TIME SetTime;
  EFI_GET_WAKEUP_TIME GetWakeupTime;
  EFI_SET_WAKEUP_TIME SetWakeupTime;

  //
  // Virtual Memory Services
  //
  EFI_SET_VIRTUAL_ADDRESS_MAP SetVirtualAddressMap;
  EFI_CONVERT_POINTER ConvertPointer;

  //
  // Variable Services
  //
  EFI_GET_VARIABLE GetVariable;
  EFI_GET_NEXT_VARIABLE_NAME GetNextVariableName;
  EFI_SET_VARIABLE SetVariable;

  //
  // Miscellaneous Services
  //
  EFI_GET_NEXT_HIGH_MONO_COUNT GetNextHighMonotonicCount;
  EFI_RESET_SYSTEM ResetSystem;

  //
  // UEFI 2.0 Capsule Services
  //
  EFI_UPDATE_CAPSULE UpdateCapsule;
  EFI_QUERY_CAPSULE_CAPABILITIES QueryCapsuleCapabilities;

  //
  // Miscellaneous UEFI 2.0 Service
  //
  EFI_QUERY_VARIABLE_INFO QueryVariableInfo;
} EFI_RUNTIME_SERVICES;

#endif // LLVM_LIBC_TYPES_EFI_RUNTIME_SERVICES_H
