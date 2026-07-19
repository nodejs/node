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
#ifndef LIEF_PE_COMCTL32_DLL_LOOKUP_H
#define LIEF_PE_COMCTL32_DLL_LOOKUP_H
#include <cstdint>

namespace LIEF {
namespace PE {

inline const char* comctl32_dll_lookup(uint32_t i) {
  switch(i) {
  case 0x0191: return "AddMRUStringW";
  case 0x00cf: return "AttachScrollBars";
  case 0x00d2: return "CCEnableScrollBar";
  case 0x00d1: return "CCGetScrollInfo";
  case 0x00d0: return "CCSetScrollInfo";
  case 0x0190: return "CreateMRUListW";
  case 0x0008: return "CreateMappedBitmap";
  case 0x000c: return "CreatePropertySheetPage";
  case 0x0013: return "CreatePropertySheetPageA";
  case 0x0014: return "CreatePropertySheetPageW";
  case 0x0015: return "CreateStatusWindow";
  case 0x0006: return "CreateStatusWindowA";
  case 0x0016: return "CreateStatusWindowW";
  case 0x0007: return "CreateToolbar";
  case 0x0017: return "CreateToolbarEx";
  case 0x0010: return "CreateUpDownControl";
  case 0x014b: return "DPA_Clone";
  case 0x0148: return "DPA_Create";
  case 0x0154: return "DPA_CreateEx";
  case 0x0151: return "DPA_DeleteAllPtrs";
  case 0x0150: return "DPA_DeletePtr";
  case 0x0149: return "DPA_Destroy";
  case 0x0182: return "DPA_DestroyCallback";
  case 0x0181: return "DPA_EnumCallback";
  case 0x014c: return "DPA_GetPtr";
  case 0x014d: return "DPA_GetPtrIndex";
  case 0x015b: return "DPA_GetSize";
  case 0x014a: return "DPA_Grow";
  case 0x014e: return "DPA_InsertPtr";
  case 0x0009: return "DPA_LoadStream";
  case 0x000b: return "DPA_Merge";
  case 0x000a: return "DPA_SaveStream";
  case 0x0153: return "DPA_Search";
  case 0x014f: return "DPA_SetPtr";
  case 0x0152: return "DPA_Sort";
  case 0x0157: return "DSA_Clone";
  case 0x0140: return "DSA_Create";
  case 0x0147: return "DSA_DeleteAllItems";
  case 0x0146: return "DSA_DeleteItem";
  case 0x0141: return "DSA_Destroy";
  case 0x0184: return "DSA_DestroyCallback";
  case 0x0183: return "DSA_EnumCallback";
  case 0x0142: return "DSA_GetItem";
  case 0x0143: return "DSA_GetItemPtr";
  case 0x015c: return "DSA_GetSize";
  case 0x0144: return "DSA_InsertItem";
  case 0x0145: return "DSA_SetItem";
  case 0x015a: return "DSA_Sort";
  case 0x019d: return "DefSubclassProc";
  case 0x0018: return "DestroyPropertySheetPage";
  case 0x00ce: return "DetachScrollBars";
  case 0x0019: return "DllGetVersion";
  case 0x001a: return "DllInstall";
  case 0x000f: return "DrawInsert";
  case 0x00c9: return "DrawScrollBar";
  case 0x001b: return "DrawShadowText";
  case 0x00c8: return "DrawSizeBox";
  case 0x001c: return "DrawStatusText";
  case 0x0005: return "DrawStatusTextA";
  case 0x001d: return "DrawStatusTextW";
  case 0x0193: return "EnumMRUListW";
  case 0x001e: return "FlatSB_EnableScrollBar";
  case 0x001f: return "FlatSB_GetScrollInfo";
  case 0x0020: return "FlatSB_GetScrollPos";
  case 0x0021: return "FlatSB_GetScrollProp";
  case 0x0022: return "FlatSB_GetScrollPropPtr";
  case 0x0023: return "FlatSB_GetScrollRange";
  case 0x0024: return "FlatSB_SetScrollInfo";
  case 0x0025: return "FlatSB_SetScrollPos";
  case 0x0026: return "FlatSB_SetScrollProp";
  case 0x0027: return "FlatSB_SetScrollRange";
  case 0x0028: return "FlatSB_ShowScrollBar";
  case 0x0098: return "FreeMRUList";
  case 0x0004: return "GetEffectiveClientRect";
  case 0x0029: return "GetMUILanguage";
  case 0x019b: return "GetWindowSubclass";
  case 0x002a: return "HIMAGELIST_QueryInterface";
  case 0x00cd: return "HandleScrollCmd";
  case 0x002b: return "ImageList_Add";
  case 0x002c: return "ImageList_AddIcon";
  case 0x002d: return "ImageList_AddMasked";
  case 0x002e: return "ImageList_BeginDrag";
  case 0x002f: return "ImageList_CoCreateInstance";
  case 0x0030: return "ImageList_Copy";
  case 0x0031: return "ImageList_Create";
  case 0x0032: return "ImageList_Destroy";
  case 0x0033: return "ImageList_DestroyShared";
  case 0x0034: return "ImageList_DragEnter";
  case 0x0035: return "ImageList_DragLeave";
  case 0x0036: return "ImageList_DragMove";
  case 0x0037: return "ImageList_DragShowNolock";
  case 0x0038: return "ImageList_Draw";
  case 0x0039: return "ImageList_DrawEx";
  case 0x003a: return "ImageList_DrawIndirect";
  case 0x003b: return "ImageList_Duplicate";
  case 0x003c: return "ImageList_EndDrag";
  case 0x003d: return "ImageList_GetBkColor";
  case 0x003e: return "ImageList_GetDragImage";
  case 0x003f: return "ImageList_GetFlags";
  case 0x0040: return "ImageList_GetIcon";
  case 0x0041: return "ImageList_GetIconSize";
  case 0x0042: return "ImageList_GetImageCount";
  case 0x0043: return "ImageList_GetImageInfo";
  case 0x0044: return "ImageList_GetImageRect";
  case 0x0045: return "ImageList_LoadImage";
  case 0x0046: return "ImageList_LoadImageA";
  case 0x004b: return "ImageList_LoadImageW";
  case 0x004c: return "ImageList_Merge";
  case 0x004d: return "ImageList_Read";
  case 0x004e: return "ImageList_ReadEx";
  case 0x004f: return "ImageList_Remove";
  case 0x0050: return "ImageList_Replace";
  case 0x0051: return "ImageList_ReplaceIcon";
  case 0x0052: return "ImageList_Resize";
  case 0x0053: return "ImageList_SetBkColor";
  case 0x0054: return "ImageList_SetDragCursorImage";
  case 0x0055: return "ImageList_SetFilter";
  case 0x0056: return "ImageList_SetFlags";
  case 0x0057: return "ImageList_SetIconSize";
  case 0x0058: return "ImageList_SetImageCount";
  case 0x0059: return "ImageList_SetOverlayImage";
  case 0x005a: return "ImageList_Write";
  case 0x005b: return "ImageList_WriteEx";
  case 0x0011: return "InitCommonControls";
  case 0x005c: return "InitCommonControlsEx";
  case 0x005d: return "InitMUILanguage";
  case 0x005e: return "InitializeFlatSB";
  case 0x000e: return "LBItemFromPt";
  case 0x017c: return "LoadIconMetric";
  case 0x017d: return "LoadIconWithScaleDown";
  case 0x000d: return "MakeDragList";
  case 0x0002: return "MenuHelp";
  case 0x005f: return "PropertySheet";
  case 0x0060: return "PropertySheetA";
  case 0x0061: return "PropertySheetW";
  case 0x018a: return "QuerySystemGestureStatus";
  case 0x0062: return "RegisterClassNameW";
  case 0x019c: return "RemoveWindowSubclass";
  case 0x00cc: return "ScrollBar_Menu";
  case 0x00cb: return "ScrollBar_MouseMove";
  case 0x019a: return "SetWindowSubclass";
  case 0x0003: return "ShowHideMenuCtl";
  case 0x00ca: return "SizeBoxHwnd";
  case 0x00ec: return "Str_SetPtrW";
  case 0x0158: return "TaskDialog";
  case 0x0159: return "TaskDialogIndirect";
  case 0x0063: return "UninitializeFlatSB";
  case 0x0064: return "_TrackMouseEvent";
  }
  return nullptr;
}


}
}

#endif

