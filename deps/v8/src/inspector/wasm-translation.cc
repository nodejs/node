// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/wasm-translation.h"

#include <algorithm>
#include <utility>

#include "src/debug/debug-interface.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-debugger-agent-impl.h"
#include "src/inspector/v8-debugger-script.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-impl.h"

namespace v8_inspector {

using OffsetTable = v8::debug::WasmDisassembly::OffsetTable;

struct WasmSourceInformation {
  String16 source;
  int end_line = 0;
  int end_column = 0;

  OffsetTable offset_table;
  OffsetTable reverse_offset_table;

  WasmSourceInformation(String16 source, OffsetTable offset_table)
      : source(std::move(source)), offset_table(std::move(offset_table)) {
    int num_lines = 0;
    int last_newline = -1;
    size_t next_newline = this->source.find('\n', last_newline + 1);
    while (next_newline != String16::kNotFound) {
      last_newline = static_cast<int>(next_newline);
      next_newline = this->source.find('\n', last_newline + 1);
      ++num_lines;
    }
    end_line = num_lines;
    end_column = static_cast<int>(this->source.length()) - last_newline - 1;

    reverse_offset_table = this->offset_table;
    // Order by line, column, then byte offset.
    auto cmp = [](OffsetTable::value_type el1, OffsetTable::value_type el2) {
      if (el1.line != el2.line) return el1.line < el2.line;
      if (el1.column != el2.column) return el1.column < el2.column;
      return el1.byte_offset < el2.byte_offset;
    };
    std::sort(reverse_offset_table.begin(), reverse_offset_table.end(), cmp);
  }

  WasmSourceInformation() = default;
};

class WasmTranslation::TranslatorImpl {
 public:
  struct TransLocation {
    WasmTranslation* translation;
    String16 script_id;
    int line;
    int column;
    TransLocation(WasmTranslation* translation, String16 script_id, int line,
                  int column)
        : translation(translation),
          script_id(std::move(script_id)),
          line(line),
          column(column) {}
  };

  TranslatorImpl(v8::Isolate* isolate, WasmTranslation* translation,
                 v8::Local<v8::debug::WasmScript> script)
      : script_(isolate, script) {
    script_.AnnotateStrongRetainer(kGlobalScriptHandleLabel);

    ForEachFunction(script, [this, translation](String16& script_id,
                                                int func_idx) {
      translation->AddFakeScript(GetFakeScriptId(script_id, func_idx), this);
    });
  }

  template <typename Callback>
  void ForEachFunction(v8::Local<v8::debug::WasmScript> script,
                       Callback callback) {
    int num_functions = script->NumFunctions();
    int num_imported_functions = script->NumImportedFunctions();
    DCHECK_LE(0, num_imported_functions);
    DCHECK_LE(0, num_functions);
    DCHECK_GE(num_functions, num_imported_functions);
    String16 script_id = String16::fromInteger(script->Id());
    for (int func_idx = num_imported_functions; func_idx < num_functions;
         ++func_idx) {
      callback(script_id, func_idx);
    }
  }

  void ReportFakeScripts(v8::Isolate* isolate, WasmTranslation* translation,
                         V8DebuggerAgentImpl* agent) {
    ForEachFunction(
        script_.Get(isolate), [=](String16& script_id, int func_idx) {
          ReportFakeScript(isolate, script_id, func_idx, translation, agent);
        });
  }

  void Translate(TransLocation* loc) {
    const OffsetTable& offset_table = GetOffsetTable(loc);
    DCHECK(!offset_table.empty());
    uint32_t byte_offset = static_cast<uint32_t>(loc->column);

    // Binary search for the given offset.
    unsigned left = 0;                                            // inclusive
    unsigned right = static_cast<unsigned>(offset_table.size());  // exclusive
    while (right - left > 1) {
      unsigned mid = (left + right) / 2;
      if (offset_table[mid].byte_offset <= byte_offset) {
        left = mid;
      } else {
        right = mid;
      }
    }

    loc->script_id = GetFakeScriptId(loc);
    if (offset_table[left].byte_offset == byte_offset) {
      loc->line = offset_table[left].line;
      loc->column = offset_table[left].column;
    } else {
      loc->line = 0;
      loc->column = 0;
    }
  }

  static bool LessThan(const v8::debug::WasmDisassemblyOffsetTableEntry& entry,
                       const TransLocation& loc) {
    return entry.line < loc.line ||
           (entry.line == loc.line && entry.column < loc.column);
  }

