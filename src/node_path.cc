#include "env-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util-inl.h"
#include "v8-fast-api-calls.h"

namespace node {

namespace path {
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

// extracted from
// https://github.com/torvalds/linux/blob/cdc9718d5e590d6905361800b938b93f2b66818e/lib/glob.c
bool glob(char const* pat, char const* str) {
  /*
   * Backtrack to previous * on mismatch and retry starting one
   * character later in the string.  Because * matches all characters
   * (no exception for /), it can be easily proved that there's
   * never a need to backtrack multiple levels.
   */
  char const* back_pat = nullptr;
  char const* back_str = nullptr;

  /*
   * Loop over each token (character or class) in pat, matching
   * it against the remaining unmatched tail of str.  Return false
   * on mismatch, or true after matching the trailing nul bytes.
   */
  for (;;) {
    unsigned char c = *str++;
    unsigned char d = *pat++;

    switch (d) {
      case '?': /* Wildcard: anything but nul */
        if (c == '\0') return false;
        break;
      case '*':           /* Any-length wildcard */
        if (*pat == '\0') /* Optimize trailing * case */
          return true;
        back_pat = pat;
        back_str = --str; /* Allow zero-length match */
        break;
      case '[': { /* Character class */
        bool match = false, inverted = (*pat == '!');
        char const* cls = pat + inverted;
        unsigned char a = *cls++;

        /*
         * Iterate over each span in the character class.
         * A span is either a single character a, or a
         * range a-b.  The first span may begin with ']'.
         */
        do {
          unsigned char b = a;

          if (a == '\0') /* Malformed */
            goto literal;

          if (cls[0] == '-' && cls[1] != ']') {
            b = cls[1];

            if (b == '\0') goto literal;

            cls += 2;
            /* Any special action if a > b? */
          }
          match |= (a <= c && c <= b);
        } while ((a = *cls++) != ']');

        if (match == inverted) goto backtrack;
        pat = cls;
      } break;
      case '\\':
        d = *pat++;
        [[fallthrough]];
      default: /* Literal character */
      literal:
        if (c == d) {
          if (d == '\0') return true;
          break;
        }
      backtrack:
        if (c == '\0' || !back_pat) return false; /* No point continuing */
        /* Try again from last *, one character later in str. */
        pat = back_pat;
        str = ++back_str;
        break;
    }
  }
}

void SlowGlob(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());

  std::string pattern = Utf8Value(env->isolate(), args[0]).ToString();
  std::string str = Utf8Value(env->isolate(), args[1]).ToString();
  args.GetReturnValue().Set(glob(pattern.c_str(), str.c_str()));
}
bool FastGlob(Local<Value> receiver,
              const v8::FastOneByteString& pattern,
              const v8::FastOneByteString& str) {
  return glob(pattern.data, str.data);
}

v8::CFunction fast_glob_(v8::CFunction::Make(FastGlob));

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetFastMethod(context, target, "glob", SlowGlob, &fast_glob_);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(SlowGlob);
  registry->Register(FastGlob);
  registry->Register(fast_glob_.GetTypeInfo());
}
}  // namespace path

}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(path, node::path::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(path, node::path::RegisterExternalReferences)
