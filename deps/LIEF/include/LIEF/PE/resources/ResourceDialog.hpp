/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_PE_RESOURCE_DIALOG_H
#define LIEF_PE_RESOURCE_DIALOG_H

#include "LIEF/visibility.h"
#include "LIEF/Object.hpp"
#include "LIEF/enums.hpp"
#include "LIEF/errors.hpp"
#include "LIEF/span.hpp"

#include <memory>
#include <vector>
#include <ostream>

namespace LIEF {
class BinaryStream;
namespace PE {

class ResourceData;

/// This class is the base class for either a regular (legacy) Dialog or
/// an extended Dialog. These different kinds of Dialogs are documented by MS
/// at the following addresses:
///
/// - https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-dlgtemplate
/// - https://learn.microsoft.com/fr-fr/windows/win32/dlgbox/dlgitemtemplateex
class LIEF_API ResourceDialog : public Object {
  public:
  using dialogs_t = std::vector<std::unique_ptr<ResourceDialog>>;

  /// This structure wraps either an ordinal value (`uint16_t`) or a string.
  /// The ordinal value should refer to an existing resource id in the resource
  /// tree.
  struct ordinal_or_str_t {
    result<uint16_t> ordinal = make_error_code(lief_errors::not_found);
    std::u16string string;

    bool is_defined() const {
      return ordinal || !string.empty();
    }

    operator bool() const {
      return is_defined();
    }

    std::string to_string() const;
  };

  /// Enum for discriminating the kind of the Dialog (regular vs extended)
  enum class TYPE {
    UNKNOWN = 0,
    REGULAR, EXTENDED,
  };

  /// From: https://learn.microsoft.com/en-us/windows/win32/dlgbox/dialog-box-styles
  enum class DIALOG_STYLES : uint32_t {
    ABSALIGN      = 0x0001,
    SYSMODAL      = 0x0002,
    LOCALEDIT     = 0x0020,
    SETFONT       = 0x0040,
    MODALFRAME    = 0x0080,
    NOIDLEMSG     = 0x0100,
    SETFOREGROUND = 0x0200,
    S3DLOOK       = 0x0004,
    FIXEDSYS      = 0x0008,
    NOFAILCREATE  = 0x0010,
    CONTROL       = 0x0400,
    CENTER        = 0x0800,
    CENTERMOUSE   = 0x1000,
    CONTEXTHELP   = 0x2000,
    SHELLFONT     = SETFONT | FIXEDSYS,
  };

  /// From: https://docs.microsoft.com/en-us/windows/win32/winmsg/window-styles
  enum class WINDOW_STYLES : uint32_t {
    OVERLAPPED      = 0x00000000,
    POPUP           = 0x80000000,
    CHILD           = 0x40000000,
    MINIMIZE        = 0x20000000,
    VISIBLE         = 0x10000000,
    DISABLED        = 0x08000000,
    CLIPSIBLINGS    = 0x04000000,
    CLIPCHILDREN    = 0x02000000,
    MAXIMIZE        = 0x01000000,
    CAPTION         = 0x00C00000,
    BORDER          = 0x00800000,
    DLGFRAME        = 0x00400000,
    VSCROLL         = 0x00200000,
    HSCROLL         = 0x00100000,
    SYSMENU         = 0x00080000,
    THICKFRAME      = 0x00040000,
    GROUP           = 0x00020000,
    TABSTOP         = 0x00010000,
  };

  /// From https://docs.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles
  enum class WINDOW_EXTENDED_STYLES : uint32_t {
    DLGMODALFRAME    = 0x00000001,
    NOPARENTNOTIFY   = 0x00000004,
    TOPMOST          = 0x00000008,
    ACCEPTFILES      = 0x00000010,
    TRANSPARENT_STY  = 0x00000020,
    MDICHILD         = 0x00000040,
    TOOLWINDOW       = 0x00000080,
    WINDOWEDGE       = 0x00000100,
    CLIENTEDGE       = 0x00000200,
    CONTEXTHELP      = 0x00000400,

    RIGHT            = 0x00001000,
    LEFT             = 0x00000000,
    RTLREADING       = 0x00002000,
    LEFTSCROLLBAR    = 0x00004000,