  void TranslateBack(TransLocation* loc) {
    v8::Isolate* isolate = loc->translation->isolate_;
    int func_index = GetFunctionIndexFromFakeScriptId(loc->script_id);
    const OffsetTable& reverse_table = GetReverseTable(isolate, func_index);
    if (reverse_table.empty()) return;

    // Binary search for the given line and column.
    auto element = std::lower_bound(reverse_table.begin(), reverse_table.end(),
                                    *loc, LessThan);

    int found_byte_offset = 0;
    // We want an entry on the same line if possible.
    if (element == reverse_table.end()) {
      // We did not find an element, so this points after the function.
      std::pair<int, int> func_range =
          script_.Get(isolate)->GetFunctionRange(func_index);
      DCHECK_LE(func_range.first, func_range.second);
      found_byte_offset = func_range.second - func_range.first;
    } else if (element->line == loc->line || element == reverse_table.begin()) {
      found_byte_offset = element->byte_offset;
    } else {
      auto prev = element - 1;
      DCHECK(prev->line == loc->line);
      found_byte_offset = prev->byte_offset;
    }

    loc->script_id = String16::fromInteger(script_.Get(isolate)->Id());
    loc->line = func_index;
    loc->column = found_byte_offset;
  }

  const WasmSourceInformation& GetSourceInformation(v8::Isolate* isolate,
                                                    int index) {
    auto it = source_informations_.find(index);
    if (it != source_informations_.end()) return it->second;
    v8::HandleScope scope(isolate);
    v8::Local<v8::debug::WasmScript> script = script_.Get(isolate);
    v8::debug::WasmDisassembly disassembly = script->DisassembleFunction(index);

    auto inserted = source_informations_.insert(std::make_pair(
        index, WasmSourceInformation({disassembly.disassembly.data(),
                                      disassembly.disassembly.length()},
                                     std::move(disassembly.offset_table))));
    DCHECK(inserted.second);
    return inserted.first->second;
  }

  const String16 GetHash(v8::Isolate* isolate, int index) {
    v8::HandleScope scope(isolate);
    v8::Local<v8::debug::WasmScript> script = script_.Get(isolate);
    uint32_t hash = script->GetFunctionHash(index);
    String16Builder builder;
    builder.appendUnsignedAsHex(hash);
    return builder.toString();
  }

  int GetContextId(v8::Isolate* isolate) {
    v8::HandleScope scope(isolate);
    v8::Local<v8::debug::WasmScript> script = script_.Get(isolate);
    return script->ContextId().FromMaybe(0);
  }

 private:
  String16 GetFakeScriptUrl(v8::Isolate* isolate, int func_index) {
    v8::Local<v8::debug::WasmScript> script = script_.Get(isolate);
    String16 script_name =
        toProtocolString(isolate, script->Name().ToLocalChecked());
    int numFunctions = script->NumFunctions();
    int numImported = script->NumImportedFunctions();
    String16Builder builder;
    builder.appendAll("wasm://wasm/", script_name, '/');
    if (numFunctions - numImported > 300) {
      size_t digits = String16::fromInteger(numFunctions - 1).length();
      String16 thisCategory = String16::fromInteger((func_index / 100) * 100);
      DCHECK_LE(thisCategory.length(), digits);
      for (size_t i = thisCategory.length(); i < digits; ++i)
        builder.append('0');
      builder.appendAll(thisCategory, '/');
    }
    builder.appendAll(script_name, '-');
    builder.appendNumber(func_index);
    return builder.toString();
  }

  String16 GetFakeScriptId(const String16& script_id, int func_index) {
    return String16::concat(script_id, '-', String16::fromInteger(func_index));
  }
  String16 GetFakeScriptId(const TransLocation* loc) {
    return GetFakeScriptId(loc->script_id, loc->line);
  }

  void ReportFakeScript(v8::Isolate* isolate,
                        const String16& underlyingScriptId, int func_idx,
                        WasmTranslation* translation,
                        V8DebuggerAgentImpl* agent) {
    String16 fake_script_id = GetFakeScriptId(underlyingScriptId, func_idx);
    String16 fake_script_url = GetFakeScriptUrl(isolate, func_idx);

    std::unique_ptr<V8DebuggerScript> fake_script =
        V8DebuggerScript::CreateWasm(isolate, translation, script_.Get(isolate),
                                     fake_script_id, std::move(fake_script_url),
                                     func_idx);

    agent->didParseSource(std::move(fake_script), true);
  }

  int GetFunctionIndexFromFakeScriptId(const String16& fake_script_id) {
    size_t last_dash_pos = fake_script_id.reverseFind('-');
    DCHECK_GT(fake_script_id.length(), last_dash_pos);
    bool ok = true;
    int func_index = fake_script_id.substring(last_dash_pos + 1).toInteger(&ok);
    DCHECK(ok);
    return func_index;
  }

  const OffsetTable& GetOffsetTable(const TransLocation* loc) {
    int func_index = loc->line;
    return GetSourceInformation(loc->translation->isolate_, func_index)
        .offset_table;
  }

  const OffsetTable& GetReverseTable(v8::Isolate* isolate, int func_index) {
    return GetSourceInformation(isolate, func_index).reverse_offset_table;
  }

  static constexpr char kGlobalScriptHandleLabel[] =
      "WasmTranslation::TranslatorImpl::script_";

