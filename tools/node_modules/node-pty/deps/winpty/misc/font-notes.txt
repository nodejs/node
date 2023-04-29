==================================================================
Notes regarding fonts, code pages, and East Asian character widths
==================================================================


Registry settings
=================

 * There are console registry settings in `HKCU\Console`.  That key has many
   default settings (e.g. the default font settings) and also per-app subkeys
   for app-specific overrides.

 * It is possible to override the code page with an app-specific setting.

 * There are registry settings in
   `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Console`.  In particular,
   the `TrueTypeFont` subkey has a list of suitable font names associated with
   various CJK code pages, as well as default font names.

 * There are two values in `HKLM\SYSTEM\CurrentControlSet\Control\Nls\CodePage`
   that specify the current code pages -- `OEMCP` and `ACP`.  Setting the
   system locale via the Control Panel's "Region" or "Language" dialogs seems
   to change these code page values.


Console fonts
=============

 * The `FontFamily` field of `CONSOLE_FONT_INFOEX` has two parts:
    - The high four bits can be exactly one of the `FF_xxxx` font families:
          FF_DONTCARE(0x00)
          FF_ROMAN(0x10)
          FF_SWISS(0x20)
          FF_MODERN(0x30)
          FF_SCRIPT(0x40)
          FF_DECORATIVE(0x50)
    - The low four bits are a bitmask:
          TMPF_FIXED_PITCH(1) -- actually means variable pitch
          TMPF_VECTOR(2)
          TMPF_TRUETYPE(4)
          TMPF_DEVICE(8)

 * Each console has its own independent console font table.  The current font
   is identified with an index into this table.  The size of the table is
   returned by the undocumented `GetNumberOfConsoleFonts` API.  It is apparently
   possible to get the table size without this API, by instead calling
   `GetConsoleFontSize` on each nonnegative index starting with 0 until the API
   fails by returning (0, 0).

 * The font table grows dynamically.  Each time the console is configured with
   a previously-unused (FaceName, Size) combination, two entries are added to
   the font table -- one with normal weight and one with bold weight.  Fonts
   added this way are always TrueType fonts.

 * Initially, the font table appears to contain only raster fonts.  For
   example, on an English Windows 8 installation, here is the initial font
   table:
       font 0: 4x6
       font 1: 6x8
       font 2: 8x8
       font 3: 16x8
       font 4: 5x12
       font 5: 7x12
       font 6: 8x12 -- the current font
       font 7: 16x12
       font 8: 12x16
       font 9: 10x18
   `GetNumberOfConsoleFonts` returns 10, and this table matches the raster font
   sizes according to the console properties dialog.

 * With a Japanese or Chinese locale, the initial font table appears to contain
   the sizes applicable to both the East Asian raster font, as well as the
   sizes for the CP437/CP1252 raster font.

 * The index passed to `SetCurrentConsoleFontEx` apparently has no effect.
   The undocumented `SetConsoleFont` API, however, accepts *only* a font index,
   and on Windows 8 English, it switches between all 10 fonts, even font index
   #0.

 * If the index passed to `SetConsoleFont` identifies a Raster Font
   incompatible with the current code page, then another Raster Font is
   activated.

 * Passing "Terminal" to `SetCurrentConsoleFontEx` seems to have no effect.
   Perhaps relatedly, `SetCurrentConsoleFontEx` does not fail if it is given a
   bogus `FaceName`.  Some font is still chosen and activated.  Passing a face
   name and height seems to work reliably, modulo the CP936 issue described
   below.


Console fonts and code pages
============================

 * On an English Windows installation, the default code page is 437, and it
   cannot be set to 932 (Shift-JIS).  (The API call fails.)  Changing the
   system locale to "Japanese (Japan)" using the Region/Language dialog
   changes the default CP to 932 and permits changing the console CP between
   437 and 932.

 * A console has both an input code page and an output code page
   (`{Get,Set}ConsoleCP` and `{Get,Set}ConsoleOutputCP`).  I'm not going to
   distinguish between the two for this document; presumably only the output
   CP matters.  The code page can change while the console is open, e.g.
   by running `mode con: cp select={932,437,1252}` or by calling
   `SetConsoleOutputCP`.

 * The current code page restricts which TrueType fonts and which Raster Font
   sizes are available in the console properties dialog.  This can change
   while the console is open.

 * Changing the code page almost(?) always changes the current console font.
   So far, I don't know how the new font is chosen.

 * With a CP of 932, the only TrueType font available in the console properties
   dialog is "MS Gothic", displayed as "ＭＳ ゴシック".  It is still possible to
   use the English-default TrueType console fonts, Lucida Console and Consolas,
   via `SetCurrentConsoleFontEx`.

 * When using a Raster Font and CP437 or CP1252, writing a UTF-16 codepoint not
   representable in the code page instead writes a question mark ('?') to the
   console.  This conversion does not apply with a TrueType font, nor with the
   Raster Font for CP932 or CP936.


