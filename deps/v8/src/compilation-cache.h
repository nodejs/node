// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_COMPILATION_CACHE_H_
#define V8_COMPILATION_CACHE_H_

namespace v8 {
namespace internal {


// The compilation cache keeps shared function infos for compiled
// scripts and evals. The shared function infos are looked up using
// the source string as the key. For regular expressions the
// compilation data is cached.
class CompilationCache {
 public:
  // Finds the script shared function info for a source
  // string. Returns an empty handle if the cache doesn't contain a
  // script for the given source string with the right origin.
  static Handle<SharedFunctionInfo> LookupScript(Handle<String> source,
                                                 Handle<Object> name,
                                                 int line_offset,
                                                 int column_offset);

  // Finds the shared function info for a source string for eval in a
  // given context.  Returns an empty handle if the cache doesn't
  // contain a script for the given source string.
  static Handle<SharedFunctionInfo> LookupEval(Handle<String> source,
                                               Handle<Context> context,
                                               bool is_global);

  // Returns the regexp data associated with the given regexp if it
  // is in cache, otherwise an empty handle.
  static Handle<FixedArray> LookupRegExp(Handle<String> source,
                                         JSRegExp::Flags flags);

  // Associate the (source, kind) pair to the shared function
  // info. This may overwrite an existing mapping.
  static void PutScript(Handle<String> source,
                        Handle<SharedFunctionInfo> function_info);

  // Associate the (source, context->closure()->shared(), kind) triple
  // with the shared function info. This may overwrite an existing mapping.
  static void PutEval(Handle<String> source,
                      Handle<Context> context,
                      bool is_global,
                      Handle<SharedFunctionInfo> function_info);

  // Associate the (source, flags) pair to the given regexp data.
  // This may overwrite an existing mapping.
  static void PutRegExp(Handle<String> source,
                        JSRegExp::Flags flags,
                        Handle<FixedArray> data);

  // Clear the cache - also used to initialize the cache at startup.
  static void Clear();

  // GC support.
  static void Iterate(ObjectVisitor* v);

  // Notify the cache that a mark-sweep garbage collection is about to
  // take place. This is used to retire entries from the cache to
  // avoid keeping them alive too long without using them.
  static void MarkCompactPrologue();

  // Enable/disable compilation cache. Used by debugger to disable compilation
  // cache during debugging to make sure new scripts are always compiled.
  static void Enable();
  static void Disable();
};


} }  // namespace v8::internal

#endif  // V8_COMPILATION_CACHE_H_
