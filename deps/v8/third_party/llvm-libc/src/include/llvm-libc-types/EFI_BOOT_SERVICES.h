//===-- Definition of EFI_BOOT_SERVICES type ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_EFI_BOOT_SERVICES_H
#define LLVM_LIBC_TYPES_EFI_BOOT_SERVICES_H

#include "../llvm-libc-macros/EFIAPI-macros.h"
#include "EFI_ALLOCATE_TYPE.h"
#include "EFI_DEVICE_PATH_PROTOCOL.h"
#include "EFI_EVENT.h"
#include "EFI_GUID.h"
#include "EFI_INTERFACE_TYPE.h"
#include "EFI_LOCATE_SEARCH_TYPE.h"
#include "EFI_MEMORY_DESCRIPTOR.h"
#include "EFI_MEMORY_TYPE.h"
#include "EFI_OPEN_PROTOCOL_INFORMATION_ENTRY.h"
#include "EFI_PHYSICAL_ADDRESS.h"
#include "EFI_STATUS.h"
#include "EFI_TABLE_HEADER.h"
#include "EFI_TIMER_DELAY.h"
#include "EFI_TPL.h"
#include "char16_t.h"
#include "size_t.h"

#define EFI_BOOT_SERVICES_SIGNATURE 0x56524553544f4f42
#define EFI_BOOT_SERVICES_REVISION EFI_SPECIFICATION_VERSION

typedef EFI_TPL(EFIAPI *EFI_RAISE_TPL)(EFI_TPL NewTpl);
typedef void(EFIAPI *EFI_RESTORE_TPL)(EFI_TPL OldTpl);

typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_PAGES)(EFI_ALLOCATE_TYPE Type,
                                               EFI_MEMORY_TYPE MemoryType,
                                               size_t Pages,
                                               EFI_PHYSICAL_ADDRESS *Memory);
typedef EFI_STATUS(EFIAPI *EFI_FREE_PAGES)(EFI_PHYSICAL_ADDRESS Memory,
                                           size_t Pages);
typedef EFI_STATUS(EFIAPI *EFI_GET_MEMORY_MAP)(size_t *MemoryMapSize,
                                               EFI_MEMORY_DESCRIPTOR *MemoryMap,
                                               size_t *MapKey,
                                               size_t *DescriptorSize,
                                               uint32_t *DescriptorVersion);

typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_POOL)(EFI_MEMORY_TYPE PoolType,
                                              size_t Size, void **Buffer);
typedef EFI_STATUS(EFIAPI *EFI_FREE_POOL)(void *Buffer);

typedef void(EFIAPI *EFI_EVENT_NOTIFY)(EFI_EVENT Event, void *Context);

typedef EFI_STATUS(EFIAPI *EFI_CREATE_EVENT)(uint32_t Type, EFI_TPL NotifyTpl,
                                             EFI_EVENT_NOTIFY NotifyFunction,
                                             void *NotifyContext,
                                             EFI_EVENT *Event);
typedef EFI_STATUS(EFIAPI *EFI_SET_TIMER)(EFI_EVENT Event, EFI_TIMER_DELAY Type,
                                          uint64_t TriggerTime);
typedef EFI_STATUS(EFIAPI *EFI_WAIT_FOR_EVENT)(size_t NumberOfEvents,
                                               EFI_EVENT *Event, size_t *Index);
typedef EFI_STATUS(EFIAPI *EFI_SIGNAL_EVENT)(EFI_EVENT Event);
typedef EFI_STATUS(EFIAPI *EFI_CLOSE_EVENT)(EFI_EVENT Event);
typedef EFI_STATUS(EFIAPI *EFI_CHECK_EVENT)(EFI_EVENT Event);

typedef EFI_STATUS(EFIAPI *EFI_INSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE *Handle, EFI_GUID *Protocol, EFI_INTERFACE_TYPE InterfaceType,
    void *Interface);
typedef EFI_STATUS(EFIAPI *EFI_REINSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE Handle, EFI_GUID *Protocol, void *OldInterface,
    void *NewInterface);
typedef EFI_STATUS(EFIAPI *EFI_UNINSTALL_PROTOCOL_INTERFACE)(EFI_HANDLE Handle,
                                                             EFI_GUID *Protocol,
                                                             void *Interface);

typedef EFI_STATUS(EFIAPI *EFI_HANDLE_PROTOCOL)(EFI_HANDLE Handle,
                                                EFI_GUID *Protocol,
                                                void **Interface);
typedef EFI_STATUS(EFIAPI *EFI_REGISTER_PROTOCOL_NOTIFY)(EFI_GUID *Protocol,
                                                         EFI_EVENT Event,
                                                         void **Registration);