ReadConsoleOutput and double-width characters
==============================================

 * With a Raster Font active, when `ReadConsoleOutputW` reads two cells of a
   double-width character, it fills only a single `CHAR_INFO` structure.  The
   unused trailing `CHAR_INFO` structures are zero-filled.  With a TrueType
   font active, `ReadConsoleOutputW` instead fills two `CHAR_INFO` structures,
   the first marked with `COMMON_LVB_LEADING_BYTE` and the second marked with
   `COMMON_LVB_TRAILING_BYTE`.  The flag is a misnomer--there aren't two
   *bytes*, but two cells, and they have equal `CHAR_INFO.Char.UnicodeChar`
   values.

 * `ReadConsoleOutputA`, on the other hand, reads two `CHAR_INFO` cells, and
   if the UTF-16 value can be represented as two bytes in the ANSI/OEM CP, then
   the two bytes are placed in the two `CHAR_INFO.Char.AsciiChar` values, and
   the `COMMON_LVB_{LEADING,TRAILING}_BYTE` values are also used.  If the
   codepoint isn't representable, I don't remember what happens -- I think the
   `AsciiChar` values take on an invalid marker.

 * Reading only one cell of a double-width character reads a space (U+0020)
   instead.  Raster-vs-TrueType and wide-vs-ANSI do not matter.
    - XXX: what about attributes?  Can a double-width character have mismatched
      color attributes?
    - XXX: what happens when writing to just one cell of a double-width
      character?


Default Windows fonts for East Asian languages
==============================================
CP932 / Japanese: "ＭＳ ゴシック" (MS Gothic)
CP936 / Chinese Simplified: "新宋体" (SimSun)


Unreliable character width (half-width vs full-width)
=====================================================

The half-width vs full-width status of a codepoint depends on at least these variables:
 * OS version (Win10 legacy and new modes are different versions)
 * system locale (English vs Japanese vs Chinese Simplified vs Chinese Traditional, etc)
 * code page (437 vs 932 vs 936, etc)
 * raster vs TrueType (Terminal vs MS Gothic vs SimSun, etc)
 * font size
 * rendered-vs-model (rendered width can be larger or smaller than model width)

Example 1: U+2014 (EM DASH): East_Asian_Width: Ambiguous
--------------------------------------------------------
                                            rendered        modeled
CP932: Win7/8 Raster Fonts                  half            half
CP932: Win7/8 Gothic 14/15px                half            full
CP932: Win7/8 Consolas 14/15px              half            full
CP932: Win7/8 Lucida Console 14px           half            full
CP932: Win7/8 Lucida Console 15px           half            half
CP932: Win10New Raster Fonts                half            half
CP932: Win10New Gothic 14/15px              half            half
CP932: Win10New Consolas 14/15px            half            half
CP932: Win10New Lucida Console 14/15px      half            half

CP936: Win7/8 Raster Fonts                  full            full
CP936: Win7/8 SimSun 14px                   full            full
CP936: Win7/8 SimSun 15px                   full            half
CP936: Win7/8 Consolas 14/15px              half            full
CP936: Win10New Raster Fonts                full            full
CP936: Win10New SimSum 14/15px              full            full
CP936: Win10New Consolas 14/15px            half            half

Example 2: U+3044 (HIRAGANA LETTER I): East_Asian_Width: Wide
-------------------------------------------------------------
                                            rendered        modeled
CP932: Win7/8/10N Raster Fonts              full            full
CP932: Win7/8/10N Gothic 14/15px            full            full
CP932: Win7/8/10N Consolas 14/15px          half(*2)        full
CP932: Win7/8/10N Lucida Console 14/15px    half(*3)        full

CP936: Win7/8/10N Raster Fonts              full            full
CP936: Win7/8/10N SimSun 14/15px            full            full
CP936: Win7/8/10N Consolas 14/15px          full            full

Example 3: U+30FC (KATAKANA-HIRAGANA PROLONGED SOUND MARK): East_Asian_Width: Wide
----------------------------------------------------------------------------------
                                            rendered        modeled
