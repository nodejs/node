// Formatting library for C++ - color support
//
// Copyright (c) 2018 - present, Victor Zverovich and fmt contributors
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_COLOR_H_
#define FMT_COLOR_H_

#include "format.h"

FMT_BEGIN_NAMESPACE
FMT_BEGIN_EXPORT

enum class color : uint32_t {
  alice_blue = 0xF0F8FF,               // rgb(240,248,255)
  antique_white = 0xFAEBD7,            // rgb(250,235,215)
  aqua = 0x00FFFF,                     // rgb(0,255,255)
  aquamarine = 0x7FFFD4,               // rgb(127,255,212)
  azure = 0xF0FFFF,                    // rgb(240,255,255)
  beige = 0xF5F5DC,                    // rgb(245,245,220)
  bisque = 0xFFE4C4,                   // rgb(255,228,196)
  black = 0x000000,                    // rgb(0,0,0)
  blanched_almond = 0xFFEBCD,          // rgb(255,235,205)
  blue = 0x0000FF,                     // rgb(0,0,255)
  blue_violet = 0x8A2BE2,              // rgb(138,43,226)
  brown = 0xA52A2A,                    // rgb(165,42,42)
  burly_wood = 0xDEB887,               // rgb(222,184,135)
  cadet_blue = 0x5F9EA0,               // rgb(95,158,160)
  chartreuse = 0x7FFF00,               // rgb(127,255,0)
  chocolate = 0xD2691E,                // rgb(210,105,30)
  coral = 0xFF7F50,                    // rgb(255,127,80)
  cornflower_blue = 0x6495ED,          // rgb(100,149,237)
  cornsilk = 0xFFF8DC,                 // rgb(255,248,220)
  crimson = 0xDC143C,                  // rgb(220,20,60)
  cyan = 0x00FFFF,                     // rgb(0,255,255)
  dark_blue = 0x00008B,                // rgb(0,0,139)
  dark_cyan = 0x008B8B,                // rgb(0,139,139)
  dark_golden_rod = 0xB8860B,          // rgb(184,134,11)
  dark_gray = 0xA9A9A9,                // rgb(169,169,169)
  dark_green = 0x006400,               // rgb(0,100,0)
  dark_khaki = 0xBDB76B,               // rgb(189,183,107)
  dark_magenta = 0x8B008B,             // rgb(139,0,139)
  dark_olive_green = 0x556B2F,         // rgb(85,107,47)
  dark_orange = 0xFF8C00,              // rgb(255,140,0)
  dark_orchid = 0x9932CC,              // rgb(153,50,204)
  dark_red = 0x8B0000,                 // rgb(139,0,0)
  dark_salmon = 0xE9967A,              // rgb(233,150,122)
  dark_sea_green = 0x8FBC8F,           // rgb(143,188,143)
  dark_slate_blue = 0x483D8B,          // rgb(72,61,139)
  dark_slate_gray = 0x2F4F4F,          // rgb(47,79,79)
  dark_turquoise = 0x00CED1,           // rgb(0,206,209)
  dark_violet = 0x9400D3,              // rgb(148,0,211)
  deep_pink = 0xFF1493,                // rgb(255,20,147)
  deep_sky_blue = 0x00BFFF,            // rgb(0,191,255)
  dim_gray = 0x696969,                 // rgb(105,105,105)
  dodger_blue = 0x1E90FF,              // rgb(30,144,255)
  fire_brick = 0xB22222,               // rgb(178,34,34)
  floral_white = 0xFFFAF0,             // rgb(255,250,240)
  forest_green = 0x228B22,             // rgb(34,139,34)
  fuchsia = 0xFF00FF,                  // rgb(255,0,255)
  gainsboro = 0xDCDCDC,                // rgb(220,220,220)
  ghost_white = 0xF8F8FF,              // rgb(248,248,255)
  gold = 0xFFD700,                     // rgb(255,215,0)
  golden_rod = 0xDAA520,               // rgb(218,165,32)
  gray = 0x808080,                     // rgb(128,128,128)
  green = 0x008000,                    // rgb(0,128,0)
  green_yellow = 0xADFF2F,             // rgb(173,255,47)
  honey_dew = 0xF0FFF0,                // rgb(240,255,240)
  hot_pink = 0xFF69B4,                 // rgb(255,105,180)
  indian_red = 0xCD5C5C,               // rgb(205,92,92)
  indigo = 0x4B0082,                   // rgb(75,0,130)
  ivory = 0xFFFFF0,                    // rgb(255,255,240)
  khaki = 0xF0E68C,                    // rgb(240,230,140)
  lavender = 0xE6E6FA,                 // rgb(230,230,250)
  lavender_blush = 0xFFF0F5,           // rgb(255,240,245)
  lawn_green = 0x7CFC00,               // rgb(124,252,0)
  lemon_chiffon = 0xFFFACD,            // rgb(255,250,205)
  light_blue = 0xADD8E6,               // rgb(173,216,230)
  light_coral = 0xF08080,              // rgb(240,128,128)
  light_cyan = 0xE0FFFF,               // rgb(224,255,255)
  light_golden_rod_yellow = 0xFAFAD2,  // rgb(250,250,210)
  light_gray = 0xD3D3D3,               // rgb(211,211,211)
  light_green = 0x90EE90,              // rgb(144,238,144)
  light_pink = 0xFFB6C1,               // rgb(255,182,193)
  light_salmon = 0xFFA07A,             // rgb(255,160,122)
  light_sea_green = 0x20B2AA,          // rgb(32,178,170)
  light_sky_blue = 0x87CEFA,           // rgb(135,206,250)
  light_slate_gray = 0x778899,         // rgb(119,136,153)
  light_steel_blue = 0xB0C4DE,         // rgb(176,196,222)
  light_yellow = 0xFFFFE0,             // rgb(255,255,224)
  lime = 0x00FF00,                     // rgb(0,255,0)
  lime_green = 0x32CD32,               // rgb(50,205,50)
  linen = 0xFAF0E6,                    // rgb(250,240,230)
  magenta = 0xFF00FF,                  // rgb(255,0,255)
  maroon = 0x800000,                   // rgb(128,0,0)
  medium_aquamarine = 0x66CDAA,        // rgb(102,205,170)
  medium_blue = 0x0000CD,              // rgb(0,0,205)
  medium_orchid = 0xBA55D3,            // rgb(186,85,211)
  medium_purple = 0x9370DB,            // rgb(147,112,219)
  medium_sea_green = 0x3CB371,         // rgb(60,179,113)
  medium_slate_blue = 0x7B68EE,        // rgb(123,104,238)
  medium_spring_green = 0x00FA9A,      // rgb(0,250,154)
  medium_turquoise = 0x48D1CC,         // rgb(72,209,204)
  medium_violet_red = 0xC71585,        // rgb(199,21,133)
  midnight_blue = 0x191970,            // rgb(25,25,112)
  mint_cream = 0xF5FFFA,               // rgb(245,255,250)
  misty_rose = 0xFFE4E1,               // rgb(255,228,225)
  moccasin = 0xFFE4B5,                 // rgb(255,228,181)
  navajo_white = 0xFFDEAD,             // rgb(255,222,173)
  navy = 0x000080,                     // rgb(0,0,128)
  old_lace = 0xFDF5E6,                 // rgb(253,245,230)
  olive = 0x808000,                    // rgb(128,128,0)
  olive_drab = 0x6B8E23,               // rgb(107,142,35)
  orange = 0xFFA500,                   // rgb(255,165,0)
  orange_red = 0xFF4500,               // rgb(255,69,0)
  orchid = 0xDA70D6,                   // rgb(218,112,214)
  pale_golden_rod = 0xEEE8AA,          // rgb(238,232,170)
  pale_green = 0x98FB98,               // rgb(152,251,152)
  pale_turquoise = 0xAFEEEE,           // rgb(175,238,238)
  pale_violet_red = 0xDB7093,          // rgb(219,112,147)
  papaya_whip = 0xFFEFD5,              // rgb(255,239,213)
  peach_puff = 0xFFDAB9,               // rgb(255,218,185)
  peru = 0xCD853F,                     // rgb(205,133,63)
  pink = 0xFFC0CB,                     // rgb(255,192,203)
  plum = 0xDDA0DD,                     // rgb(221,160,221)
  powder_blue = 0xB0E0E6,              // rgb(176,224,230)
  purple = 0x800080,                   // rgb(128,0,128)
  rebecca_purple = 0x663399,           // rgb(102,51,153)
  red = 0xFF0000,                      // rgb(255,0,0)
  rosy_brown = 0xBC8F8F,               // rgb(188,143,143)
  royal_blue = 0x4169E1,               // rgb(65,105,225)
  saddle_brown = 0x8B4513,             // rgb(139,69,19)
  salmon = 0xFA8072,                   // rgb(250,128,114)
  sandy_brown = 0xF4A460,              // rgb(244,164,96)
  sea_green = 0x2E8B57,                // rgb(46,139,87)
  sea_shell = 0xFFF5EE,                // rgb(255,245,238)
  sienna = 0xA0522D,                   // rgb(160,82,45)
  silver = 0xC0C0C0,                   // rgb(192,192,192)
  sky_blue = 0x87CEEB,                 // rgb(135,206,235)
  slate_blue = 0x6A5ACD,               // rgb(106,90,205)
  slate_gray = 0x708090,               // rgb(112,128,144)
  snow = 0xFFFAFA,                     // rgb(255,250,250)
  spring_green = 0x00FF7F,             // rgb(0,255,127)
  steel_blue = 0x4682B4,               // rgb(70,130,180)
  tan = 0xD2B48C,                      // rgb(210,180,140)
  teal = 0x008080,                     // rgb(0,128,128)
  thistle = 0xD8BFD8,                  // rgb(216,191,216)
  tomato = 0xFF6347,                   // rgb(255,99,71)
  turquoise = 0x40E0D0,                // rgb(64,224,208)
  violet = 0xEE82EE,                   // rgb(238,130,238)
  wheat = 0xF5DEB3,                    // rgb(245,222,179)
  white = 0xFFFFFF,                    // rgb(255,255,255)
  white_smoke = 0xF5F5F5,              // rgb(245,245,245)
  yellow = 0xFFFF00,                   // rgb(255,255,0)
  yellow_green = 0x9ACD32              // rgb(154,205,50)
};                                     // enum class color