    CONTROLPARENT    = 0x00010000,
    STATICEDGE       = 0x00020000,
    APPWINDOW        = 0x00040000,
  };

  /// From: https://learn.microsoft.com/en-us/windows/win32/controls/common-control-styles
  enum class CONTROL_STYLES : uint32_t {
    TOP            = 0x00000001,
    NOMOVEY        = 0x00000002,
    BOTTOM         = 0x00000003,
    NORESIZE       = 0x00000004,
    NOPARENTALIGN  = 0x00000008,
    ADJUSTABLE     = 0x00000020,
    NODIVIDER      = 0x00000040,
    VERT           = 0x00000080,
    LEFT           = VERT | TOP,
    RIGHT          = VERT | BOTTOM,
    NOMOVEX        = VERT | NOMOVEY
  };

  /// This class represents an element of the dialog. It can be for instance,
  /// a button, or a caption.
  ///
  /// This class is inherited by the regular or extended dialog's item:
  /// - ResourceDialogRegular::Item
  /// - ResourceDialogExtended::Item
  class LIEF_API Item {
    public:
    Item() = default;
    Item(const Item&) = default;
    Item& operator=(const Item&) = default;

    Item(Item&&) = default;
    Item& operator=(Item&&) = default;

    enum class WINDOW_CLASS : uint32_t {
      BUTTON     = 0x0080,
      EDIT       = 0x0081,
      STATIC     = 0x0082,
      LIST_BOX   = 0x0083,
      SCROLL_BAR = 0x0084,
      COMBO_BOX  = 0x0085,
    };

    /// The style of the control. This can be a combination of WINDOW_STYLES or
    /// CONTROL_STYLES.
    uint32_t style() const {
      return style_;
    }

    /// The extended styles for a window. This member is not used to create
    /// controls in dialog boxes, but applications that use dialog box templates
    /// can use it to create other types of windows.
    ///
    /// It can take a combination of WINDOW_EXTENDED_STYLES
    uint32_t extended_style() const {
      return extended_style_;
    }

    /// The control identifier.
    int32_t id() const {
      return id_;
    }

    /// Check if this item has the given WINDOW_STYLES
    bool has(WINDOW_STYLES style) const {
      return (style_ & (uint32_t)style) != 0;
    }

    /// Check if this item has the given CONTROL_STYLES
    bool has(CONTROL_STYLES style) const {
      return (style_ & (uint32_t)style) != 0;
    }

    /// List of WINDOW_STYLES used by this item
    std::vector<WINDOW_STYLES> window_styles() const;

    /// List of CONTROL_STYLES used by this item
    std::vector<CONTROL_STYLES> control_styles() const;

    /// The x-coordinate, in dialog box units, of the upper-left corner of the
    /// control. This coordinate is always relative to the upper-left corner of
    /// the dialog box's client area.
    int16_t x() const { return x_; }

    /// The y-coordinate, in dialog box units, of the upper-left corner of the
    /// control. This coordinate is always relative to the upper-left corner of
    /// the dialog box's client area.
    int16_t y() const { return y_; }

    /// The width, in dialog box units, of the control.
    int16_t cx() const { return cx_; }

    /// The height, in dialog box units, of the control.
    int16_t cy() const { return cy_; }

    Item& style(uint32_t value) {
      style_ = value;
      return *this;
    }

    Item& extended_style(uint32_t value) {
      extended_style_ = value;
      return *this;
    }

    Item& x(int16_t value) { x_ = value; return *this; }
    Item& y(int16_t value) { y_ = value; return *this; }
    Item& cx(int16_t value) { cx_ = value; return *this; }
    Item& cy(int16_t value) { cy_ = value; return *this; }

    Item& id(int32_t value) { id_ = value; return *this; }

    Item& data(std::vector<uint8_t> creation_data) {
      creation_data_ = std::move(creation_data);
      return *this;
    }

    Item& clazz(std::u16string title) {
      class_.string = std::move(title);
      class_.ordinal = make_error_code(lief_errors::not_found);
      return *this;
    }