CP932: Win7 Raster Fonts                    full            full
CP932: Win7 Gothic 14/15px                  full            full
CP932: Win7 Consolas 14/15px                half(*2)        full
CP932: Win7 Lucida Console 14px             half(*3)        full
CP932: Win7 Lucida Console 15px             half(*3)        half
CP932: Win8 Raster Fonts                    full            full
CP932: Win8 Gothic 14px                     full            half
CP932: Win8 Gothic 15px                     full            full
CP932: Win8 Consolas 14/15px                half(*2)        full
CP932: Win8 Lucida Console 14px             half(*3)        full
CP932: Win8 Lucida Console 15px             half(*3)        half
CP932: Win10New Raster Fonts                full            full
CP932: Win10New Gothic 14/15px              full            full
CP932: Win10New Consolas 14/15px            half(*2)        half
CP932: Win10New Lucida Console 14/15px      half(*2)        half

CP936: Win7/8 Raster Fonts                  full            full
CP936: Win7/8 SimSun 14px                   full            full
CP936: Win7/8 SimSun 15px                   full            half
CP936: Win7/8 Consolas 14px                 full            full
CP936: Win7/8 Consolas 15px                 full            half
CP936: Win10New Raster Fonts                full            full
CP936: Win10New SimSum 14/15px              full            full
CP936: Win10New Consolas 14/15px            full            full

Example 4: U+4000 (CJK UNIFIED IDEOGRAPH-4000): East_Asian_Width: Wide
----------------------------------------------------------------------
                                            rendered        modeled
CP932: Win7 Raster Fonts                    half(*1)        half
CP932: Win7 Gothic 14/15px                  full            full
CP932: Win7 Consolas 14/15px                half(*2)        full
CP932: Win7 Lucida Console 14px             half(*3)        full
CP932: Win7 Lucida Console 15px             half(*3)        half
CP932: Win8 Raster Fonts                    half(*1)        half
CP932: Win8 Gothic 14px                     full            half
CP932: Win8 Gothic 15px                     full            full
CP932: Win8 Consolas 14/15px                half(*2)        full
CP932: Win8 Lucida Console 14px             half(*3)        full
CP932: Win8 Lucida Console 15px             half(*3)        half
CP932: Win10New Raster Fonts                half(*1)        half
CP932: Win10New Gothic 14/15px              full            full
CP932: Win10New Consolas 14/15px            half(*2)        half
CP932: Win10New Lucida Console 14/15px      half(*2)        half

CP936: Win7/8 Raster Fonts                  full            full
CP936: Win7/8 SimSun 14px                   full            full
CP936: Win7/8 SimSun 15px                   full            half
CP936: Win7/8 Consolas 14px                 full            full
CP936: Win7/8 Consolas 15px                 full            half
CP936: Win10New Raster Fonts                full            full
CP936: Win10New SimSum 14/15px              full            full
CP936: Win10New Consolas 14/15px            full            full

(*1) Rendered as a half-width filled white box
(*2) Rendered as a half-width box with a question mark inside
(*3) Rendered as a half-width empty box
(!!) One of the only places in Win10New where rendered and modeled width disagree


Windows quirk: unreliable font heights with CP936 / Chinese Simplified
======================================================================

When I set the font to 新宋体 17px, using either the properties dialog or
`SetCurrentConsoleFontEx`, the height reported by `GetCurrentConsoleFontEx` is
not 17, but is instead 19.  The same problem does not affect Raster Fonts,
nor have I seen the problem in the English or Japanese locales.  I observed
this with Windows 7 and Windows 10 new mode.

If I set the font using the facename, width, *and* height, then the
`SetCurrentConsoleFontEx` and `GetCurrentConsoleFontEx` values agree.  If I
set the font using *only* the facename and height, then the two values
disagree.


Windows bug: GetCurrentConsoleFontEx is initially invalid
=========================================================

 - Assume there is no configured console font name in the registry.  In this
   case, the console defaults to a raster font.
 - Open a new console and call the `GetCurrentConsoleFontEx` API.
 - The `FaceName` field of the returned `CONSOLE_FONT_INFOEX` data
   structure is incorrect.  On Windows 7, 8, and 10, I observed that the
   field was blank.  On Windows 8, occasionally, it instead contained:
      U+AE72 U+75BE U+0001
   The other fields of the structure all appeared correct:
      nFont=6 dwFontSize=(8,12) FontFamily=0x30 FontWeight=400
 - The `FaceName` field becomes initialized easily:
    - Open the console properties dialog and click OK.  (Cancel is not
      sufficient.)
    - Call the undocumented `SetConsoleFont` with the current font table
      index, which is 6 in the example above.
 - It seems that the console uncritically accepts whatever string is
   stored in the registry, including a blank string, and passes it on the
   the `GetCurrentConsoleFontEx` caller.  It is possible to get the console
   to *write* a blank setting into the registry -- simply open the console
   (default or app-specific) properties and click OK.
