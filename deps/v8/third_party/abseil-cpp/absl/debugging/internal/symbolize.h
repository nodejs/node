// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file contains internal parts of the Abseil symbolizer.
// Do not depend on the anything in this file, it may change at anytime.

#ifndef ABSL_DEBUGGING_INTERNAL_SYMBOLIZE_H_
#define ABSL_DEBUGGING_INTERNAL_SYMBOLIZE_H_

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/base/config.h"
#include "absl/strings/string_view.h"

#ifdef ABSL_INTERNAL_HAVE_ELF_SYMBOLIZE
#error ABSL_INTERNAL_HAVE_ELF_SYMBOLIZE cannot be directly set
#elif defined(__ELF__) && defined(__GLIBC__) && !defined(__asmjs__) \
      && !defined(__wasm__)
#define ABSL_INTERNAL_HAVE_ELF_SYMBOLIZE 1

#include <elf.h>
#include <link.h>  // For ElfW() macro.
#include <functional>

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {

// Iterates over all sections, invoking callback on each with the section name
// and the section header.
//
// Returns true on success; otherwise returns false in case of errors.
//
// This is not async-signal-safe.
bool ForEachSection(int fd,
                    const std::function<bool(absl::string_view name,
                                             const ElfW(Shdr) &)>& callback);

// Gets the section header for the given name, if it exists. Returns true on
// success. Otherwise, returns false.
bool GetSectionHeaderByName(int fd, const char *name, size_t name_len,
                            ElfW(Shdr) *out);

}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_INTERNAL_HAVE_ELF_SYMBOLIZE

#ifdef ABSL_INTERNAL_HAVE_DARWIN_SYMBOLIZE
#error ABSL_INTERNAL_HAVE_DARWIN_SYMBOLIZE cannot be directly set
#elif defined(__APPLE__)
#define ABSL_INTERNAL_HAVE_DARWIN_SYMBOLIZE 1
#endif

#ifdef ABSL_INTERNAL_HAVE_EMSCRIPTEN_SYMBOLIZE
#error ABSL_INTERNAL_HAVE_EMSCRIPTEN_SYMBOLIZE cannot be directly set
#elif defined(__EMSCRIPTEN__)
#define ABSL_INTERNAL_HAVE_EMSCRIPTEN_SYMBOLIZE 1
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace debugging_internal {

class SymbolDecorator;

class SymbolDecoratorDeleter {
 public:
  void operator()(SymbolDecorator* ptr);
};

using SymbolDecoratorPtr =
    std::unique_ptr<SymbolDecorator, SymbolDecoratorDeleter>;

// Represents an object that can add additional information to a symbol
// name.
class SymbolDecorator {
 public:
  // The signature of a factory function used to register and create a symbol
  // decorator. This function may be called from a signal handler, so it must
  // use an async-signal-safe allocation mechanism to allocate the returned
  // object.
  using Factory = SymbolDecoratorPtr(int fd);

  virtual ~SymbolDecorator() = default;

  // Decorates symbol name with additional information.
  //
  // pc represents the program counter we are getting symbolic name for.
  // relocation is difference between the link-time and the load-time address.
  // symbol_buf and symbol_buf_size represent the output buffer and its size.
  // Note that the buffer may not be empty -- default symbolizer may have
  // already produced some output. You are free to replace or augment the
  // contents (within the symbol_buf_size limit). tmp_buf and tmp_buf_size
  // represent temporary scratch space and its size. Use that space in
  // preference to allocating your own stack buffer to conserve stack.
  //
  // This function will not be called concurrently for the same object, but it
  // may be called from a signal handler, so it must avoid any operation that is
  // not async-signal-safe. However, it does not have to be reentrant -- if it
  // is interrupted by a signal and the handler tries to symbolize, the request
  // will go to a new SymbolDecorator instance.
  virtual void Decorate(
      const void* pc,
      ptrdiff_t relocation,
      char* symbol_buf, size_t symbol_buf_size,
      char* tmp_buf, size_t tmp_buf_size) const = 0;
};

// Registers a new symbol decorator factory and returns the previous one.
SymbolDecorator::Factory* SetSymbolDecoratorFactory(
    SymbolDecorator::Factory* factory);

// Registers an address range to a file mapping.
//
// Preconditions:
//   start <= end
//   filename != nullptr
//
// Returns true if the file was successfully registered.
bool RegisterFileMappingHint(const void* start, const void* end,
                             uint64_t offset, const char* filename);

// Looks up the file mapping registered by RegisterFileMappingHint for an
// address range. If there is one, the file name is stored in *filename and
// *start and *end are modified to reflect the registered mapping. Returns
// whether any hint was found.
bool GetFileMappingHint(const void** start, const void** end, uint64_t* offset,
                        const char** filename);

}  // namespace debugging_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // __cplusplus

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
#endif  // __cplusplus

    bool
    AbslInternalGetFileMappingHint(const void** start, const void** end,
                                   uint64_t* offset, const char** filename);

#endif  // ABSL_DEBUGGING_INTERNAL_SYMBOLIZE_H_
