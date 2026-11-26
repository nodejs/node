// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Bindings for <canvas> and CanvasRenderingContext2D.
//
// There is no rasterizer. Drawing calls are real native API calls that
// validate nothing and paint nothing, much like a browser's 2D context
// recording into a display list that is never played back. Drawing state
// that pages read back (styles, transform, line dash) is tracked.

#include <cmath>
#include <cstdlib>

#include "experimental/d8/mock-dom/bindings.h"
#include "experimental/d8/mock-dom/isolate-data.h"
#include "experimental/d8/mock-dom/layout.h"
#include "experimental/d8/mock-dom/objects.h"
#include "include/v8-container.h"
#include "include/v8-primitive.h"
#include "include/v8-typed-array.h"

namespace v8 {
namespace mock_dom {

namespace {

#define CALLBACK(name) void name(const CallbackInfo& info)

using State = CanvasContext::State;

void WrapThis(const CallbackInfo& info, ScriptWrappable* object) {
  Object::Wrap<kWrapperTag>(info.GetIsolate(), info.This(), object);
  object->set_wrapper(info.GetIsolate(), info.This());
}

// Gradients, patterns and paths carry no data: nothing is ever painted with
// them.
class CanvasObject final : public ScriptWrappable {
 public:
  explicit CanvasObject(Interface interface) : ScriptWrappable(interface) {}
};

class TextMetrics final : public ScriptWrappable {
 public:
  TextMetrics(double width, double font_size)
      : ScriptWrappable(Interface::kTextMetrics),
        width(width),
        font_size(font_size) {}
  double width;
  double font_size;
};

// Extracts the pixel size from a CSS font shorthand such as
// "bold 12px/1.2 Helvetica".
double FontSize(const std::string& font) {
  size_t px = font.find("px");
  if (px == std::string::npos) return 10;
  size_t start = px;
  while (start > 0 &&
         (std::isdigit(static_cast<unsigned char>(font[start - 1])) ||
          font[start - 1] == '.')) {
    --start;
  }
  double size = std::atof(font.substr(start, px - start).c_str());
  return size > 0 ? size : 10;
}

// Text is measured with a fixed advance of 0.55em per code point, close to
// the average of a proportional sans-serif face.
double MeasureText(const String& text, const std::string& font) {
  return TextWidth(text) / kCharWidth * FontSize(font) * 0.55;
}

CALLBACK(Draw) { ++Receiver<CanvasContext>(info)->draw_calls; }

CALLBACK(NoOp) {}

// --- State -------------------------------------------------------------------

struct StringState {
  const char* name;
  std::string State::* field;
};
const StringState kStringStates[] = {
    {"font", &State::font},
    {"textAlign", &State::text_align},
    {"textBaseline", &State::text_baseline},
    {"lineCap", &State::line_cap},
    {"lineJoin", &State::line_join},
    {"globalCompositeOperation", &State::global_composite_operation},
    {"shadowColor", &State::shadow_color},
};

struct NumberState {
  const char* name;
  double State::* field;
};
const NumberState kNumberStates[] = {
    {"lineWidth", &State::line_width},
    {"miterLimit", &State::miter_limit},
    {"globalAlpha", &State::global_alpha},
    {"lineDashOffset", &State::line_dash_offset},
    {"shadowBlur", &State::shadow_blur},
    {"shadowOffsetX", &State::shadow_offset_x},
    {"shadowOffsetY", &State::shadow_offset_y},
};

template <bool fill>
CALLBACK(StyleGetter) {
  Isolate* isolate = info.GetIsolate();
  State& state = Receiver<CanvasContext>(info)->state;
  TracedReference<Value>& style = fill ? state.fill_style : state.stroke_style;
  if (style.IsEmpty()) {
    info.GetReturnValue().Set(V8Name(isolate, "#000000"));
  } else {
    info.GetReturnValue().Set(style.Get(isolate));
  }
}

template <bool fill>
CALLBACK(StyleSetter) {
  Isolate* isolate = info.GetIsolate();
  State& state = Receiver<CanvasContext>(info)->state;
  TracedReference<Value>& style = fill ? state.fill_style : state.stroke_style;
  // Strings are colours; objects are gradients or patterns. Other values are
  // ignored, as an invalid colour would be.
  if (info[0]->IsString() || info[0]->IsObject()) {
    style.Reset(isolate, info[0]);
  }
}

void Multiply(double* m, const double* n) {
  double a = m[0] * n[0] + m[2] * n[1];
  double b = m[1] * n[0] + m[3] * n[1];
  double c = m[0] * n[2] + m[2] * n[3];
  double d = m[1] * n[2] + m[3] * n[3];
  double e = m[0] * n[4] + m[2] * n[5] + m[4];
  double f = m[1] * n[4] + m[3] * n[5] + m[5];
  m[0] = a;
  m[1] = b;
  m[2] = c;
  m[3] = d;
  m[4] = e;
  m[5] = f;
}

bool NumberArguments(const CallbackInfo& info, int count, double* out) {
  if (!RequireArguments(info, count)) return false;
  for (int i = 0; i < count; ++i) {
    if (!ToNumber(info, info[i], &out[i])) return false;
  }
  return true;
}

// Reads a transform from six numbers or from a DOMMatrix-like object.
bool TransformArguments(const CallbackInfo& info, double* out) {
  Isolate* isolate = info.GetIsolate();
  if (info.Length() == 1 && info[0]->IsObject()) {
    static const char* const kNames[] = {"a", "b", "c", "d", "e", "f"};
    for (int i = 0; i < 6; ++i) {
      Local<Value> value;
      if (!GetProperty(isolate, info[0].As<Object>(), kNames[i], &value) ||
          !ToNumber(info, value, &out[i])) {
        return false;
      }
    }
    return true;
  }
  return NumberArguments(info, 6, out);
}

void InstallStateBindings(Isolate* isolate) {
  const Interface kContext = Interface::kCanvasRenderingContext2D;
  DefineAccessor(isolate, kContext, "canvas", [](const CallbackInfo& info) {
    Return(info, Receiver<CanvasContext>(info)->canvas());
  });
  DefineAccessor(isolate, kContext, "fillStyle", StyleGetter<true>,
                 StyleSetter<true>);
  DefineAccessor(isolate, kContext, "strokeStyle", StyleGetter<false>,
                 StyleSetter<false>);
  for (size_t i = 0; i < arraysize(kStringStates); ++i) {
    DefineAccessor(
        isolate, kContext, kStringStates[i].name,
        [](const CallbackInfo& info) {
          info.GetReturnValue().Set(ToV8StringFromUtf8(
              info.GetIsolate(), Receiver<CanvasContext>(info)->state.*
                                     kStringStates[DataInt(info)].field));
        },
        [](const CallbackInfo& info) {
          std::string value;
          if (!ToStdString(info, info[0], &value)) return;
          Receiver<CanvasContext>(info)->state.*
              kStringStates[DataInt(info)].field = value;
        },
        Integer::New(isolate, static_cast<int>(i)));
  }
  for (size_t i = 0; i < arraysize(kNumberStates); ++i) {
    DefineAccessor(
        isolate, kContext, kNumberStates[i].name,
        [](const CallbackInfo& info) {
          info.GetReturnValue().Set(Receiver<CanvasContext>(info)->state.*
                                    kNumberStates[DataInt(info)].field);
        },
        [](const CallbackInfo& info) {
          double value;
          if (!ToNumber(info, info[0], &value)) return;
          if (!std::isfinite(value)) return;
          Receiver<CanvasContext>(info)->state.*
              kNumberStates[DataInt(info)].field = value;
        },
        Integer::New(isolate, static_cast<int>(i)));
  }
  // State with no effect here that pages still set and read back.
  static const struct {
    const char* name;
    const char* initial;
  } kStoredStrings[] = {
      {"direction", "ltr"},     {"filter", "none"},
      {"letterSpacing", "0px"}, {"wordSpacing", "0px"},
      {"fontKerning", "auto"},  {"imageSmoothingQuality", "low"}};
  for (const auto& stored : kStoredStrings) {
    DefineAccessor(
        isolate, kContext, stored.name,
        [](const CallbackInfo& info) {
          info.GetReturnValue().Set(info.Data().As<Value>());
        },
        [](const CallbackInfo& info) {}, V8Name(isolate, stored.initial));
  }
  DefineAccessor(
      isolate, kContext, "imageSmoothingEnabled",
      [](const CallbackInfo& info) { info.GetReturnValue().Set(true); },
      [](const CallbackInfo& info) {});

  DefineMethod(isolate, kContext, "save", [](const CallbackInfo& info) {
    CanvasContext* context = Receiver<CanvasContext>(info);
    State copy;
    copy.fill_style.Reset(info.GetIsolate(),
                          context->state.fill_style.Get(info.GetIsolate()));
    copy.stroke_style.Reset(info.GetIsolate(),
                            context->state.stroke_style.Get(info.GetIsolate()));
    copy.font = context->state.font;
    copy.text_align = context->state.text_align;
    copy.text_baseline = context->state.text_baseline;
    copy.line_cap = context->state.line_cap;
    copy.line_join = context->state.line_join;
    copy.global_composite_operation = context->state.global_composite_operation;
    copy.line_width = context->state.line_width;
    copy.miter_limit = context->state.miter_limit;
    copy.global_alpha = context->state.global_alpha;
    copy.line_dash_offset = context->state.line_dash_offset;
    copy.shadow_blur = context->state.shadow_blur;
    copy.shadow_offset_x = context->state.shadow_offset_x;
    copy.shadow_offset_y = context->state.shadow_offset_y;
    copy.shadow_color = context->state.shadow_color;
    copy.line_dash = context->state.line_dash;
    std::copy(std::begin(context->state.transform),
              std::end(context->state.transform), std::begin(copy.transform));
    context->saved_states.push_back(std::move(copy));
  });
  DefineMethod(isolate, kContext, "restore", [](const CallbackInfo& info) {
    CanvasContext* context = Receiver<CanvasContext>(info);
    if (context->saved_states.empty()) return;
    context->state = std::move(context->saved_states.back());
    context->saved_states.pop_back();
  });
  DefineMethod(isolate, kContext, "reset", [](const CallbackInfo& info) {
    CanvasContext* context = Receiver<CanvasContext>(info);
    context->state = State();
    context->saved_states.clear();
  });

  // Transforms.
  DefineMethod(
      isolate, kContext, "scale",
      [](const CallbackInfo& info) {
        double v[2];
        if (!NumberArguments(info, 2, v)) return;
        double m[6] = {v[0], 0, 0, v[1], 0, 0};
        Multiply(Receiver<CanvasContext>(info)->state.transform, m);
      },
      2);
  DefineMethod(
      isolate, kContext, "rotate",
      [](const CallbackInfo& info) {
        double angle;
        if (!NumberArguments(info, 1, &angle)) return;
        double m[6] = {std::cos(angle),
                       std::sin(angle),
                       -std::sin(angle),
                       std::cos(angle),
                       0,
                       0};
        Multiply(Receiver<CanvasContext>(info)->state.transform, m);
      },
      1);
  DefineMethod(
      isolate, kContext, "translate",
      [](const CallbackInfo& info) {
        double v[2];
        if (!NumberArguments(info, 2, v)) return;
        double m[6] = {1, 0, 0, 1, v[0], v[1]};
        Multiply(Receiver<CanvasContext>(info)->state.transform, m);
      },
      2);
  DefineMethod(
      isolate, kContext, "transform",
      [](const CallbackInfo& info) {
        double m[6];
        if (!NumberArguments(info, 6, m)) return;
        Multiply(Receiver<CanvasContext>(info)->state.transform, m);
      },
      6);
  DefineMethod(isolate, kContext, "setTransform", [](const CallbackInfo& info) {
    double m[6] = {1, 0, 0, 1, 0, 0};
    if (info.Length() > 0 && !TransformArguments(info, m)) return;
    std::copy(m, m + 6, Receiver<CanvasContext>(info)->state.transform);
  });
  DefineMethod(
      isolate, kContext, "resetTransform", [](const CallbackInfo& info) {
        double m[6] = {1, 0, 0, 1, 0, 0};
        std::copy(m, m + 6, Receiver<CanvasContext>(info)->state.transform);
      });
  DefineMethod(isolate, kContext, "getTransform", [](const CallbackInfo& info) {
    // A plain object with the 2D components of a DOMMatrix.
    Isolate* isolate = info.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    const double* m = Receiver<CanvasContext>(info)->state.transform;
    static const char* const kNames[] = {"a", "b", "c", "d", "e", "f"};
    Local<Object> matrix = Object::New(isolate);
    for (int i = 0; i < 6; ++i) {
      USE(matrix->Set(context, V8Name(isolate, kNames[i]),
                      Number::New(isolate, m[i])));
    }
    USE(matrix->Set(context, V8Name(isolate, "is2D"),
                    Boolean::New(isolate, true)));
    USE(matrix->Set(
        context, V8Name(isolate, "isIdentity"),
        Boolean::New(isolate, m[0] == 1 && m[1] == 0 && m[2] == 0 &&
                                  m[3] == 1 && m[4] == 0 && m[5] == 0)));
    info.GetReturnValue().Set(matrix);
  });

  DefineMethod(
      isolate, kContext, "setLineDash",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (info.Length() < 1 || !info[0]->IsArray()) {
          ThrowTypeError(isolate,
                         "The provided value cannot be converted to a "
                         "sequence.");
          return;
        }
        Local<Array> array = info[0].As<Array>();
        Local<Context> context = isolate->GetCurrentContext();
        std::vector<double> dash;
        for (uint32_t i = 0; i < array->Length(); ++i) {
          Local<Value> item;
          double value;
          if (!array->Get(context, i).ToLocal(&item) ||
              !ToNumber(info, item, &value)) {
            return;
          }
          if (!std::isfinite(value) || value < 0) return;
          dash.push_back(value);
        }
        if (dash.size() % 2 == 1) {
          std::vector<double> doubled = dash;
          doubled.insert(doubled.end(), dash.begin(), dash.end());
          dash = doubled;
        }
        Receiver<CanvasContext>(info)->state.line_dash = dash;
      },
      1);
  DefineMethod(isolate, kContext, "getLineDash", [](const CallbackInfo& info) {
    Isolate* isolate = info.GetIsolate();
    LocalVector<Value> values(isolate);
    for (double value : Receiver<CanvasContext>(info)->state.line_dash) {
      values.push_back(Number::New(isolate, value));
    }
    info.GetReturnValue().Set(
        Array::New(isolate, values.data(), values.size()));
  });
}

// --- Drawing -----------------------------------------------------------------

CALLBACK(CreateImageData) {
  Isolate* isolate = info.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  double width = 0, height = 0;
  // getImageData(sx, sy, sw, sh) or createImageData(sw, sh).
  int offset = DataInt(info);
  if (!RequireArguments(info, offset + 2) ||
      !ToNumber(info, info[offset], &width) ||
      !ToNumber(info, info[offset + 1], &height)) {
    return;
  }
  width = std::abs(std::floor(width));
  height = std::abs(std::floor(height));
  if (width == 0 || height == 0 || width * height > (1 << 26)) {
    ThrowDOMException(isolate, "IndexSizeError",
                      "The source width or height is zero or too large.");
    return;
  }
  size_t length = static_cast<size_t>(width * height * 4);
  Local<Object> image_data = Object::New(isolate);
  USE(image_data->Set(context, V8Name(isolate, "width"),
                      Number::New(isolate, width)));
  USE(image_data->Set(context, V8Name(isolate, "height"),
                      Number::New(isolate, height)));
  USE(image_data->Set(
      context, V8Name(isolate, "data"),
      Uint8ClampedArray::New(ArrayBuffer::New(isolate, length), 0, length)));
  info.GetReturnValue().Set(image_data);
}

void InstallDrawingBindings(Isolate* isolate) {
  const Interface kContext = Interface::kCanvasRenderingContext2D;
  // Path construction, also available on Path2D.
  static const char* const kPathMethods[] = {
      "moveTo", "lineTo",  "bezierCurveTo", "quadraticCurveTo", "arc",
      "arcTo",  "ellipse", "rect",          "roundRect",        "closePath"};
  for (const char* name : kPathMethods) {
    DefineMethod(isolate, kContext, name, Draw);
    DefineMethod(isolate, Interface::kPath2D, name, NoOp);
  }
  DefineMethod(isolate, Interface::kPath2D, "addPath", NoOp);
  SetConstructor(isolate, Interface::kPath2D, [](const CallbackInfo& info) {
    if (!RequireConstructCall(info)) return;
    WrapThis(info, MakeGarbageCollected<CanvasObject>(info.GetIsolate(),
                                                      Interface::kPath2D));
  });

  static const char* const kDrawMethods[] = {
      "beginPath",  "fill",       "stroke",       "clip",
      "fillRect",   "strokeRect", "clearRect",    "fillText",
      "strokeText", "drawImage",  "putImageData", "drawFocusIfNeeded"};
  for (const char* name : kDrawMethods) {
    DefineMethod(isolate, kContext, name, Draw);
  }
  for (const char* name : {"isPointInPath", "isPointInStroke"}) {
    DefineMethod(isolate, kContext, name, [](const CallbackInfo& info) {
      // Nothing is rasterized, so no point is ever inside a path.
      info.GetReturnValue().Set(false);
    });
  }

  DefineMethod(
      isolate, kContext, "measureText",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 1)) return;
        String text;
        if (!ToString(info, info[0], &text)) return;
        const std::string& font = Receiver<CanvasContext>(info)->state.font;
        Return(info, MakeGarbageCollected<TextMetrics>(
                         isolate, MeasureText(text, font), FontSize(font)));
      },
      1);
  struct Metric {
    const char* name;
    double (*get)(TextMetrics*);
  };
  static const Metric kMetrics[] = {
      {"width", [](TextMetrics* m) { return m->width; }},
      {"actualBoundingBoxLeft", [](TextMetrics*) { return 0.0; }},
      {"actualBoundingBoxRight", [](TextMetrics* m) { return m->width; }},
      {"actualBoundingBoxAscent",
       [](TextMetrics* m) { return m->font_size * 0.75; }},
      {"actualBoundingBoxDescent",
       [](TextMetrics* m) { return m->font_size * 0.2; }},
      {"fontBoundingBoxAscent",
       [](TextMetrics* m) { return m->font_size * 0.9; }},
      {"fontBoundingBoxDescent",
       [](TextMetrics* m) { return m->font_size * 0.25; }},
      {"emHeightAscent", [](TextMetrics* m) { return m->font_size * 0.8; }},
      {"emHeightDescent", [](TextMetrics* m) { return m->font_size * 0.2; }},
  };
  for (size_t i = 0; i < arraysize(kMetrics); ++i) {
    DefineAccessor(
        isolate, Interface::kTextMetrics, kMetrics[i].name,
        [](const CallbackInfo& info) {
          info.GetReturnValue().Set(
              kMetrics[DataInt(info)].get(Receiver<TextMetrics>(info)));
        },
        nullptr, Integer::New(isolate, static_cast<int>(i)));
  }

  for (const char* name : {"createLinearGradient", "createRadialGradient",
                           "createConicGradient"}) {
    DefineMethod(isolate, kContext, name, [](const CallbackInfo& info) {
      Return(info, MakeGarbageCollected<CanvasObject>(
                       info.GetIsolate(), Interface::kCanvasGradient));
    });
  }
  DefineMethod(isolate, Interface::kCanvasGradient, "addColorStop", NoOp, 2);
  DefineMethod(
      isolate, kContext, "createPattern",
      [](const CallbackInfo& info) {
        Return(info, MakeGarbageCollected<CanvasObject>(
                         info.GetIsolate(), Interface::kCanvasPattern));
      },
      2);
  DefineMethod(isolate, Interface::kCanvasPattern, "setTransform", NoOp);
  // All pixels read back as transparent black.
  DefineMethod(isolate, kContext, "getImageData", CreateImageData, 4,
               Integer::New(isolate, 2));
  DefineMethod(isolate, kContext, "createImageData", CreateImageData, 2,
               Integer::New(isolate, 0));
}

