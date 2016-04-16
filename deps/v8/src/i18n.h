// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// limitations under the License.

#ifndef V8_I18N_H_
#define V8_I18N_H_

#include "src/handles.h"
#include "unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
class Collator;
class DecimalFormat;
class SimpleDateFormat;
}

namespace v8 {
namespace internal {

// Forward declarations.
class ObjectTemplateInfo;

class I18N {
 public:
  // Creates an ObjectTemplate with one internal field.
  static Handle<ObjectTemplateInfo> GetTemplate(Isolate* isolate);

  // Creates an ObjectTemplate with two internal fields.
  static Handle<ObjectTemplateInfo> GetTemplate2(Isolate* isolate);

 private:
  I18N();
};


class DateFormat {
 public:
  // Create a formatter for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::SimpleDateFormat* InitializeDateTimeFormat(
      Isolate* isolate,
      Handle<String> locale,
      Handle<JSObject> options,
      Handle<JSObject> resolved);

  // Unpacks date format object from corresponding JavaScript object.
  static icu::SimpleDateFormat* UnpackDateFormat(Isolate* isolate,
                                                 Handle<JSObject> obj);

  // Release memory we allocated for the DateFormat once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteDateFormat(
      const v8::WeakCallbackData<v8::Value, void>& data);

 private:
  DateFormat();
};


class NumberFormat {
 public:
  // Create a formatter for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::DecimalFormat* InitializeNumberFormat(
      Isolate* isolate,
      Handle<String> locale,
      Handle<JSObject> options,
      Handle<JSObject> resolved);

  // Unpacks number format object from corresponding JavaScript object.
  static icu::DecimalFormat* UnpackNumberFormat(Isolate* isolate,
                                                Handle<JSObject> obj);

  // Release memory we allocated for the NumberFormat once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteNumberFormat(
      const v8::WeakCallbackData<v8::Value, void>& data);

 private:
  NumberFormat();
};


class Collator {
 public:
  // Create a collator for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::Collator* InitializeCollator(
      Isolate* isolate,
      Handle<String> locale,
      Handle<JSObject> options,
      Handle<JSObject> resolved);

  // Unpacks collator object from corresponding JavaScript object.
  static icu::Collator* UnpackCollator(Isolate* isolate, Handle<JSObject> obj);

  // Release memory we allocated for the Collator once the JS object that holds
  // the pointer gets garbage collected.
  static void DeleteCollator(
      const v8::WeakCallbackData<v8::Value, void>& data);

 private:
  Collator();
};

class BreakIterator {
 public:
  // Create a BreakIterator for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::BreakIterator* InitializeBreakIterator(
      Isolate* isolate,
      Handle<String> locale,
      Handle<JSObject> options,
      Handle<JSObject> resolved);

  // Unpacks break iterator object from corresponding JavaScript object.
  static icu::BreakIterator* UnpackBreakIterator(Isolate* isolate,
                                                 Handle<JSObject> obj);

  // Release memory we allocated for the BreakIterator once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteBreakIterator(
      const v8::WeakCallbackData<v8::Value, void>& data);

 private:
  BreakIterator();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_I18N_H_