enum class terminal_color : uint8_t {
  black = 30,
  red,
  green,
  yellow,
  blue,
  magenta,
  cyan,
  white,
  bright_black = 90,
  bright_red,
  bright_green,
  bright_yellow,
  bright_blue,
  bright_magenta,
  bright_cyan,
  bright_white
};

enum class emphasis : uint8_t {
  bold = 1,
  faint = 1 << 1,
  italic = 1 << 2,
  underline = 1 << 3,
  blink = 1 << 4,
  reverse = 1 << 5,
  conceal = 1 << 6,
  strikethrough = 1 << 7,
};

// rgb is a struct for red, green and blue colors.
// Using the name "rgb" makes some editors show the color in a tooltip.
struct rgb {
  constexpr rgb() : r(0), g(0), b(0) {}
  constexpr rgb(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
  constexpr rgb(uint32_t hex)
      : r((hex >> 16) & 0xFF), g((hex >> 8) & 0xFF), b(hex & 0xFF) {}
  constexpr rgb(color hex)
      : r((uint32_t(hex) >> 16) & 0xFF),
        g((uint32_t(hex) >> 8) & 0xFF),
        b(uint32_t(hex) & 0xFF) {}
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

namespace detail {

// A bit-packed variant of an RGB color, a terminal color, or unset color.
// see text_style for the bit-packing scheme.
struct color_type {
  constexpr color_type() noexcept = default;
  constexpr color_type(color rgb_color) noexcept
      : value_(static_cast<uint32_t>(rgb_color) | (1 << 24)) {}
  constexpr color_type(rgb rgb_color) noexcept
      : color_type(static_cast<color>(
            (static_cast<uint32_t>(rgb_color.r) << 16) |
            (static_cast<uint32_t>(rgb_color.g) << 8) | rgb_color.b)) {}
  constexpr color_type(terminal_color term_color) noexcept
      : value_(static_cast<uint32_t>(term_color) | (3 << 24)) {}

  constexpr auto is_terminal_color() const noexcept -> bool {
    return (value_ & (1 << 25)) != 0;
  }

  constexpr auto value() const noexcept -> uint32_t {
    return value_ & 0xFFFFFF;
  }

  constexpr color_type(uint32_t value) noexcept : value_(value) {}

  uint32_t value_ = 0;
};
}  // namespace detail

/// A text style consisting of foreground and background colors and emphasis.
class text_style {
  // The information is packed as follows:
  // ┌──┐
  // │ 0│─┐
  // │..│ ├── foreground color value
  // │23│─┘
  // ├──┤
  // │24│─┬── discriminator for the above value. 00 if unset, 01 if it's
  // │25│─┘   an RGB color, or 11 if it's a terminal color (10 is unused)
  // ├──┤
  // │26│──── overflow bit, always zero (see below)
  // ├──┤
  // │27│─┐
  // │..│ │
  // │50│ │
  // ├──┤ │
  // │51│ ├── background color (same format as the foreground color)
  // │52│ │
  // ├──┤ │
  // │53│─┘
  // ├──┤
  // │54│─┐
  // │..│ ├── emphases
  // │61│─┘
  // ├──┤
  // │62│─┬── unused
  // │63│─┘
  // └──┘
  // The overflow bits are there to make operator|= efficient.
  // When ORing, we must throw if, for either the foreground or background,
  // one style specifies a terminal color and the other specifies any color
  // (terminal or RGB); in other words, if one discriminator is 11 and the
  // other is 11 or 01.
  //
  // We do that check by adding the styles. Consider what adding does to each
  // possible pair of discriminators:
  //    00 + 00 = 000
  //    01 + 00 = 001
  //    11 + 00 = 011
  //    01 + 01 = 010
  //    11 + 01 = 100 (!!)
  //    11 + 11 = 110 (!!)
  // In the last two cases, the ones we want to catch, the third bit——the
  // overflow bit——is set. Bingo.
  //
  // We must take into account the possible carry bit from the bits
  // before the discriminator. The only potentially problematic case is
  // 11 + 00 = 011 (a carry bit would make it 100, not good!), but a carry
  // bit is impossible in that case, because 00 (unset color) means the
  // 24 bits that precede the discriminator are all zero.
  //
  // This test can be applied to both colors simultaneously.

 public:
  FMT_CONSTEXPR text_style(emphasis em = emphasis()) noexcept
      : style_(static_cast<uint64_t>(em) << 54) {}

  FMT_CONSTEXPR auto operator|=(text_style rhs) -> text_style& {
    if (((style_ + rhs.style_) & ((1ULL << 26) | (1ULL << 53))) != 0)
      report_error("can't OR a terminal color");
    style_ |= rhs.style_;
    return *this;
  }

  friend FMT_CONSTEXPR auto operator|(text_style lhs, text_style rhs)
      -> text_style {
    return lhs |= rhs;
  }

  FMT_CONSTEXPR auto operator==(text_style rhs) const noexcept -> bool {
    return style_ == rhs.style_;
  }

  FMT_CONSTEXPR auto operator!=(text_style rhs) const noexcept -> bool {
    return !(*this == rhs);
  }

  FMT_CONSTEXPR auto has_foreground() const noexcept -> bool {
    return (style_ & (1 << 24)) != 0;
  }
  FMT_CONSTEXPR auto has_background() const noexcept -> bool {
    return (style_ & (1ULL << 51)) != 0;
  }
  FMT_CONSTEXPR auto has_emphasis() const noexcept -> bool {
    return (style_ >> 54) != 0;
  }
  FMT_CONSTEXPR auto get_foreground() const noexcept -> detail::color_type {
    FMT_ASSERT(has_foreground(), "no foreground specified for this style");
    return style_ & 0x3FFFFFF;
  }
  FMT_CONSTEXPR auto get_background() const noexcept -> detail::color_type {
    FMT_ASSERT(has_background(), "no background specified for this style");
    return (style_ >> 27) & 0x3FFFFFF;
  }
  FMT_CONSTEXPR auto get_emphasis() const noexcept -> emphasis {
    FMT_ASSERT(has_emphasis(), "no emphasis specified for this style");
    return static_cast<emphasis>(style_ >> 54);
  }

 private:
  FMT_CONSTEXPR text_style(uint64_t style) noexcept : style_(style) {}

  friend FMT_CONSTEXPR auto fg(detail::color_type foreground) noexcept
      -> text_style;

  friend FMT_CONSTEXPR auto bg(detail::color_type background) noexcept
      -> text_style;

  uint64_t style_ = 0;
};

/// Creates a text style from the foreground (text) color.
FMT_CONSTEXPR inline auto fg(detail::color_type foreground) noexcept
    -> text_style {
  return foreground.value_;
}

/// Creates a text style from the background color.
FMT_CONSTEXPR inline auto bg(detail::color_type background) noexcept
    -> text_style {
  return static_cast<uint64_t>(background.value_) << 27;
}

FMT_CONSTEXPR inline auto operator|(emphasis lhs, emphasis rhs) noexcept
    -> text_style {
  return text_style(lhs) | rhs;
}

namespace detail {

template <typename Char> struct ansi_color_escape {
  FMT_CONSTEXPR ansi_color_escape(color_type text_color,
                                  const char* esc) noexcept {
    // If we have a terminal color, we need to output another escape code
    // sequence.
    if (text_color.is_terminal_color()) {
      bool is_background = esc == string_view("\x1b[48;2;");
      uint32_t value = text_color.value();
      // Background ASCII codes are the same as the foreground ones but with
      // 10 more.
      if (is_background) value += 10u;

      size_t index = 0;
      buffer[index++] = static_cast<Char>('\x1b');
      buffer[index++] = static_cast<Char>('[');

      if (value >= 100u) {
        buffer[index++] = static_cast<Char>('1');
        value %= 100u;
      }
      buffer[index++] = static_cast<Char>('0' + value / 10u);
      buffer[index++] = static_cast<Char>('0' + value % 10u);

      buffer[index++] = static_cast<Char>('m');
      buffer[index++] = static_cast<Char>('\0');
      return;
    }

    for (int i = 0; i < 7; i++) {
      buffer[i] = static_cast<Char>(esc[i]);
    }
    rgb color(text_color.value());
    to_esc(color.r, buffer + 7, ';');
    to_esc(color.g, buffer + 11, ';');
    to_esc(color.b, buffer + 15, 'm');
    buffer[19] = static_cast<Char>(0);
  }
  FMT_CONSTEXPR ansi_color_escape(emphasis em) noexcept {
    uint8_t em_codes[num_emphases] = {};
    if (has_emphasis(em, emphasis::bold)) em_codes[0] = 1;
    if (has_emphasis(em, emphasis::faint)) em_codes[1] = 2;
    if (has_emphasis(em, emphasis::italic)) em_codes[2] = 3;
    if (has_emphasis(em, emphasis::underline)) em_codes[3] = 4;
    if (has_emphasis(em, emphasis::blink)) em_codes[4] = 5;
    if (has_emphasis(em, emphasis::reverse)) em_codes[5] = 7;
    if (has_emphasis(em, emphasis::conceal)) em_codes[6] = 8;
    if (has_emphasis(em, emphasis::strikethrough)) em_codes[7] = 9;

    size_t index = 0;
    for (size_t i = 0; i < num_emphases; ++i) {
      if (!em_codes[i]) continue;
      buffer[index++] = static_cast<Char>('\x1b');
      buffer[index++] = static_cast<Char>('[');
      buffer[index++] = static_cast<Char>('0' + em_codes[i]);
      buffer[index++] = static_cast<Char>('m');
    }
    buffer[index++] = static_cast<Char>(0);
  }
  FMT_CONSTEXPR operator const Char*() const noexcept { return buffer; }

  FMT_CONSTEXPR auto begin() const noexcept -> const Char* { return buffer; }
  FMT_CONSTEXPR20 auto end() const noexcept -> const Char* {
    return buffer + basic_string_view<Char>(buffer).size();
  }

 private:
  static constexpr size_t num_emphases = 8;
  Char buffer[7u + 3u * num_emphases + 1u];

  static FMT_CONSTEXPR void to_esc(uint8_t c, Char* out,
                                   char delimiter) noexcept {
    out[0] = static_cast<Char>('0' + c / 100);
    out[1] = static_cast<Char>('0' + c / 10 % 10);
    out[2] = static_cast<Char>('0' + c % 10);
    out[3] = static_cast<Char>(delimiter);
  }
  static FMT_CONSTEXPR auto has_emphasis(emphasis em, emphasis mask) noexcept
      -> bool {
    return static_cast<uint8_t>(em) & static_cast<uint8_t>(mask);
  }
};

template <typename Char>
FMT_CONSTEXPR auto make_foreground_color(color_type foreground) noexcept
    -> ansi_color_escape<Char> {
  return ansi_color_escape<Char>(foreground, "\x1b[38;2;");
}

template <typename Char>
FMT_CONSTEXPR auto make_background_color(color_type background) noexcept
    -> ansi_color_escape<Char> {
  return ansi_color_escape<Char>(background, "\x1b[48;2;");
}

template <typename Char>
FMT_CONSTEXPR auto make_emphasis(emphasis em) noexcept
    -> ansi_color_escape<Char> {
  return ansi_color_escape<Char>(em);
}

template <typename Char> inline void reset_color(buffer<Char>& buffer) {
  auto reset_color = string_view("\x1b[0m");
  buffer.append(reset_color.begin(), reset_color.end());
}

template <typename T> struct styled_arg : view {
  const T& value;
  text_style style;
  styled_arg(const T& v, text_style s) : value(v), style(s) {}
};

template <typename Char>
void vformat_to(buffer<Char>& buf, text_style ts, basic_string_view<Char> fmt,
                basic_format_args<buffered_context<Char>> args) {
  if (ts.has_emphasis()) {
    auto emphasis = make_emphasis<Char>(ts.get_emphasis());
    buf.append(emphasis.begin(), emphasis.end());
  }
  if (ts.has_foreground()) {
    auto foreground = make_foreground_color<Char>(ts.get_foreground());
    buf.append(foreground.begin(), foreground.end());
  }
  if (ts.has_background()) {
    auto background = make_background_color<Char>(ts.get_background());
    buf.append(background.begin(), background.end());
  }
  vformat_to(buf, fmt, args);
  if (ts != text_style()) reset_color<Char>(buf);
}
}  // namespace detail

inline void vprint(FILE* f, text_style ts, string_view fmt, format_args args) {
  auto buf = memory_buffer();
  detail::vformat_to(buf, ts, fmt, args);
  print(f, FMT_STRING("{}"), string_view(buf.begin(), buf.size()));
}

/**
 * Formats a string and prints it to the specified file stream using ANSI
 * escape sequences to specify text formatting.
 *
 * **Example**:
 *
 *     fmt::print(fmt::emphasis::bold | fg(fmt::color::red),
 *                "Elapsed time: {0:.2f} seconds", 1.23);
 */
template <typename... T>
void print(FILE* f, text_style ts, format_string<T...> fmt, T&&... args) {
  vprint(f, ts, fmt.str, vargs<T...>{{args...}});
}

/**
 * Formats a string and prints it to stdout using ANSI escape sequences to
 * specify text formatting.
 *
 * **Example**:
 *
 *     fmt::print(fmt::emphasis::bold | fg(fmt::color::red),
 *                "Elapsed time: {0:.2f} seconds", 1.23);
 */
template <typename... T>
void print(text_style ts, format_string<T...> fmt, T&&... args) {
  return print(stdout, ts, fmt, std::forward<T>(args)...);
}

inline auto vformat(text_style ts, string_view fmt, format_args args)
    -> std::string {
  auto buf = memory_buffer();
  detail::vformat_to(buf, ts, fmt, args);
  return fmt::to_string(buf);
}

/**
 * Formats arguments and returns the result as a string using ANSI escape
 * sequences to specify text formatting.
 *
 * **Example**:
 *
 * ```
 * #include <fmt/color.h>
 * std::string message = fmt::format(fmt::emphasis::bold | fg(fmt::color::red),
 *                                   "The answer is {}", 42);
 * ```
 */
template <typename... T>
inline auto format(text_style ts, format_string<T...> fmt, T&&... args)
    -> std::string {
  return fmt::vformat(ts, fmt.str, vargs<T...>{{args...}});
}

/// Formats a string with the given text_style and writes the output to `out`.
template <typename OutputIt,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value)>
auto vformat_to(OutputIt out, text_style ts, string_view fmt, format_args args)
    -> OutputIt {
  auto&& buf = detail::get_buffer<char>(out);
  detail::vformat_to(buf, ts, fmt, args);
  return detail::get_iterator(buf, out);
}

/**
 * Formats arguments with the given text style, writes the result to the output
 * iterator `out` and returns the iterator past the end of the output range.
 *
 * **Example**:
 *
 *     std::vector<char> out;
 *     fmt::format_to(std::back_inserter(out),
 *                    fmt::emphasis::bold | fg(fmt::color::red), "{}", 42);
 */
template <typename OutputIt, typename... T,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value)>
inline auto format_to(OutputIt out, text_style ts, format_string<T...> fmt,
                      T&&... args) -> OutputIt {
  return vformat_to(out, ts, fmt.str, vargs<T...>{{args...}});
}