// --- HTMLCanvasElement -------------------------------------------------------

void InstallCanvasElementBindings(Isolate* isolate) {
  const Interface kCanvas = Interface::kHTMLCanvasElement;
  DefineMethod(
      isolate, kCanvas, "getContext",
      [](const CallbackInfo& info) {
        Isolate* isolate = info.GetIsolate();
        if (!RequireArguments(info, 1)) return;
        std::string type;
        if (!ToStdString(info, info[0], &type)) return;
        if (type != "2d") {
          // No WebGL, WebGPU or bitmaprenderer: like a browser without GPU
          // support, the context cannot be created.
          info.GetReturnValue().SetNull();
          return;
        }
        Element* canvas = Receiver<Element>(info);
        ElementRareData* rare = canvas->EnsureRareData(isolate);
        if (!rare->canvas_context) {
          rare->canvas_context =
              MakeGarbageCollected<CanvasContext>(isolate, canvas);
        }
        Return(info, rare->canvas_context.Get());
      },
      1);
  DefineMethod(isolate, kCanvas, "toDataURL", [](const CallbackInfo& info) {
    // The data URL of an empty image.
    Return(info, AtomicString("data:,"));
  });
  DefineMethod(isolate, kCanvas, "toBlob", [](const CallbackInfo& info) {
    Receiver<Element>(info)->document()->window()->ReportUnsupported(
        info.GetIsolate(), "canvas.toBlob()");
  });
}

#undef CALLBACK

}  // namespace

void InstallCanvasBindings(Isolate* isolate) {
  InstallStateBindings(isolate);
  InstallDrawingBindings(isolate);
  InstallCanvasElementBindings(isolate);
}

}  // namespace mock_dom
}  // namespace v8