    Item& clazz(uint16_t ord) {
      class_.ordinal = ord;
      return *this;
    }

    Item& title(std::u16string value) {
      title_.string = std::move(value);
      title_.ordinal = make_error_code(lief_errors::not_found);
      return *this;
    }

    Item& title(uint16_t ord) {
      title_.ordinal = ord;
      return *this;
    }

    /// Window class of the control. This can be either: a string that specifies
    /// the name of a registered window class or an ordinal value of a predefined
    /// system class.
    const ordinal_or_str_t& clazz() const {
      return class_;
    }

    /// Title of the item which can be either: a string that specifies the
    /// initial text or an ordinal value of a resource, such as an icon, in an
    /// executable file
    const ordinal_or_str_t& title() const {
      return title_;
    }

    /// Creation data that is passed to the control's window procedure
    span<const uint8_t> creation_data() const {
      return creation_data_;
    }

    span<uint8_t> creation_data() {
      return creation_data_;
    }

    virtual ~Item() = default;

    virtual std::string to_string() const = 0;

    LIEF_API friend std::ostream& operator<<(std::ostream& os, const Item& item) {
      os << item.to_string();
      return os;
    }

    protected:
    uint32_t style_ = 0;
    uint32_t extended_style_ = 0;

    int16_t x_ = 0;
    int16_t y_ = 0;
    int16_t cx_ = 0;
    int16_t cy_ = 0;

    int32_t id_ = 0;

    ordinal_or_str_t class_;
    ordinal_or_str_t title_;
    std::vector<uint8_t> creation_data_;
  };

  /// Parse dialogs from the given resource data node
  static dialogs_t parse(const ResourceData& node);
  static dialogs_t parse(const uint8_t* buffer, size_t size);

  ResourceDialog() = default;
  ResourceDialog(const ResourceDialog&) = default;
  ResourceDialog& operator=(const ResourceDialog&) = default;

  ResourceDialog(ResourceDialog&&) = default;
  ResourceDialog& operator=(ResourceDialog&&) = default;

  ResourceDialog(TYPE ty) :
    type_(ty)
  {}

  virtual std::unique_ptr<ResourceDialog> clone() const = 0;

  TYPE type() const {
    return type_;
  }

  /// The style of the dialog box. This member can be a combination of window
  /// style values (such as WINDOW_STYLES::CAPTION and WINDOW_STYLES::SYSMENU)
  /// and dialog box style values (such as DIALOG_STYLES::CENTER).
  uint32_t style() const {
    return style_;
  }

  /// The extended styles for a window. This member is not used to create dialog
  /// boxes, but applications that use dialog box templates can use it to create
  /// other types of windows. For a list of values, see WINDOW_EXTENDED_STYLES
  uint32_t extended_style() const {
    return extended_style_;
  }

  /// The x-coordinate, in dialog box units, of the upper-left corner of the
  /// dialog box.
  int16_t x() const { return x_; }

  /// The y-coordinate, in dialog box units, of the upper-left corner of the
  /// dialog box.
  int16_t y() const { return y_; }

  /// The width, in dialog box units, of the dialog box.
  int16_t cx() const { return cx_; }

  /// The height, in dialog box units, of the dialog box.
  int16_t cy() const { return cy_; }

  ResourceDialog& style(uint32_t value) {
    style_ = value;
    return *this;
  }

  ResourceDialog& extended_style(uint32_t value) {
    extended_style_ = value;
    return *this;
  }

  ResourceDialog& x(int16_t value) { x_ = value; return *this; }
  ResourceDialog& y(int16_t value) { y_ = value; return *this; }
  ResourceDialog& cx(int16_t value) { cx_ = value; return *this; }
  ResourceDialog& cy(int16_t value) { cy_ = value; return *this; }

  ResourceDialog& menu(std::u16string title) {
    menu_.string = std::move(title);
    menu_.ordinal = make_error_code(lief_errors::not_found);
    return *this;
  }

  ResourceDialog& menu(uint16_t ord) {
    menu_.ordinal = ord;
    return *this;
  }

  ResourceDialog& window_class(std::u16string title) {
    window_class_.string = std::move(title);
    window_class_.ordinal = make_error_code(lief_errors::not_found);
    return *this;
  }