template <typename T, typename Char>
struct formatter<detail::styled_arg<T>, Char> : formatter<T, Char> {
  template <typename FormatContext>
  auto format(const detail::styled_arg<T>& arg, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    const auto& ts = arg.style;
    auto out = ctx.out();

    bool has_style = false;
    if (ts.has_emphasis()) {
      has_style = true;
      auto emphasis = detail::make_emphasis<Char>(ts.get_emphasis());
      out = detail::copy<Char>(emphasis.begin(), emphasis.end(), out);
    }
    if (ts.has_foreground()) {
      has_style = true;
      auto foreground =
          detail::make_foreground_color<Char>(ts.get_foreground());
      out = detail::copy<Char>(foreground.begin(), foreground.end(), out);
    }
    if (ts.has_background()) {
      has_style = true;
      auto background =
          detail::make_background_color<Char>(ts.get_background());
      out = detail::copy<Char>(background.begin(), background.end(), out);
    }
    out = formatter<T, Char>::format(arg.value, ctx);
    if (has_style) {
      auto reset_color = string_view("\x1b[0m");
      out = detail::copy<Char>(reset_color.begin(), reset_color.end(), out);
    }
    return out;
  }
};

/**
 * Returns an argument that will be formatted using ANSI escape sequences,
 * to be used in a formatting function.
 *
 * **Example**:
 *
 *     fmt::print("Elapsed time: {0:.2f} seconds",
 *                fmt::styled(1.23, fmt::fg(fmt::color::green) |
 *                                  fmt::bg(fmt::color::blue)));
 */
template <typename T>
FMT_CONSTEXPR auto styled(const T& value, text_style ts)
    -> detail::styled_arg<remove_cvref_t<T>> {
  return detail::styled_arg<remove_cvref_t<T>>{value, ts};
}

FMT_END_EXPORT
FMT_END_NAMESPACE

#endif  // FMT_COLOR_H_
