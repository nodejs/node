// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/uri.h"

#include "src/char-predicates-inl.h"
#include "src/handles.h"
#include "src/isolate-inl.h"
#include "src/list.h"

namespace v8 {
namespace internal {

namespace {  // anonymous namespace for EncodeURI helper functions
bool IsUnescapePredicateInUriComponent(uc16 c) {
  if (IsAlphaNumeric(c)) {
    return true;
  }

  switch (c) {
    case '!':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '-':
    case '.':
    case '_':
    case '~':
      return true;
    default:
      return false;
  }
}

bool IsUriSeparator(uc16 c) {
  switch (c) {
    case '#':
    case ':':
    case ';':
    case '/':
    case '?':
    case '$':
    case '&':
    case '+':
    case ',':
    case '@':
    case '=':
      return true;
    default:
      return false;
  }
}

void AddHexEncodedToBuffer(uint8_t octet, List<uint8_t>* buffer) {
  buffer->Add('%');
  buffer->Add(HexCharOfValue(octet >> 4));
  buffer->Add(HexCharOfValue(octet & 0x0F));
}

void EncodeSingle(uc16 c, List<uint8_t>* buffer) {
  uint8_t x = (c >> 12) & 0xF;
  uint8_t y = (c >> 6) & 63;
  uint8_t z = c & 63;
  if (c <= 0x007F) {
    AddHexEncodedToBuffer(c, buffer);
  } else if (c <= 0x07FF) {
    AddHexEncodedToBuffer(y + 192, buffer);
    AddHexEncodedToBuffer(z + 128, buffer);
  } else {
    AddHexEncodedToBuffer(x + 224, buffer);
    AddHexEncodedToBuffer(y + 128, buffer);
    AddHexEncodedToBuffer(z + 128, buffer);
  }
}

void EncodePair(uc16 cc1, uc16 cc2, List<uint8_t>* buffer) {
  uint8_t u = ((cc1 >> 6) & 0xF) + 1;
  uint8_t w = (cc1 >> 2) & 0xF;
  uint8_t x = cc1 & 3;
  uint8_t y = (cc2 >> 6) & 0xF;
  uint8_t z = cc2 & 63;
  AddHexEncodedToBuffer((u >> 2) + 240, buffer);
  AddHexEncodedToBuffer((((u & 3) << 4) | w) + 128, buffer);
  AddHexEncodedToBuffer(((x << 4) | y) + 128, buffer);
  AddHexEncodedToBuffer(z + 128, buffer);
}

}  // anonymous namespace

Object* Uri::Encode(Isolate* isolate, Handle<String> uri, bool is_uri) {
  uri = String::Flatten(uri);
  int uri_length = uri->length();
  List<uint8_t> buffer(uri_length);

  {
    DisallowHeapAllocation no_gc;
    String::FlatContent uri_content = uri->GetFlatContent();

    for (int k = 0; k < uri_length; k++) {
      uc16 cc1 = uri_content.Get(k);
      if (unibrow::Utf16::IsLeadSurrogate(cc1)) {
        k++;
        if (k < uri_length) {
          uc16 cc2 = uri->Get(k);
          if (unibrow::Utf16::IsTrailSurrogate(cc2)) {
            EncodePair(cc1, cc2, &buffer);
            continue;
          }
        }
      } else if (!unibrow::Utf16::IsTrailSurrogate(cc1)) {
        if (IsUnescapePredicateInUriComponent(cc1) ||
            (is_uri && IsUriSeparator(cc1))) {
          buffer.Add(cc1);
        } else {
          EncodeSingle(cc1, &buffer);
        }
        continue;
      }

      AllowHeapAllocation allocate_error_and_return;
      THROW_NEW_ERROR_RETURN_FAILURE(isolate, NewURIError());
    }
  }

  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      isolate->factory()->NewStringFromOneByte(buffer.ToConstVector()));
  return *result;
}

}  // namespace internal
}  // namespace v8