  v8::Global<v8::debug::WasmScript> script_;

  // We assume to only disassemble a subset of the functions, so store them in a
  // map instead of an array.
  std::unordered_map<int, WasmSourceInformation> source_informations_;

  // Disallow copies, because our pointer is registered in translation.
  DISALLOW_COPY_AND_ASSIGN(TranslatorImpl);
};

constexpr char WasmTranslation::TranslatorImpl::kGlobalScriptHandleLabel[];

WasmTranslation::WasmTranslation(v8::Isolate* isolate) : isolate_(isolate) {}

WasmTranslation::~WasmTranslation() { Clear(); }

void WasmTranslation::AddScript(v8::Local<v8::debug::WasmScript> script,
                                V8DebuggerAgentImpl* agent) {
  auto& impl = wasm_translators_[script->Id()];
  if (impl == nullptr) {
    impl = std::make_unique<TranslatorImpl>(isolate_, this, script);
  }
  impl->ReportFakeScripts(isolate_, this, agent);
}

void WasmTranslation::Clear() {
  wasm_translators_.clear();
  fake_scripts_.clear();
}

void WasmTranslation::Clear(v8::Isolate* isolate,
                            const std::vector<int>& contextIdsToClear) {
  for (auto iter = fake_scripts_.begin(); iter != fake_scripts_.end();) {
    auto contextId = iter->second->GetContextId(isolate);
    auto it = std::find(std::begin(contextIdsToClear),
                        std::end(contextIdsToClear), contextId);
    if (it != std::end(contextIdsToClear)) {
      iter = fake_scripts_.erase(iter);
    } else {
      ++iter;
    }
  }

  for (auto iter = wasm_translators_.begin();
       iter != wasm_translators_.end();) {
    auto contextId = iter->second->GetContextId(isolate);
    auto it = std::find(std::begin(contextIdsToClear),
                        std::end(contextIdsToClear), contextId);
    if (it != std::end(contextIdsToClear)) {
      iter = wasm_translators_.erase(iter);
    } else {
      ++iter;
    }
  }
}

const String16& WasmTranslation::GetSource(const String16& script_id,
                                           int func_index) {
  auto it = fake_scripts_.find(script_id);
  DCHECK_NE(it, fake_scripts_.end());
  return it->second->GetSourceInformation(isolate_, func_index).source;
}

int WasmTranslation::GetEndLine(const String16& script_id, int func_index) {
  auto it = fake_scripts_.find(script_id);
  DCHECK_NE(it, fake_scripts_.end());
  return it->second->GetSourceInformation(isolate_, func_index).end_line;
}

int WasmTranslation::GetEndColumn(const String16& script_id, int func_index) {
  auto it = fake_scripts_.find(script_id);
  DCHECK_NE(it, fake_scripts_.end());
  return it->second->GetSourceInformation(isolate_, func_index).end_column;
}

String16 WasmTranslation::GetHash(const String16& script_id, int func_index) {
  auto it = fake_scripts_.find(script_id);
  DCHECK_NE(it, fake_scripts_.end());
  return it->second->GetHash(isolate_, func_index);
}

// Translation "forward" (to artificial scripts).
bool WasmTranslation::TranslateWasmScriptLocationToProtocolLocation(
    String16* script_id, int* line_number, int* column_number) {
  DCHECK(script_id && line_number && column_number);
  bool ok = true;
  int script_id_int = script_id->toInteger(&ok);
  if (!ok) return false;

  auto it = wasm_translators_.find(script_id_int);
  if (it == wasm_translators_.end()) return false;
  TranslatorImpl* translator = it->second.get();

  TranslatorImpl::TransLocation trans_loc(this, std::move(*script_id),
                                          *line_number, *column_number);
  translator->Translate(&trans_loc);

  *script_id = std::move(trans_loc.script_id);
  *line_number = trans_loc.line;
  *column_number = trans_loc.column;

  return true;
}

// Translation "backward" (from artificial to real scripts).
bool WasmTranslation::TranslateProtocolLocationToWasmScriptLocation(
    String16* script_id, int* line_number, int* column_number) {
  auto it = fake_scripts_.find(*script_id);
  if (it == fake_scripts_.end()) return false;
  TranslatorImpl* translator = it->second;

  TranslatorImpl::TransLocation trans_loc(this, std::move(*script_id),
                                          *line_number, *column_number);
  translator->TranslateBack(&trans_loc);

  *script_id = std::move(trans_loc.script_id);
  *line_number = trans_loc.line;
  *column_number = trans_loc.column;

  return true;
}

void WasmTranslation::AddFakeScript(const String16& scriptId,
                                    TranslatorImpl* translator) {
  DCHECK_EQ(0, fake_scripts_.count(scriptId));
  fake_scripts_.insert(std::make_pair(scriptId, translator));
}
}  // namespace v8_inspector
