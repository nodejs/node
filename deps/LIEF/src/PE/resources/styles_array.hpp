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
#ifndef LIEF_PE_RESOURCES_STYLE_ARRAY_H
#define LIEF_PE_RESOURCES_STYLE_ARRAY_H
#include "LIEF/PE/resources/ResourceDialog.hpp"

namespace LIEF::PE {
static constexpr auto DIALOG_STYLES_ARRAY = {
  ResourceDialog::DIALOG_STYLES::ABSALIGN,
  ResourceDialog::DIALOG_STYLES::SYSMODAL,
  ResourceDialog::DIALOG_STYLES::LOCALEDIT,
  ResourceDialog::DIALOG_STYLES::SETFONT,
  ResourceDialog::DIALOG_STYLES::MODALFRAME,
  ResourceDialog::DIALOG_STYLES::NOIDLEMSG,
  ResourceDialog::DIALOG_STYLES::SETFOREGROUND,
  ResourceDialog::DIALOG_STYLES::S3DLOOK,
  ResourceDialog::DIALOG_STYLES::FIXEDSYS,
  ResourceDialog::DIALOG_STYLES::NOFAILCREATE,
  ResourceDialog::DIALOG_STYLES::CONTROL,
  ResourceDialog::DIALOG_STYLES::CENTER,
  ResourceDialog::DIALOG_STYLES::CENTERMOUSE,
  ResourceDialog::DIALOG_STYLES::CONTEXTHELP,
  ResourceDialog::DIALOG_STYLES::SHELLFONT,
};

static constexpr auto WINDOW_STYLES_ARRAY = {
  ResourceDialog::WINDOW_STYLES::OVERLAPPED,
  ResourceDialog::WINDOW_STYLES::POPUP,
  ResourceDialog::WINDOW_STYLES::CHILD,
  ResourceDialog::WINDOW_STYLES::MINIMIZE,
  ResourceDialog::WINDOW_STYLES::VISIBLE,
  ResourceDialog::WINDOW_STYLES::DISABLED,
  ResourceDialog::WINDOW_STYLES::CLIPSIBLINGS,
  ResourceDialog::WINDOW_STYLES::CLIPCHILDREN,
  ResourceDialog::WINDOW_STYLES::MAXIMIZE,
  ResourceDialog::WINDOW_STYLES::CAPTION,
  ResourceDialog::WINDOW_STYLES::BORDER,
  ResourceDialog::WINDOW_STYLES::DLGFRAME,
  ResourceDialog::WINDOW_STYLES::VSCROLL,
  ResourceDialog::WINDOW_STYLES::HSCROLL,
  ResourceDialog::WINDOW_STYLES::SYSMENU,
  ResourceDialog::WINDOW_STYLES::THICKFRAME,
  ResourceDialog::WINDOW_STYLES::GROUP,
  ResourceDialog::WINDOW_STYLES::TABSTOP,
};

static constexpr auto WINDOW_EXTENDED_STYLES_ARRAY = {
  ResourceDialog::WINDOW_EXTENDED_STYLES::DLGMODALFRAME,
  ResourceDialog::WINDOW_EXTENDED_STYLES::NOPARENTNOTIFY,
  ResourceDialog::WINDOW_EXTENDED_STYLES::TOPMOST,
  ResourceDialog::WINDOW_EXTENDED_STYLES::ACCEPTFILES,
  ResourceDialog::WINDOW_EXTENDED_STYLES::TRANSPARENT_STY,
  ResourceDialog::WINDOW_EXTENDED_STYLES::MDICHILD,
  ResourceDialog::WINDOW_EXTENDED_STYLES::TOOLWINDOW,
  ResourceDialog::WINDOW_EXTENDED_STYLES::WINDOWEDGE,
  ResourceDialog::WINDOW_EXTENDED_STYLES::CLIENTEDGE,
  ResourceDialog::WINDOW_EXTENDED_STYLES::CONTEXTHELP,
  ResourceDialog::WINDOW_EXTENDED_STYLES::RIGHT,
  ResourceDialog::WINDOW_EXTENDED_STYLES::LEFT,
  ResourceDialog::WINDOW_EXTENDED_STYLES::RTLREADING,
  ResourceDialog::WINDOW_EXTENDED_STYLES::LEFTSCROLLBAR,
  ResourceDialog::WINDOW_EXTENDED_STYLES::CONTROLPARENT,
  ResourceDialog::WINDOW_EXTENDED_STYLES::STATICEDGE,
  ResourceDialog::WINDOW_EXTENDED_STYLES::APPWINDOW,
};

static constexpr auto CONTROL_STYLES_ARRAY = {
  ResourceDialog::CONTROL_STYLES::TOP,
  ResourceDialog::CONTROL_STYLES::NOMOVEY,
  ResourceDialog::CONTROL_STYLES::BOTTOM,
  ResourceDialog::CONTROL_STYLES::NORESIZE,
  ResourceDialog::CONTROL_STYLES::NOPARENTALIGN,
  ResourceDialog::CONTROL_STYLES::ADJUSTABLE,
  ResourceDialog::CONTROL_STYLES::NODIVIDER,
  ResourceDialog::CONTROL_STYLES::VERT,
  ResourceDialog::CONTROL_STYLES::LEFT,
  ResourceDialog::CONTROL_STYLES::RIGHT,
  ResourceDialog::CONTROL_STYLES::NOMOVEX,
};
}

#endif