  ResourceDialog& window_class(uint16_t ord) {
    window_class_.ordinal = ord;
    return *this;
  }

  ResourceDialog& title(std::u16string value) {
    title_ = std::move(value);
    return *this;
  }

  ResourceDialog& title(const std::string& title);

  /// Check if the dialog used to given dialog style
  bool has(DIALOG_STYLES style) const {
    return (style_ & (uint32_t)style) != 0;
  }

  /// Check if the dialog used to given window style
  bool has(WINDOW_STYLES style) const {
    return (style_ & (uint32_t)style) != 0;
  }

  /// Check if the dialog used to given extended window style
  bool has(WINDOW_EXTENDED_STYLES style) const {
    return (extended_style_ & (uint32_t)style) != 0;
  }

  /// List of DIALOG_STYLES used by this dialog
  std::vector<DIALOG_STYLES> styles_list() const;

  /// List of WINDOW_STYLES used by this dialog
  std::vector<WINDOW_STYLES> windows_styles_list() const;

  /// List of WINDOW_EXTENDED_STYLES used by this dialog
  std::vector<WINDOW_EXTENDED_STYLES> windows_ext_styles_list() const;

  /// title of the dialog box
  const std::u16string& title() const {
    return title_;
  }

  /// title of the dialog box
  std::string title_utf8() const;

  /// ordinal or name value of a menu resource.
  const ordinal_or_str_t& menu() const {
    return menu_;
  }

  /// ordinal of a predefined system window class or name of a registered
  /// window class
  const ordinal_or_str_t& window_class() const {
    return window_class_;
  }

  virtual std::string to_string() const = 0;

  virtual ~ResourceDialog() = default;


  /// Helper to **downcast** a ResourceDialog into a ResourceDialogRegular or
  /// a ResourceDialogExtended.
  template<class T>
  const T* as() const {
    static_assert(std::is_base_of<ResourceDialog, T>::value,
                  "Require ResourceDialog inheritance");
    if (T::classof(this)) {
      return static_cast<const T*>(this);
    }
    return nullptr;
  }

  friend LIEF_API std::ostream& operator<<(std::ostream& os, const ResourceDialog& dialog) {
    os << dialog.to_string();
    return os;
  }

  protected:
  static ok_error_t parse_menu(ResourceDialog& dialog, BinaryStream& stream);
  static ok_error_t parse_class(ResourceDialog& dialog, BinaryStream& stream);
  static ok_error_t parse_class(ResourceDialog::Item& dialog, BinaryStream& stream);
  static ok_error_t parse_title(ResourceDialog& dialog, BinaryStream& stream);
  static ok_error_t parse_title(ResourceDialog::Item& dialog, BinaryStream& stream);
  static ok_error_t parse_creation_data(ResourceDialog::Item& item, BinaryStream& stream);
  TYPE type_ = TYPE::UNKNOWN;

  uint32_t style_ = 0;
  uint32_t extended_style_ = 0;

  int16_t x_ = 0;
  int16_t y_ = 0;
  int16_t cx_ = 0;
  int16_t cy_ = 0;

  ordinal_or_str_t menu_;
  ordinal_or_str_t window_class_;

  std::u16string title_;
};

LIEF_API const char* to_string(ResourceDialog::DIALOG_STYLES s);
LIEF_API const char* to_string(ResourceDialog::WINDOW_STYLES s);
LIEF_API const char* to_string(ResourceDialog::WINDOW_EXTENDED_STYLES s);
LIEF_API const char* to_string(ResourceDialog::CONTROL_STYLES s);

}
}

ENABLE_BITMASK_OPERATORS(LIEF::PE::ResourceDialog::DIALOG_STYLES);
ENABLE_BITMASK_OPERATORS(LIEF::PE::ResourceDialog::WINDOW_STYLES);
ENABLE_BITMASK_OPERATORS(LIEF::PE::ResourceDialog::WINDOW_EXTENDED_STYLES);
ENABLE_BITMASK_OPERATORS(LIEF::PE::ResourceDialog::CONTROL_STYLES);


#endif