typedef EFI_STATUS(EFIAPI *EFI_LOCATE_HANDLE)(EFI_LOCATE_SEARCH_TYPE SearchType,
                                              EFI_GUID *Protocol,
                                              void *SearchKey,
                                              size_t *BufferSize,
                                              EFI_HANDLE *Buffer);
typedef EFI_STATUS(EFIAPI *EFI_LOCATE_DEVICE_PATH)(
    EFI_GUID *Protocol, EFI_DEVICE_PATH_PROTOCOL **DevicePath,
    EFI_HANDLE *Device);

typedef EFI_STATUS(EFIAPI *EFI_INSTALL_CONFIGURATION_TABLE)(EFI_GUID *Guid,
                                                            void *Table);
typedef EFI_STATUS(EFIAPI *EFI_IMAGE_UNLOAD)(EFI_HANDLE ImageHandle);
typedef EFI_STATUS(EFIAPI *EFI_IMAGE_START)(EFI_HANDLE ImageHandle,
                                            size_t *ExitDataSize,
                                            char16_t **ExitData);

typedef EFI_STATUS(EFIAPI *EFI_EXIT)(EFI_HANDLE ImageHandle,
                                     EFI_STATUS ExitStatus, size_t ExitDataSize,
                                     char16_t *ExitData);
typedef EFI_STATUS(EFIAPI *EFI_EXIT_BOOT_SERVICES)(EFI_HANDLE ImageHandle,
                                                   size_t MapKey);
typedef EFI_STATUS(EFIAPI *EFI_GET_NEXT_MONOTONIC_COUNT)(uint64_t *Count);
typedef EFI_STATUS(EFIAPI *EFI_STALL)(size_t Microseconds);
typedef EFI_STATUS(EFIAPI *EFI_SET_WATCHDOG_TIMER)(size_t Timeout,
                                                   uint64_t WatchdogCode,
                                                   size_t DataSize,
                                                   char16_t *WatchdogData);

typedef EFI_STATUS(EFIAPI *EFI_CONNECT_CONTROLLER)(
    EFI_HANDLE ControllerHandle, EFI_HANDLE *DriverImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath, bool Recursive);

typedef EFI_STATUS(EFIAPI *EFI_DISCONNECT_CONTROLLER)(
    EFI_HANDLE ControllerHandle, EFI_HANDLE DriverImageHandle,
    EFI_HANDLE ChildHandle);

typedef EFI_STATUS(EFIAPI *EFI_OPEN_PROTOCOL)(
    EFI_HANDLE Handle, EFI_GUID *Protocol, void **Interface,
    EFI_HANDLE AgentHandle, EFI_HANDLE ControllerHandle, uint32_t Attributes);

typedef EFI_STATUS(EFIAPI *EFI_CLOSE_PROTOCOL)(EFI_HANDLE Handle,
                                               EFI_GUID *Protocol,
                                               EFI_HANDLE AgentHandle,
                                               EFI_HANDLE ControllerHandle);

typedef EFI_STATUS(EFIAPI *EFI_OPEN_PROTOCOL_INFORMATION)(
    EFI_HANDLE Handle, EFI_GUID *Protocol,
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer, size_t *EntryCount);

typedef EFI_STATUS(EFIAPI *EFI_PROTOCOLS_PER_HANDLE)(
    EFI_HANDLE Handle, EFI_GUID ***ProtocolBuffer, size_t *ProtocolBufferCount);

