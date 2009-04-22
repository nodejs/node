// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "token.h"

namespace v8 { namespace internal {

#ifdef DEBUG
#define T(name, string, precedence) #name,
const char* Token::name_[NUM_TOKENS] = {
  TOKEN_LIST(T, T, IGNORE_TOKEN)
};
#undef T
#endif


#define T(name, string, precedence) string,
const char* Token::string_[NUM_TOKENS] = {
  TOKEN_LIST(T, T, IGNORE_TOKEN)
};
#undef T


#define T(name, string, precedence) precedence,
int8_t Token::precedence_[NUM_TOKENS] = {
  TOKEN_LIST(T, T, IGNORE_TOKEN)
};
#undef T


// A perfect (0 collision) hash table of keyword token values.

// larger N will reduce the number of collisions (power of 2 for fast %)
const unsigned int N = 128;
// make this small since we have <= 256 tokens
static uint8_t Hashtable[N];
static bool IsInitialized = false;


static unsigned int Hash(const char* s) {
  // The following constants have been found using trial-and-error. If the
  // keyword set changes, they may have to be recomputed (make them flags
  // and play with the flag values). Increasing N is the simplest way to
  // reduce the number of collisions.

  // we must use at least 4 or more chars ('const' and 'continue' share
  // 'con')
  const unsigned int L = 5;
  // smaller S tend to reduce the number of collisions
  const unsigned int S = 4;
  // make this a prime, or at least an odd number
  const unsigned int M = 3;

  unsigned int h = 0;
  for (unsigned int i = 0; s[i] != '\0' && i < L; i++) {
    h += (h << S) + s[i];
  }
  // unsigned int % by a power of 2 (otherwise this will not be a bit mask)
  return h * M % N;
}


Token::Value Token::Lookup(const char* str) {
  ASSERT(IsInitialized);
  Value k = static_cast<Value>(Hashtable[Hash(str)]);
  const char* s = string_[k];
  ASSERT(s != NULL || k == IDENTIFIER);
  if (s == NULL || strcmp(s, str) == 0) {
    return k;
  }
  return IDENTIFIER;
}


#ifdef DEBUG
// We need this function because C++ doesn't allow the expression
// NULL == NULL, which is a result of macro expansion below. What
// the hell?
static bool IsNull(const char* s) {
  return s == NULL;
}
#endif


void Token::Initialize() {
  if (IsInitialized) return;

  // A list of all keywords, terminated by ILLEGAL.
#define T(name, string, precedence) name,
  static Value keyword[] = {
    TOKEN_LIST(IGNORE_TOKEN, T, IGNORE_TOKEN)
    ILLEGAL
  };
#undef T

  // Assert that the keyword array contains the 25 keywords, 3 future
  // reserved words (const, debugger, and native), and the 3 named literals
  // defined by ECMA-262 standard.
  ASSERT(ARRAY_SIZE(keyword) == 25 + 3 + 3 + 1);  // +1 for ILLEGAL sentinel

  // Initialize Hashtable.
  ASSERT(NUM_TOKENS <= 256);  // Hashtable contains uint8_t elements
  for (unsigned int i = 0; i < N; i++) {
    Hashtable[i] = IDENTIFIER;
  }

  // Insert all keywords into Hashtable.
  int collisions = 0;
  for (int i = 0; keyword[i] != ILLEGAL; i++) {
    Value k = keyword[i];
    unsigned int h = Hash(string_[k]);
    if (Hashtable[h] != IDENTIFIER) collisions++;
    Hashtable[h] = k;
  }

  if (collisions > 0) {
    PrintF("%d collisions in keyword hashtable\n", collisions);
    FATAL("Fix keyword lookup!");
  }

  IsInitialized = true;

  // Verify hash table.
#define T(name, string, precedence) \
  ASSERT(IsNull(string) || Lookup(string) == IDENTIFIER);

#define K(name, string, precedence) \
  ASSERT(Lookup(string) == name);

  TOKEN_LIST(T, K, IGNORE_TOKEN)

#undef K
#undef T
}

} }  // namespace v8::internal
