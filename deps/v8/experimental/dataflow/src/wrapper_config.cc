// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wrapper_config.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "llvm/Support/FileSystem.h"

namespace dfg {

static std::string StripWhitespace(const std::string& Str) {
  size_t First = Str.find_first_not_of(" \t\r\n");
  if (First == std::string::npos) return "";
  size_t Last = Str.find_last_not_of(" \t\r\n");
  return Str.substr(First, (Last - First + 1));
}

std::string WrapperConfigManager::GetBaseTypeName(
    const std::string& TypeName) const {
  std::string S = StripWhitespace(TypeName);

  // Strip leading const
  if (S.rfind("const ", 0) == 0) {
    S = S.substr(6);
  }
  // Strip leading class or struct keywords
  if (S.rfind("class ", 0) == 0) {
    S = S.substr(6);
  } else if (S.rfind("struct ", 0) == 0) {
    S = S.substr(7);
  }
  // Strip trailing pointer/reference chars
  while (!S.empty() &&
         (S.back() == '*' || S.back() == '&' || S.back() == ' ')) {
    S.pop_back();
  }
  S = StripWhitespace(S);

  // Strip template parameters first (e.g. std::unique_ptr<Map> ->
  // std::unique_ptr)
  size_t TemplIdx = S.find('<');
  if (TemplIdx != std::string::npos) {
    S = S.substr(0, TemplIdx);
  }

  // Strip namespace prefix next (e.g. std::shared_ptr -> shared_ptr)
  size_t ColonIdx = S.rfind("::");
  if (ColonIdx != std::string::npos) {
    S = S.substr(ColonIdx + 2);
  }

  std::string BaseName = StripWhitespace(S);

  return BaseName;
}

const WrapperConfig* WrapperConfigManager::GetConfigForType(
    const std::string& TypeName) const {
  std::string BaseName = GetBaseTypeName(TypeName);
  auto it = Configs.find(BaseName);
  if (it != Configs.end()) {
    return &it->second;
  }
  return nullptr;
}

bool WrapperConfigManager::IsIdentityFunction(const std::string& Name) const {
  return IdentityFnNames.count(Name) > 0;
}

void WrapperConfigManager::RegisterDefaultConfigs() {
  // Register default identity / cast function names
  IdentityFnNames = {"cast",
                     "Cast",
                     "TrustedCast",
                     "TrustedCast_null",
                     "UncheckedCast",
                     "UncheckedCast_null",
                     "CheckedCast",
                     "CheckedCast_null",
                     "move",
                     "forward",
                     "Just",
                     "FromJust",
                     "direct_handle",
                     "indirect_handle",
                     "make_handle",
                     "handle",
                     "OpenHandle",
                     "OpenDirectHandle",
                     "OpenIndirectHandle",
                     "ToLocal",
                     "__builtin_expect",
                     "DecompressTagged",
                     "DecompressTaggedSigned",
                     "DecompressTaggedAny",
                     "CompressObject",
                     "CompressAny",
                     "Acquire_Load",
                     "Relaxed_Load",
                     "SeqCst_Load",
                     "Acquire_Load_No_Unpack",
                     "Relaxed_Load_Map_Word",
                     "Relaxed_Load_Raw",
                     "to_storage_type",
                     "to_return_type",
                     "to_storage_addr",
                     "AsAtomicPtr",
                     "tagged_to_full",
                     "full_to_tagged",
                     "RawToTagged"};

  // 1. TaggedMember (V8 wrapper member variable)
  {
    WrapperConfig Config;
    Config.WrapperTypeName = "TaggedMember";

    // Loads: .load(), atomic loads, swap/CAS returning old value, operators,
    // constructor
    Config.LoadOps.push_back({"load", -1});
    Config.LoadOps.push_back({"Relaxed_Load", -1});
    Config.LoadOps.push_back({"Acquire_Load", -1});
    Config.LoadOps.push_back({"SeqCst_Load", -1});
    Config.LoadOps.push_back({"operator*", -1});
    Config.LoadOps.push_back({"operator->", -1});
    Config.LoadOps.push_back({"implicit_cast", -1});
    Config.LoadOps.push_back({"constructor", 0});
    Config.LoadOps.push_back({"SeqCst_Swap", 1});
    Config.LoadOps.push_back({"SeqCst_CompareAndSwap", 2});

    // Stores: .store(), atomic stores, stores w/o barrier, swap/CAS, assignment
    Config.StoreOps.push_back({"store", 1});
    Config.StoreOps.push_back({"store_no_write_barrier", 0});
    Config.StoreOps.push_back({"Relaxed_Store", 1});
    Config.StoreOps.push_back({"Relaxed_Store_no_write_barrier", 0});
    Config.StoreOps.push_back({"Release_Store", 1});
    Config.StoreOps.push_back({"Release_Store_no_write_barrier", 0});
    Config.StoreOps.push_back({"SeqCst_Store", 1});
    Config.StoreOps.push_back({"SeqCst_Store_no_write_barrier", 0});
    Config.StoreOps.push_back({"SeqCst_Swap", 1});
    Config.StoreOps.push_back({"SeqCst_CompareAndSwap", 2});
    Config.StoreOps.push_back({"operator=", 0});

    Configs[Config.WrapperTypeName] = Config;
  }

  // 1.5 ProtectedTaggedMember (Trusted space variant of TaggedMember)
  {
    WrapperConfig Config;
    Config.WrapperTypeName = "ProtectedTaggedMember";

    // Copy loads/stores from TaggedMember
    Config.LoadOps = Configs["TaggedMember"].LoadOps;
    Config.StoreOps = Configs["TaggedMember"].StoreOps;

    Configs[Config.WrapperTypeName] = Config;
  }

  // 1.7 Tagged (V8 primary smart pointer/reference wrapper)
  {
    WrapperConfig Config;
    Config.WrapperTypeName = "Tagged";

    Config.LoadOps.push_back({"operator*", -1});
    Config.LoadOps.push_back({"operator->", -1});
    Config.LoadOps.push_back({"implicit_cast", -1});
    Config.LoadOps.push_back({"GetHeapObjectAssumeWeak", -1});
    Config.LoadOps.push_back({"GetHeapObjectAssumeStrong", -1});
    Config.LoadOps.push_back({"GetHeapObjectOrSmi", -1});
    Config.LoadOps.push_back({"ToStrongHeapObject", -1});
    Config.LoadOps.push_back({"value", -1});
    Config.LoadOps.push_back({"ptr", -1});
    Config.LoadOps.push_back({"constructor", 0});

    Config.StoreOps.push_back({"operator=", 0});

    Configs[Config.WrapperTypeName] = Config;
  }

  // 2. Handle, IndirectHandle, DirectHandle, MaybeObjectHandle, MaybeHandle,
  // MaybeDirectHandle, MaybeObjectDirectHandle, AllocationResult, Local, MaybeLocal,
  // TNode, SloppyTNode, OptionalRef, Optional (V8 pointer/value wrappers)
  for (const std::string& Name :
       {"Handle", "IndirectHandle", "DirectHandle", "MaybeObjectHandle",
        "MaybeObjectIndirectHandle", "MaybeHandle", "MaybeIndirectHandle",
        "MaybeDirectHandle", "MaybeObjectDirectHandle", "AllocationResult",
        "Local", "MaybeLocal", "TNode", "SloppyTNode", "V", "ConstOrV",
        "OptionalV", "OptionalRef", "Optional"}) {
    WrapperConfig Config;
    Config.WrapperTypeName = Name;

    Config.LoadOps.push_back({"operator*", -1});
    Config.LoadOps.push_back({"operator->", -1});
    Config.LoadOps.push_back({"implicit_cast", -1});
    Config.LoadOps.push_back({"location", -1});
    Config.LoadOps.push_back({"ToObject", -1});
    Config.LoadOps.push_back({"ToObjectChecked", -1});
    Config.LoadOps.push_back({"ToLocal", -1});
    Config.LoadOps.push_back({"ToLocalChecked", -1});
    Config.LoadOps.push_back({"ToAddress", -1});
    Config.LoadOps.push_back({"value", -1});
    Config.LoadOps.push_back({"value_or", -1});
    Config.LoadOps.push_back({"constructor", 0});

    Config.StoreOps.push_back({"operator=", 0});

    Configs[Config.WrapperTypeName] = Config;
  }

  // 3. std::unique_ptr, std::shared_ptr (Standard C++ smart pointers)
  for (const std::string& Name : {"unique_ptr", "shared_ptr"}) {
    WrapperConfig Config;
    Config.WrapperTypeName = Name;

    Config.LoadOps.push_back({"get", -1});
    Config.LoadOps.push_back({"operator*", -1});
    Config.LoadOps.push_back({"operator->", -1});
    Config.LoadOps.push_back({"constructor", 0});

    Config.StoreOps.push_back({"reset", 0});
    Config.StoreOps.push_back({"operator=", 0});

    Configs[Config.WrapperTypeName] = Config;
  }

  // 4. std::atomic, std::atomic_ref, v8::base::AtomicValue, v8::base::StrongAlias,
  // and V8 slot/member wrappers
  for (const std::string& Name :
       {"atomic", "atomic_ref", "AtomicValue", "StrongAlias", "SlotBase",
        "FullObjectSlot", "MaybeObjectSlot", "ObjectSlot",
        "OffHeapFullObjectSlot", "AtomicSlot", "ExternalPointerMember",
        "TrustedPointerMember", "JSDispatchHandleMember", "Effect",
        "Control"}) {
    WrapperConfig Config;
    Config.WrapperTypeName = Name;

    Config.LoadOps.push_back({"load", -1});
    Config.LoadOps.push_back({"Acquire_Load", -1});
    Config.LoadOps.push_back({"Relaxed_Load", -1});
    Config.LoadOps.push_back({"SeqCst_Load", -1});
    Config.LoadOps.push_back({"Acquire_Load_No_Unpack", -1});
    Config.LoadOps.push_back({"Relaxed_Load_Map_Word", -1});
    Config.LoadOps.push_back({"Relaxed_Load_Raw", -1});
    Config.LoadOps.push_back({"load_map", -1});
    Config.LoadOps.push_back({"Value", -1});
    Config.LoadOps.push_back({"value", -1});
    Config.LoadOps.push_back({"ptr", -1});
    Config.LoadOps.push_back({"ptr_location", -1});
    Config.LoadOps.push_back({"location", -1});
    Config.LoadOps.push_back({"address", -1});
    Config.LoadOps.push_back({"operator*", -1});
    Config.LoadOps.push_back({"operator->", -1});
    Config.LoadOps.push_back({"implicit_cast", -1});
    Config.LoadOps.push_back({"constructor", 0});

    Config.StoreOps.push_back({"store", 0});
    Config.StoreOps.push_back({"Release_Store", 0});
    Config.StoreOps.push_back({"Relaxed_Store", 0});
    Config.StoreOps.push_back({"SeqCst_Store", 0});
    Config.StoreOps.push_back({"store_map", 0});
    Config.StoreOps.push_back({"SetValue", 0});
    Config.StoreOps.push_back({"exchange", 0});
    Config.StoreOps.push_back({"compare_exchange_strong", 1});
    Config.StoreOps.push_back({"compare_exchange_weak", 1});
    Config.StoreOps.push_back({"Release_CompareAndSwap", 1});
    Config.StoreOps.push_back({"Relaxed_CompareAndSwap", 1});
    Config.StoreOps.push_back({"SeqCst_CompareAndSwap", 1});
    Config.StoreOps.push_back({"operator=", 0});

    Configs[Config.WrapperTypeName] = Config;
  }

  // 5. Sequence containers (ZoneVector, std::vector, base::SmallVector,
  // base::Vector, base::OwnedVector, std::deque, std::list)
  for (const std::string& Name :
       {"ZoneVector", "vector", "SmallVector", "Vector", "OwnedVector",
        "deque", "list"}) {
    WrapperConfig Config;
    Config.WrapperTypeName = Name;

    Config.LoadOps.push_back({"at", -1});
    Config.LoadOps.push_back({"operator[]", -1});
    Config.LoadOps.push_back({"front", -1});
    Config.LoadOps.push_back({"back", -1});
    Config.LoadOps.push_back({"data", -1});

    Config.StoreOps.push_back({"push_back", 0});
    Config.StoreOps.push_back({"emplace_back", 0});
    Config.StoreOps.push_back({"push_front", 0});
    Config.StoreOps.push_back({"emplace_front", 0});
    Config.StoreOps.push_back({"operator=", 0});

    Configs[Config.WrapperTypeName] = Config;
  }

  // 8. Optional & Maybe Wrappers (std::optional, v8::base::Optional, v8::Maybe)
  for (const char* Name : {"optional", "Optional", "Maybe"}) {
    WrapperConfig Config;
    Config.WrapperTypeName = Name;

    Config.LoadOps.push_back({"value", -1});
    Config.LoadOps.push_back({"value_or", -1});
    Config.LoadOps.push_back({"FromJust", -1});
    Config.LoadOps.push_back({"FromMaybe", -1});
    Config.LoadOps.push_back({"operator*", -1});
    Config.LoadOps.push_back({"operator->", -1});
    Config.LoadOps.push_back({"implicit_cast", -1});
    Config.LoadOps.push_back({"constructor", 0});

    Config.StoreOps.push_back({"operator=", 0});
    Config.StoreOps.push_back({"emplace", 0});

    Configs[Config.WrapperTypeName] = Config;
  }
}

bool WrapperConfigManager::LoadFromJson(const std::string& JsonStr) {
  auto ValueOrErr = llvm::json::parse(JsonStr);
  if (!ValueOrErr) {
    std::cerr
        << "[WrapperConfig] Error: Failed to parse dynamic JSON configuration: "
        << llvm::toString(ValueOrErr.takeError()) << std::endl;
    return false;
  }

  const auto* Obj = ValueOrErr->getAsObject();
  if (!Obj) return false;

  // Parse optional global identity / cast functions
  if (const auto* IdentityArr = Obj->getArray("identity_functions")) {
    for (const auto& FnVal : *IdentityArr) {
      if (auto FnName = FnVal.getAsString()) {
        IdentityFnNames.insert(FnName->str());
      }
    }
    std::cerr << "[WrapperConfig] Loaded " << IdentityFnNames.size()
              << " total identity/cast functions." << std::endl;
  }

  if (const auto* WrappersArr = Obj->getArray("wrappers")) {
    for (const auto& WrapperVal : *WrappersArr) {
      const auto* WrapperObj = WrapperVal.getAsObject();
      if (!WrapperObj) continue;

      std::string Name = WrapperObj->getString("name").value_or("").str();
      if (Name.empty()) continue;

      WrapperConfig Config;
      Config.WrapperTypeName = Name;

      // Parse Load operations
      if (const auto* LoadsArr = WrapperObj->getArray("loads")) {
        for (const auto& LoadVal : *LoadsArr) {
          const auto* LoadObj = LoadVal.getAsObject();
          if (!LoadObj) continue;

          WrapperOp Op;
          Op.MethodName = LoadObj->getString("method").value_or("").str();
          Op.ValueArgIndex = LoadObj->getInteger("value_arg").value_or(-1);

          if (!Op.MethodName.empty()) {
            Config.LoadOps.push_back(Op);
          }
        }
      }

      // Parse Store operations
      if (const auto* StoresArr = WrapperObj->getArray("stores")) {
        for (const auto& StoreVal : *StoresArr) {
          const auto* StoreObj = StoreVal.getAsObject();
          if (!StoreObj) continue;

          WrapperOp Op;
          Op.MethodName = StoreObj->getString("method").value_or("").str();
          Op.ValueArgIndex = StoreObj->getInteger("value_arg").value_or(-1);

          if (!Op.MethodName.empty()) {
            Config.StoreOps.push_back(Op);
          }
        }
      }

      // Override or register configuration
      Configs[Config.WrapperTypeName] = Config;
      std::cerr << "[WrapperConfig] Loaded custom config for wrapper: " << Name
                << std::endl;
    }
  }

  return true;
}

bool WrapperConfigManager::LoadFromFile(const std::string& FilePath) {
  if (!llvm::sys::fs::exists(FilePath)) {
    return false;
  }

  std::ifstream File(FilePath);
  if (!File.is_open()) return false;

  std::stringstream Stream;
  Stream << File.rdbuf();
  return LoadFromJson(Stream.str());
}

bool WrapperConfigManager::IsWrapperClass(const std::string& ClassName) const {
  std::string BaseName = GetBaseTypeName(ClassName);
  if (Configs.count(BaseName) > 0) return true;

  static const std::unordered_set<std::string> ExtraWrappers = {
      "HandleBase", "DirectHandleBase", "TaggedImpl", "MaybeHandle",
      "MaybeDirectHandle"};
  return ExtraWrappers.count(BaseName) > 0 ||
         ExtraWrappers.count(ClassName) > 0;
}

}  // namespace dfg