typedef EFI_STATUS(EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(
    EFI_LOCATE_SEARCH_TYPE SearchType, EFI_GUID *Protocol, void *SearchKey,
    size_t *NoHandles, EFI_HANDLE **Buffer);

typedef EFI_STATUS(EFIAPI *EFI_LOCATE_PROTOCOL)(EFI_GUID *Protocol,
                                                void *Registration,
                                                void **Interface);

typedef EFI_STATUS(EFIAPI *EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)(
    EFI_HANDLE Handle, ...);
typedef EFI_STATUS(EFIAPI *EFI_CALCULATE_CRC32)(void *Data, size_t DataSize,
                                                uint32_t *Crc32);

typedef void(EFIAPI *EFI_COPY_MEM)(void *Destination, void *Source,
                                   size_t Length);
typedef void(EFIAPI *EFI_SET_MEM)(void *Buffer, size_t Size, uint8_t Value);

typedef EFI_STATUS(EFIAPI *EFI_CREATE_EVENT_EX)(
    uint32_t Type, EFI_TPL NotifyTpl, EFI_EVENT_NOTIFY NotifyFunction,
    const void *NotifyContext, const EFI_GUID *EventGroup, EFI_EVENT *Event);

typedef struct {
  EFI_TABLE_HEADER Hdr;

  //
  // Task Priority Services
  //
  EFI_RAISE_TPL RaiseTPL;     // EFI 1.0+
  EFI_RESTORE_TPL RestoreTPL; // EFI 1.0+

  //
  // Memory Services
  //
  EFI_ALLOCATE_PAGES AllocatePages; // EFI 1.0+
  EFI_FREE_PAGES FreePages;         // EFI 1.0+
  EFI_GET_MEMORY_MAP GetMemoryMap;  // EFI 1.0+
  EFI_ALLOCATE_POOL AllocatePool;   // EFI 1.0+
  EFI_FREE_POOL FreePool;           // EFI 1.0+

  //
  // Event & Timer Services
  //
  EFI_CREATE_EVENT CreateEvent;    // EFI 1.0+
  EFI_SET_TIMER SetTimer;          // EFI 1.0+
  EFI_WAIT_FOR_EVENT WaitForEvent; // EFI 1.0+
  EFI_SIGNAL_EVENT SignalEvent;    // EFI 1.0+
  EFI_CLOSE_EVENT CloseEvent;      // EFI 1.0+
  EFI_CHECK_EVENT CheckEvent;      // EFI 1.0+

  //
  // Protocol Handler Services
  //
  EFI_INSTALL_PROTOCOL_INTERFACE InstallProtocolInterface;     // EFI 1.0+
  EFI_REINSTALL_PROTOCOL_INTERFACE ReinstallProtocolInterface; // EFI 1.0+
  EFI_UNINSTALL_PROTOCOL_INTERFACE UninstallProtocolInterface; // EFI 1.0+
  EFI_HANDLE_PROTOCOL HandleProtocol;                          // EFI 1.0+
  void *Reserved;                                              // EFI 1.0+
  EFI_REGISTER_PROTOCOL_NOTIFY RegisterProtocolNotify;         // EFI 1.0+
  EFI_LOCATE_HANDLE LocateHandle;                              // EFI 1.+
  EFI_LOCATE_DEVICE_PATH LocateDevicePath;                     // EFI 1.0+
  EFI_INSTALL_CONFIGURATION_TABLE InstallConfigurationTable;   // EFI 1.0+

  //
  // Image Services
  //
  EFI_IMAGE_UNLOAD LoadImage;              // EFI 1.0+
  EFI_IMAGE_START StartImage;              // EFI 1.0+
  EFI_EXIT Exit;                           // EFI 1.0+
  EFI_IMAGE_UNLOAD UnloadImage;            // EFI 1.0+
  EFI_EXIT_BOOT_SERVICES ExitBootServices; // EFI 1.0+

  //
  // Miscellaneous Services
  //
  EFI_GET_NEXT_MONOTONIC_COUNT GetNextMonotonicCount; // EFI 1.0+
  EFI_STALL Stall;                                    // EFI 1.0+
  EFI_SET_WATCHDOG_TIMER SetWatchdogTimer;            // EFI 1.0+

  //
  // DriverSupport Services
  //
  EFI_CONNECT_CONTROLLER ConnectController;       // EFI 1.1
  EFI_DISCONNECT_CONTROLLER DisconnectController; // EFI 1.1+

  //
  // Open and Close Protocol Services
  //
  EFI_OPEN_PROTOCOL OpenProtocol;                        // EFI 1.1+
  EFI_CLOSE_PROTOCOL CloseProtocol;                      // EFI 1.1+
  EFI_OPEN_PROTOCOL_INFORMATION OpenProtocolInformation; // EFI 1.1+

  //
  // Library Services
  //
  EFI_PROTOCOLS_PER_HANDLE ProtocolsPerHandle; // EFI 1.1+
  EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer; // EFI 1.1+
  EFI_LOCATE_PROTOCOL LocateProtocol;          // EFI 1.1+
  EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES
  InstallMultipleProtocolInterfaces; // EFI 1.1+
  EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES
  UninstallMultipleProtocolInterfaces; // EFI 1.1+*

  //
  // 32-bit CRC Services
  //
  EFI_CALCULATE_CRC32 CalculateCrc32; // EFI 1.1+

  //
  // Miscellaneous Services
  //
  EFI_COPY_MEM CopyMem;              // EFI 1.1+
  EFI_SET_MEM SetMem;                // EFI 1.1+
  EFI_CREATE_EVENT_EX CreateEventEx; // UEFI 2.0+
} EFI_BOOT_SERVICES;

#endif // LLVM_LIBC_TYPES_EFI_BOOT_SERVICES_H
