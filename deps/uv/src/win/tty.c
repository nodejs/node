/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>
#include <io.h>
#include <string.h>
#include <stdint.h>

#include "uv.h"
#include "../uv-common.h"
#include "internal.h"


#define UNICODE_REPLACEMENT_CHARACTER (0xfffd)

#define ANSI_NORMAL           0x00
#define ANSI_ESCAPE_SEEN      0x02
#define ANSI_CSI              0x04
#define ANSI_ST_CONTROL       0x08
#define ANSI_IGNORE           0x10
#define ANSI_IN_ARG           0x20
#define ANSI_IN_STRING        0x40
#define ANSI_BACKSLASH_SEEN   0x80


static void uv_tty_update_virtual_window(CONSOLE_SCREEN_BUFFER_INFO* info);


/* Null uv_buf_t */
static const uv_buf_t uv_null_buf_ = { 0, NULL };


/*
 * The console virtual window.
 *
 * Normally cursor movement in windows is relative to the console screen buffer,
 * e.g. the application is allowed to overwrite the 'history'. This is very
 * inconvenient, it makes absolute cursor movement pretty useless. There is
 * also the concept of 'client rect' which is defined by the actual size of
 * the console window and the scroll position of the screen buffer, but it's
 * very volatile because it changes when the user scrolls.
 *
 * To make cursor movement behave sensibly we define a virtual window to which
 * cursor movement is confined. The virtual window is always as wide as the
 * console screen buffer, but it's height is defined by the size of the
 * console window. The top of the virtual window aligns with the position
 * of the caret when the first stdout/err handle is created, unless that would
 * mean that it would extend beyond the bottom of the screen buffer -  in that
 * that case it's located as far down as possible.
 *
 * When the user writes a long text or many newlines, such that the output
 * reaches beyond the bottom of the virtual window, the virtual window is
 * shifted downwards, but not resized.
 *
 * Since all tty i/o happens on the same console, this window is shared
 * between all stdout/stderr handles.
 */

static int uv_tty_virtual_offset = -1;
static int uv_tty_virtual_height = -1;
static int uv_tty_virtual_width = -1;

static CRITICAL_SECTION uv_tty_output_lock;


void uv_console_init() {
  InitializeCriticalSection(&uv_tty_output_lock);
}


int uv_tty_init(uv_loop_t* loop, uv_tty_t* tty, uv_file fd, int readable) {
  HANDLE win_handle;
  CONSOLE_SCREEN_BUFFER_INFO info;

  loop->counters.tty_init++;

  win_handle = (HANDLE) _get_osfhandle(fd);
  if (win_handle == INVALID_HANDLE_VALUE) {
    uv__set_sys_error(loop, ERROR_INVALID_HANDLE);
    return -1;
  }

  if (!GetConsoleMode(win_handle, &tty->original_console_mode)) {
    uv__set_sys_error(loop, GetLastError());
    return -1;
  }

  /* Initialize virtual window size; if it fails, assume that this is stdin. */
  if (GetConsoleScreenBufferInfo(win_handle, &info)) {
    EnterCriticalSection(&uv_tty_output_lock);
    uv_tty_update_virtual_window(&info);
    LeaveCriticalSection(&uv_tty_output_lock);
  }

  uv_stream_init(loop, (uv_stream_t*) tty);
  uv_connection_init((uv_stream_t*) tty);

  tty->type = UV_TTY;
  tty->handle = win_handle;
  tty->read_line_handle = NULL;
  tty->read_line_buffer = uv_null_buf_;
  tty->read_raw_wait = NULL;
  tty->reqs_pending = 0;
  tty->flags |= UV_HANDLE_BOUND;

  /* Init keycode-to-vt100 mapper state. */
  tty->last_key_len = 0;
  tty->last_key_offset = 0;
  tty->last_utf16_high_surrogate = 0;
  memset(&tty->last_input_record, 0, sizeof tty->last_input_record);

  /* Init utf8-to-utf16 conversion state. */
  tty->utf8_bytes_left = 0;
  tty->utf8_codepoint = 0;

  /* Initialize eol conversion state */
  tty->previous_eol = 0;

  /* Init ANSI parser state. */
  tty->ansi_parser_state = ANSI_NORMAL;

  return 0;
}


int uv_tty_set_mode(uv_tty_t* tty, int mode) {
  DWORD flags = 0;
  unsigned char was_reading;
  uv_alloc_cb alloc_cb;
  uv_read_cb read_cb;

  if (!!mode == !!(tty->flags & UV_HANDLE_TTY_RAW)) {
    return 0;
  }

  if (tty->original_console_mode & ENABLE_QUICK_EDIT_MODE) {
    flags = ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS;
  }

  if (mode) {
    /* Raw input */
    flags |= ENABLE_WINDOW_INPUT;
  } else {
    /* Line-buffered mode. */
    flags |= ENABLE_ECHO_INPUT | ENABLE_INSERT_MODE | ENABLE_LINE_INPUT |
        ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT;
  }

  if (!SetConsoleMode(tty->handle, flags)) {
    uv__set_sys_error(tty->loop, GetLastError());
    return -1;
  }

  /* If currently reading, stop, and restart reading. */
  if (tty->flags & UV_HANDLE_READING) {
    was_reading = 1;
    alloc_cb = tty->alloc_cb;
    read_cb = tty->read_cb;

    if (was_reading && uv_tty_read_stop(tty) != 0) {
      return -1;
    }
  } else {
    was_reading = 0;
  }

  /* Update flag. */
  tty->flags &= ~UV_HANDLE_TTY_RAW;
  tty->flags |= mode ? UV_HANDLE_TTY_RAW : 0;

  /* If we just stopped reading, restart. */
  if (was_reading && uv_tty_read_start(tty, alloc_cb, read_cb) != 0) {
    return -1;
  }

  return 0;
}


int uv_is_tty(uv_file file) {
  DWORD result;
  return GetConsoleMode((HANDLE) _get_osfhandle(file), &result) != 0;
}


int uv_tty_get_winsize(uv_tty_t* tty, int* width, int* height) {
  CONSOLE_SCREEN_BUFFER_INFO info;

  if (!GetConsoleScreenBufferInfo(tty->handle, &info)) {
    uv__set_sys_error(tty->loop, GetLastError());
    return -1;
  }

  EnterCriticalSection(&uv_tty_output_lock);
  uv_tty_update_virtual_window(&info);
  LeaveCriticalSection(&uv_tty_output_lock);

  *width = uv_tty_virtual_width;
  *height = uv_tty_virtual_height;

  return 0;
}


static void CALLBACK uv_tty_post_raw_read(void* data, BOOLEAN didTimeout) {
  uv_loop_t* loop;
  uv_tty_t* handle;
  uv_req_t* req;

  assert(data);
  assert(!didTimeout);

  req = (uv_req_t*) data;
  handle = (uv_tty_t*) req->data;
  loop = handle->loop;

  UnregisterWait(handle->read_raw_wait);
  handle->read_raw_wait = NULL;

  SET_REQ_SUCCESS(req);
  POST_COMPLETION_FOR_REQ(loop, req);
}


static void uv_tty_queue_read_raw(uv_loop_t* loop, uv_tty_t* handle) {
  uv_read_t* req;
  BOOL r;

  assert(handle->flags & UV_HANDLE_READING);
  assert(!(handle->flags & UV_HANDLE_READ_PENDING));

  assert(handle->handle && handle->handle != INVALID_HANDLE_VALUE);

  handle->read_line_buffer = uv_null_buf_;

  req = &handle->read_req;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  r = RegisterWaitForSingleObject(&handle->read_raw_wait,
                                  handle->handle,
                                  uv_tty_post_raw_read,
                                  (void*) req,
                                  INFINITE,
                                  WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE);
  if (!r) {
    handle->read_raw_wait = NULL;
    SET_REQ_ERROR(req, GetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
  }

  handle->flags |= UV_HANDLE_READ_PENDING;
  handle->reqs_pending++;
}


static DWORD CALLBACK uv_tty_line_read_thread(void* data) {
  uv_loop_t* loop;
  uv_tty_t* handle;
  uv_req_t* req;
  DWORD bytes, read_bytes;

  assert(data);

  req = (uv_req_t*) data;
  handle = (uv_tty_t*) req->data;
  loop = handle->loop;

  assert(handle->read_line_buffer.base != NULL);
  assert(handle->read_line_buffer.len > 0);

  /* ReadConsole can't handle big buffers. */
  if (handle->read_line_buffer.len < 8192) {
    bytes = handle->read_line_buffer.len;
  } else {
    bytes = 8192;
  }

  /* Todo: Unicode */
  if (ReadConsoleA(handle->read_line_handle,
                   (void*) handle->read_line_buffer.base,
                   bytes,
                   &read_bytes,
                   NULL)) {
    SET_REQ_SUCCESS(req);
    req->overlapped.InternalHigh = read_bytes;
  } else {
    SET_REQ_ERROR(req, GetLastError());
  }

  POST_COMPLETION_FOR_REQ(loop, req);
  return 0;
}


static void uv_tty_queue_read_line(uv_loop_t* loop, uv_tty_t* handle) {
  uv_read_t* req;
  BOOL r;

  assert(handle->flags & UV_HANDLE_READING);
  assert(!(handle->flags & UV_HANDLE_READ_PENDING));
  assert(handle->handle && handle->handle != INVALID_HANDLE_VALUE);

  req = &handle->read_req;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  handle->read_line_buffer = handle->alloc_cb((uv_handle_t*) handle, 8192);
  assert(handle->read_line_buffer.base != NULL);
  assert(handle->read_line_buffer.len > 0);

  /* Duplicate the console handle, so if we want to cancel the read, we can */
  /* just close this handle duplicate. */
  if (handle->read_line_handle == NULL) {
    HANDLE this_process = GetCurrentProcess();
    r = DuplicateHandle(this_process,
                        handle->handle,
                        this_process,
                        &handle->read_line_handle,
                        0,
                        0,
                        DUPLICATE_SAME_ACCESS);
    if (!r) {
      handle->read_line_handle = NULL;
      SET_REQ_ERROR(req, GetLastError());
      uv_insert_pending_req(loop, (uv_req_t*)req);
      goto out;
    }
  }

  r = QueueUserWorkItem(uv_tty_line_read_thread,
                        (void*) req,
                        WT_EXECUTELONGFUNCTION);
  if (!r) {
    SET_REQ_ERROR(req, GetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
  }

 out:
  handle->flags |= UV_HANDLE_READ_PENDING;
  handle->reqs_pending++;
}


static void uv_tty_queue_read(uv_loop_t* loop, uv_tty_t* handle) {
  if (handle->flags & UV_HANDLE_TTY_RAW) {
    uv_tty_queue_read_raw(loop, handle);
  } else {
    uv_tty_queue_read_line(loop, handle);
  }
}


static const char* get_vt100_fn_key(DWORD code, char shift, char ctrl,
    size_t* len) {
#define VK_CASE(vk, normal_str, shift_str, ctrl_str, shift_ctrl_str)          \
    case (vk):                                                                \
      if (shift && ctrl) {                                                    \
        *len = sizeof shift_ctrl_str;                                         \
        return "\033" shift_ctrl_str;                                         \
      } else if (shift) {                                                     \
        *len = sizeof shift_str ;                                             \
        return "\033" shift_str;                                              \
      } else if (ctrl) {                                                      \
        *len = sizeof ctrl_str;                                               \
        return "\033" ctrl_str;                                               \
      } else {                                                                \
        *len = sizeof normal_str;                                             \
        return "\033" normal_str;                                             \
      }

  switch (code) {
    /* These mappings are the same as Cygwin's. Unmodified and alt-modified */
    /* keypad keys comply with linux console, modifiers comply with xterm */
    /* modifier usage. F1..f12 and shift-f1..f10 comply with linux console, */
    /* f6..f12 with and without modifiers comply with rxvt. */
    VK_CASE(VK_INSERT,  "[2~",  "[2;2~", "[2;5~", "[2;6~")
    VK_CASE(VK_END,     "[4~",  "[4;2~", "[4;5~", "[4;6~")
    VK_CASE(VK_DOWN,    "[B",   "[1;2B", "[1;5B", "[1;6B")
    VK_CASE(VK_NEXT,    "[6~",  "[6;2~", "[6;5~", "[6;6~")
    VK_CASE(VK_LEFT,    "[D",   "[1;2D", "[1;5D", "[1;6D")
    VK_CASE(VK_CLEAR,   "[G",   "[1;2G", "[1;5G", "[1;6G")
    VK_CASE(VK_RIGHT,   "[C",   "[1;2C", "[1;5C", "[1;6C")
    VK_CASE(VK_UP,      "[A",   "[1;2A", "[1;5A", "[1;6A")
    VK_CASE(VK_HOME,    "[1~",  "[1;2~", "[1;5~", "[1;6~")
    VK_CASE(VK_PRIOR,   "[5~",  "[5;2~", "[5;5~", "[5;6~")
    VK_CASE(VK_DELETE,  "[3~",  "[3;2~", "[3;5~", "[3;6~")
    VK_CASE(VK_NUMPAD0, "[2~",  "[2;2~", "[2;5~", "[2;6~")
    VK_CASE(VK_NUMPAD1, "[4~",  "[4;2~", "[4;5~", "[4;6~")
    VK_CASE(VK_NUMPAD2, "[B",   "[1;2B", "[1;5B", "[1;6B")
    VK_CASE(VK_NUMPAD3, "[6~",  "[6;2~", "[6;5~", "[6;6~")
    VK_CASE(VK_NUMPAD4, "[D",   "[1;2D", "[1;5D", "[1;6D")
    VK_CASE(VK_NUMPAD5, "[G",   "[1;2G", "[1;5G", "[1;6G")
    VK_CASE(VK_NUMPAD6, "[C",   "[1;2C", "[1;5C", "[1;6C")
    VK_CASE(VK_NUMPAD7, "[A",   "[1;2A", "[1;5A", "[1;6A")
    VK_CASE(VK_NUMPAD8, "[1~",  "[1;2~", "[1;5~", "[1;6~")
    VK_CASE(VK_NUMPAD9, "[5~",  "[5;2~", "[5;5~", "[5;6~")
    VK_CASE(VK_DECIMAL, "[3~",  "[3;2~", "[3;5~", "[3;6~")
    VK_CASE(VK_F1,      "[[A",  "[23~",  "[11^",  "[23^" )
    VK_CASE(VK_F2,      "[[B",  "[24~",  "[12^",  "[24^" )
    VK_CASE(VK_F3,      "[[C",  "[25~",  "[13^",  "[25^" )
    VK_CASE(VK_F4,      "[[D",  "[26~",  "[14^",  "[26^" )
    VK_CASE(VK_F5,      "[[E",  "[28~",  "[15^",  "[28^" )
    VK_CASE(VK_F6,      "[17~", "[29~",  "[17^",  "[29^" )
    VK_CASE(VK_F7,      "[18~", "[31~",  "[18^",  "[31^" )
    VK_CASE(VK_F8,      "[19~", "[32~",  "[19^",  "[32^" )
    VK_CASE(VK_F9,      "[20~", "[33~",  "[20^",  "[33^" )
    VK_CASE(VK_F10,     "[21~", "[34~",  "[21^",  "[34^" )
    VK_CASE(VK_F11,     "[23~", "[23$",  "[23^",  "[23@" )
    VK_CASE(VK_F12,     "[24~", "[24$",  "[24^",  "[24@" )

    default:
      *len = 0;
      return NULL;
  }
#undef VK_CASE
}


void uv_process_tty_read_raw_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_req_t* req) {
  /* Shortcut for handle->last_input_record.Event.KeyEvent. */
#define KEV handle->last_input_record.Event.KeyEvent

  DWORD records_left, records_read;
  uv_buf_t buf;
  off_t buf_used;

  assert(handle->type == UV_TTY);
  handle->flags &= ~UV_HANDLE_READ_PENDING;

  if (!(handle->flags & UV_HANDLE_READING) ||
      !(handle->flags & UV_HANDLE_TTY_RAW)) {
    goto out;
  }

  if (!REQ_SUCCESS(req)) {
    /* An error occurred while waiting for the event. */
    if ((handle->flags & UV_HANDLE_READING)) {
      handle->flags &= ~UV_HANDLE_READING;
      uv__set_sys_error(loop, GET_REQ_ERROR(req));
      handle->read_cb((uv_stream_t*)handle, -1, uv_null_buf_);
    }
    goto out;
  }

  /* Fetch the number of events  */
  if (!GetNumberOfConsoleInputEvents(handle->handle, &records_left)) {
    handle->flags &= ~UV_HANDLE_READING;
    uv__set_sys_error(loop, GetLastError());
    handle->read_cb((uv_stream_t*)handle, -1, uv_null_buf_);
    goto out;
  }

  /* Windows sends a lot of events that we're not interested in, so buf */
  /* will be allocated on demand, when there's actually something to emit. */
  buf = uv_null_buf_;
  buf_used = 0;

  while ((records_left > 0 || handle->last_key_len > 0) &&
         (handle->flags & UV_HANDLE_READING)) {
    if (handle->last_key_len == 0) {
      /* Read the next input record */
      if (!ReadConsoleInputW(handle->handle,
                             &handle->last_input_record,
                             1,
                             &records_read)) {
        uv__set_sys_error(loop, GetLastError());
        handle->flags &= ~UV_HANDLE_READING;
        handle->read_cb((uv_stream_t*) handle, -1, buf);
        goto out;
      }
      records_left--;

      /* Ignore events that are not keyboard events */
      if (handle->last_input_record.EventType != KEY_EVENT) {
        continue;
      }

      /* Ignore keyup events, unless the left alt key was held and a valid */
      /* unicode character was emitted. */
      if (!KEV.bKeyDown && !(((KEV.dwControlKeyState & LEFT_ALT_PRESSED) ||
          KEV.wVirtualKeyCode==VK_MENU) && KEV.uChar.UnicodeChar != 0)) {
        continue;
      }

      /* Ignore keypresses to numpad number keys if the left alt is held */
      /* because the user is composing a character, or windows simulating */
      /* this. */
      if ((KEV.dwControlKeyState & LEFT_ALT_PRESSED) &&
          !(KEV.dwControlKeyState & ENHANCED_KEY) &&
          (KEV.wVirtualKeyCode == VK_INSERT ||
          KEV.wVirtualKeyCode == VK_END ||
          KEV.wVirtualKeyCode == VK_DOWN ||
          KEV.wVirtualKeyCode == VK_NEXT ||
          KEV.wVirtualKeyCode == VK_LEFT ||
          KEV.wVirtualKeyCode == VK_CLEAR ||
          KEV.wVirtualKeyCode == VK_RIGHT ||
          KEV.wVirtualKeyCode == VK_HOME ||
          KEV.wVirtualKeyCode == VK_UP ||
          KEV.wVirtualKeyCode == VK_PRIOR ||
          KEV.wVirtualKeyCode == VK_NUMPAD0 ||
          KEV.wVirtualKeyCode == VK_NUMPAD1 ||
          KEV.wVirtualKeyCode == VK_NUMPAD2 ||
          KEV.wVirtualKeyCode == VK_NUMPAD3 ||
          KEV.wVirtualKeyCode == VK_NUMPAD4 ||
          KEV.wVirtualKeyCode == VK_NUMPAD5 ||
          KEV.wVirtualKeyCode == VK_NUMPAD6 ||
          KEV.wVirtualKeyCode == VK_NUMPAD7 ||
          KEV.wVirtualKeyCode == VK_NUMPAD8 ||
          KEV.wVirtualKeyCode == VK_NUMPAD9)) {
        continue;
      }

      if (KEV.uChar.UnicodeChar != 0) {
        int prefix_len, char_len;

        /* Character key pressed */
        if (KEV.uChar.UnicodeChar >= 0xD800 &&
            KEV.uChar.UnicodeChar < 0xDC00) {
          /* UTF-16 high surrogate */
          handle->last_utf16_high_surrogate = KEV.uChar.UnicodeChar;
          continue;
        }

        /* Prefix with \u033 if alt was held, but alt was not used as part */
        /* a compose sequence. */
        if ((KEV.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))
            && !(KEV.dwControlKeyState & (LEFT_CTRL_PRESSED |
            RIGHT_CTRL_PRESSED)) && KEV.bKeyDown) {
          handle->last_key[0] = '\033';
          prefix_len = 1;
        } else {
          prefix_len = 0;
        }

        if (KEV.uChar.UnicodeChar >= 0xDC00 &&
            KEV.uChar.UnicodeChar < 0xE000) {
          /* UTF-16 surrogate pair */
          WCHAR utf16_buffer[2] = { handle->last_utf16_high_surrogate,
                                    KEV.uChar.UnicodeChar};
          char_len = WideCharToMultiByte(CP_UTF8,
                                         0,
                                         utf16_buffer,
                                         2,
                                         &handle->last_key[prefix_len],
                                         sizeof handle->last_key,
                                         NULL,
                                         NULL);
        } else {
          /* Single UTF-16 character */
          char_len = WideCharToMultiByte(CP_UTF8,
                                         0,
                                         &KEV.uChar.UnicodeChar,
                                         1,
                                         &handle->last_key[prefix_len],
                                         sizeof handle->last_key,
                                         NULL,
                                         NULL);
        }

        /* Whatever happened, the last character wasn't a high surrogate. */
        handle->last_utf16_high_surrogate = 0;

        /* If the utf16 character(s) couldn't be converted something must */
        /* be wrong. */
        if (!char_len) {
          uv__set_sys_error(loop, GetLastError());
          handle->flags &= ~UV_HANDLE_READING;
          handle->read_cb((uv_stream_t*) handle, -1, buf);
          goto out;
        }

        handle->last_key_len = (unsigned char) (prefix_len + char_len);
        handle->last_key_offset = 0;
        continue;

      } else {
        /* Function key pressed */
        const char* vt100;
        size_t prefix_len, vt100_len;

        vt100 = get_vt100_fn_key(KEV.wVirtualKeyCode,
                                  !!(KEV.dwControlKeyState & SHIFT_PRESSED),
                                  !!(KEV.dwControlKeyState & (
                                    LEFT_CTRL_PRESSED |
                                    RIGHT_CTRL_PRESSED)),
                                  &vt100_len);

        /* If we were unable to map to a vt100 sequence, just ignore. */
        if (!vt100) {
          continue;
        }

        /* Prefix with \x033 when the alt key was held. */
        if (KEV.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
          handle->last_key[0] = '\033';
          prefix_len = 1;
        } else {
          prefix_len = 0;
        }

        /* Copy the vt100 sequence to the handle buffer. */
        assert(prefix_len + vt100_len < sizeof handle->last_key);
        memcpy(&handle->last_key[prefix_len], vt100, vt100_len);

        handle->last_key_len = (unsigned char) (prefix_len + vt100_len);
        handle->last_key_offset = 0;
        continue;
      }
    } else {
      /* Copy any bytes left from the last keypress to the user buffer. */
      if (handle->last_key_offset < handle->last_key_len) {
        /* Allocate a buffer if needed */
        if (buf_used == 0) {
          buf = handle->alloc_cb((uv_handle_t*) handle, 1024);
        }

        buf.base[buf_used++] = handle->last_key[handle->last_key_offset++];

        /* If the buffer is full, emit it */
        if (buf_used == buf.len) {
          handle->read_cb((uv_stream_t*) handle, buf_used, buf);
          buf = uv_null_buf_;
          buf_used = 0;
        }

        continue;
      }

      /* Apply dwRepeat from the last input record. */
      if (--KEV.wRepeatCount > 0) {
        handle->last_key_offset = 0;
        continue;
      }

      handle->last_key_len = 0;
      continue;
    }
  }

  /* Send the buffer back to the user */
  if (buf_used > 0) {
    handle->read_cb((uv_stream_t*) handle, buf_used, buf);
  }

 out:
  /* Wait for more input events. */
  if ((handle->flags & UV_HANDLE_READING) &&
      !(handle->flags & UV_HANDLE_READ_PENDING)) {
    uv_tty_queue_read(loop, handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);

#undef KEV
}



void uv_process_tty_read_line_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_req_t* req) {
  uv_buf_t buf;

  assert(handle->type == UV_TTY);

  buf = handle->read_line_buffer;

  handle->flags &= ~UV_HANDLE_READ_PENDING;
  handle->read_line_buffer = uv_null_buf_;

  if (!REQ_SUCCESS(req)) {
    /* Read was not successful */
    if ((handle->flags & UV_HANDLE_READING) &&
        !(handle->flags & UV_HANDLE_TTY_RAW)) {
      /* Real error */
      handle->flags &= ~UV_HANDLE_READING;
      uv__set_sys_error(loop, GET_REQ_ERROR(req));
      handle->read_cb((uv_stream_t*) handle, -1, buf);
    } else {
      /* The read was cancelled, or whatever we don't care */
      uv__set_sys_error(loop, WSAEWOULDBLOCK); /* maps to UV_EAGAIN */
      handle->read_cb((uv_stream_t*) handle, 0, buf);
    }

  } else {
    /* Read successful */
    /* TODO: read unicode, convert to utf-8 */
    DWORD bytes = req->overlapped.InternalHigh;
    if (bytes == 0) {
      uv__set_sys_error(loop, WSAEWOULDBLOCK); /* maps to UV_EAGAIN */
    }
    handle->read_cb((uv_stream_t*) handle, bytes, buf);
  }

  /* Wait for more input events. */
  if ((handle->flags & UV_HANDLE_READING) &&
      !(handle->flags & UV_HANDLE_READ_PENDING)) {
    uv_tty_queue_read(loop, handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_tty_read_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_req_t* req) {

  /* If the read_line_buffer member is zero, it must have been an raw read. */
  /* Otherwise it was a line-buffered read. */
  /* FIXME: This is quite obscure. Use a flag or something. */
  if (handle->read_line_buffer.len == 0) {
    uv_process_tty_read_raw_req(loop, handle, req);
  } else {
    uv_process_tty_read_line_req(loop, handle, req);
  }
}


int uv_tty_read_start(uv_tty_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb) {
  uv_loop_t* loop = handle->loop;

  handle->flags |= UV_HANDLE_READING;
  handle->read_cb = read_cb;
  handle->alloc_cb = alloc_cb;

  /* If reading was stopped and then started again, there could still be a */
  /* read request pending. */
  if (handle->flags & UV_HANDLE_READ_PENDING) {
    return 0;
  }

  /* Maybe the user stopped reading half-way while processing key events. */
  /* Short-circuit if this could be the case. */
  if (handle->last_key_len > 0) {
    SET_REQ_SUCCESS(&handle->read_req);
    uv_insert_pending_req(handle->loop, (uv_req_t*) &handle->read_req);
    return -1;
  }

  uv_tty_queue_read(loop, handle);

  return 0;
}


int uv_tty_read_stop(uv_tty_t* handle) {
  handle->flags &= ~UV_HANDLE_READING;

  /* Cancel raw read */
  if ((handle->flags & UV_HANDLE_READ_PENDING) &&
      (handle->flags & UV_HANDLE_TTY_RAW)) {
    /* Write some bullshit event to force the console wait to return. */
    INPUT_RECORD record;
    DWORD written;
    memset(&record, 0, sizeof record);
    if (!WriteConsoleInputW(handle->handle, &record, 1, &written)) {
      uv__set_sys_error(handle->loop, GetLastError());
      return -1;
    }
  }

  /* Cancel line-buffered read */
  if (handle->read_line_handle != NULL) {
    /* Closing this handle will cancel the ReadConsole operation */
    CloseHandle(handle->read_line_handle);
    handle->read_line_handle = NULL;
  }


  return 0;
}


static void uv_tty_update_virtual_window(CONSOLE_SCREEN_BUFFER_INFO* info) {
  uv_tty_virtual_height = info->srWindow.Bottom - info->srWindow.Top + 1;
  uv_tty_virtual_width = info->dwSize.X;

  /* Recompute virtual window offset row. */
  if (uv_tty_virtual_offset == -1) {
    uv_tty_virtual_offset = info->dwCursorPosition.Y;
  } else if (uv_tty_virtual_offset < info->dwCursorPosition.Y -
             uv_tty_virtual_height + 1) {
    /* If suddenly find the cursor outside of the virtual window, it must */
    /* have somehow scrolled. Update the virtual window offset. */
    uv_tty_virtual_offset = info->dwCursorPosition.Y -
                            uv_tty_virtual_height + 1;
  }
  if (uv_tty_virtual_offset + uv_tty_virtual_height > info->dwSize.Y) {
    uv_tty_virtual_offset = info->dwSize.Y - uv_tty_virtual_height;
  }
  if (uv_tty_virtual_offset < 0) {
    uv_tty_virtual_offset = 0;
  }
}


static COORD uv_tty_make_real_coord(uv_tty_t* handle,
    CONSOLE_SCREEN_BUFFER_INFO* info, int x, unsigned char x_relative, int y,
    unsigned char y_relative) {
  COORD result;

  uv_tty_update_virtual_window(info);

  /* Adjust y position */
  if (y_relative) {
    y = info->dwCursorPosition.Y + y;
  } else {
    y = uv_tty_virtual_offset + y;
  }
  /* Clip y to virtual client rectangle */
  if (y < uv_tty_virtual_offset) {
    y = uv_tty_virtual_offset;
  } else if (y >= uv_tty_virtual_offset + uv_tty_virtual_height) {
    y = uv_tty_virtual_offset + uv_tty_virtual_height - 1;
  }

  /* Adjust x */
  if (x_relative) {
    x = info->dwCursorPosition.X + x;
  }
  /* Clip x */
  if (x < 0) {
    x = 0;
  } else if (x >= uv_tty_virtual_width) {
    x = uv_tty_virtual_width - 1;
  }

  result.X = (unsigned short) x;
  result.Y = (unsigned short) y;
  return result;
}


static int uv_tty_emit_text(uv_tty_t* handle, WCHAR buffer[], DWORD length,
    DWORD* error) {
  DWORD written;

  if (*error != ERROR_SUCCESS) {
    return -1;
  }

  if (!WriteConsoleW(handle->handle,
                     (void*) buffer,
                     length,
                     &written,
                     NULL)) {
    *error = GetLastError();
    return -1;
  }

  return 0;
}


static int uv_tty_move_caret(uv_tty_t* handle, int x, unsigned char x_relative,
    int y, unsigned char y_relative, DWORD* error) {
  CONSOLE_SCREEN_BUFFER_INFO info;
  COORD pos;

  if (*error != ERROR_SUCCESS) {
    return -1;
  }

 retry:
  if (!GetConsoleScreenBufferInfo(handle->handle, &info)) {
    *error = GetLastError();
  }

  pos = uv_tty_make_real_coord(handle, &info, x, x_relative, y, y_relative);

  if (!SetConsoleCursorPosition(handle->handle, pos)) {
    if (GetLastError() == ERROR_INVALID_PARAMETER) {
      /* The console may be resized - retry */
      goto retry;
    } else {
      *error = GetLastError();
      return -1;
    }
  }

  return 0;
}


static int uv_tty_reset(uv_tty_t* handle, DWORD* error) {
  const COORD origin = {0, 0};
  const WORD char_attrs = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_RED;
  CONSOLE_SCREEN_BUFFER_INFO info;
  DWORD count, written;

  if (*error != ERROR_SUCCESS) {
    return -1;
  }

  /* Reset original text attributes. */
  if (!SetConsoleTextAttribute(handle->handle, char_attrs)) {
    *error = GetLastError();
    return -1;
  }

  /* Move the cursor position to (0, 0). */
  if (!SetConsoleCursorPosition(handle->handle, origin)) {
    *error = GetLastError();
    return -1;
  }

  /* Clear the screen buffer. */
 retry:
  if (!GetConsoleScreenBufferInfo(handle->handle, &info)) {
    *error = GetLastError();
    return -1;
  }

  count = info.dwSize.X * info.dwSize.Y;

  if (!(FillConsoleOutputCharacterW(handle->handle,
                              L'\x20',
                              count,
                              origin,
                              &written) &&
        FillConsoleOutputAttribute(handle->handle,
                                   char_attrs,
                                   written,
                                   origin,
                                   &written))) {
    if (GetLastError() == ERROR_INVALID_PARAMETER) {
      /* The console may be resized - retry */
      goto retry;
    } else {
      *error = GetLastError();
      return -1;
    }
  }

  /* Move the virtual window up to the top. */
  uv_tty_virtual_offset = 0;
  uv_tty_update_virtual_window(&info);

  return 0;
}


static int uv_tty_clear(uv_tty_t* handle, int dir, char entire_screen,
    DWORD* error) {
  unsigned short argc = handle->ansi_csi_argc;
  unsigned short* argv = handle->ansi_csi_argv;

  CONSOLE_SCREEN_BUFFER_INFO info;
  COORD start, end;
  DWORD count, written;

  int x1, x2, y1, y2;
  int x1r, x2r, y1r, y2r;

  if (*error != ERROR_SUCCESS) {
    return -1;
  }

  if (dir == 0) {
    /* Clear from current position */
    x1 = 0;
    x1r = 1;
  } else {
    /* Clear from column 0 */
    x1 = 0;
    x1r = 0;
  }

  if (dir == 1) {
    /* Clear to current position */
    x2 = 0;
    x2r = 1;
  } else {
    /* Clear to end of row. We pretend the console is 65536 characters wide, */
    /* uv_tty_make_real_coord will clip it to the actual console width. */
    x2 = 0xffff;
    x2r = 0;
  }

  if (!entire_screen) {
    /* Stay on our own row */
    y1 = y2 = 0;
    y1r = y2r = 1;
  } else {
    /* Apply columns direction to row */
    y1 = x1;
    y1r = x1r;
    y2 = x2;
    y2r = x2r;
  }

 retry:
  if (!GetConsoleScreenBufferInfo(handle->handle, &info)) {
    *error = GetLastError();
    return -1;
  }

  start = uv_tty_make_real_coord(handle, &info, x1, x1r, y1, y1r);
  end = uv_tty_make_real_coord(handle, &info, x2, x2r, y2, y2r);
  count = (end.Y * info.dwSize.X + end.X) -
          (start.Y * info.dwSize.X + start.X) + 1;

  if (!(FillConsoleOutputCharacterW(handle->handle,
                              L'\x20',
                              count,
                              start,
                              &written) &&
        FillConsoleOutputAttribute(handle->handle,
                                   info.wAttributes,
                                   written,
                                   start,
                                   &written))) {
    if (GetLastError() == ERROR_INVALID_PARAMETER) {
      /* The console may be resized - retry */
      goto retry;
    } else {
      *error = GetLastError();
      return -1;
    }
  }

  return 0;
}


static int uv_tty_set_style(uv_tty_t* handle, DWORD* error) {
  unsigned short argc = handle->ansi_csi_argc;
  unsigned short* argv = handle->ansi_csi_argv;
  int i;
  CONSOLE_SCREEN_BUFFER_INFO info;

  char fg_color = -1, bg_color = -1;
  char fg_bright = -1, bg_bright = -1;

  if (argc == 0) {
    /* Reset mode */
    fg_color = 7;
    bg_color = 0;
    fg_bright = 0;
    bg_bright = 0;
  }

  for (i = 0; i < argc; i++) {
    short arg = argv[i];

    if (arg == 0) {
      /* Reset mode */
      fg_color = 7;
      bg_color = 0;
      fg_bright = 0;
      bg_bright = 0;

    } else if (arg == 1) {
      /* Foreground bright on */
      fg_bright = 1;

    } else if (arg == 2) {
      /* Both bright off */
      fg_bright = 0;
      bg_bright = 0;

    } else if (arg == 5) {
      /* Background bright on */
      bg_bright = 1;

    } else if (arg == 21 || arg == 22) {
      /* Foreground bright off */
      fg_bright = 0;

    } else if (arg == 25) {
      /* Background bright off */
      bg_bright = 0;

    } else if (arg >= 30 && arg <= 37) {
      /* Set foreground color */
      fg_color = arg - 30;

    } else if (arg == 39) {
      /* Default text color */
      fg_color = 7;

    } else if (arg >= 40 && arg <= 47) {
      /* Set background color */
      bg_color = arg - 40;

    } else if (arg ==  49) {
      /* Default background color */
      bg_color = 0;

    } else if (arg >= 90 && arg <= 97) {
      /* Set bold foreground color */
      fg_bright = 1;
      fg_color = arg - 90;

    } else if (arg >= 100 && arg <= 107) {
      /* Set bold background color */
      bg_bright = 1;
      bg_color = arg - 100;

    }
  }

  if (fg_color == -1 && bg_color == -1 && fg_bright == -1 &&
      bg_bright == -1) {
    /* Nothing changed */
    return 0;
  }

  if (!GetConsoleScreenBufferInfo(handle->handle, &info)) {
    *error = GetLastError();
    return -1;
  }

  if (fg_color != -1) {
    info.wAttributes &= ~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    if (fg_color & 1) info.wAttributes |= FOREGROUND_RED;
    if (fg_color & 2) info.wAttributes |= FOREGROUND_GREEN;
    if (fg_color & 4) info.wAttributes |= FOREGROUND_BLUE;
  }

  if (fg_bright != -1) {
    if (fg_bright) {
      info.wAttributes |= FOREGROUND_INTENSITY;
    } else {
      info.wAttributes &= ~FOREGROUND_INTENSITY;
    }
  }

  if (bg_color != -1) {
    info.wAttributes &= ~(BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE);
    if (bg_color & 1) info.wAttributes |= BACKGROUND_RED;
    if (bg_color & 2) info.wAttributes |= BACKGROUND_GREEN;
    if (bg_color & 4) info.wAttributes |= BACKGROUND_BLUE;
  }

  if (bg_bright != -1) {
    if (bg_bright) {
      info.wAttributes |= BACKGROUND_INTENSITY;
    } else {
      info.wAttributes &= ~BACKGROUND_INTENSITY;
    }
  }

  if (!SetConsoleTextAttribute(handle->handle, info.wAttributes)) {
    *error = GetLastError();
    return -1;
  }

  return 0;
}


static int uv_tty_save_state(uv_tty_t* handle, unsigned char save_attributes,
    DWORD* error) {
  CONSOLE_SCREEN_BUFFER_INFO info;

  if (*error != ERROR_SUCCESS) {
    return -1;
  }

  if (!GetConsoleScreenBufferInfo(handle->handle, &info)) {
    *error = GetLastError();
    return -1;
  }

  uv_tty_update_virtual_window(&info);

  handle->saved_position.X = info.dwCursorPosition.X;
  handle->saved_position.Y = info.dwCursorPosition.Y - uv_tty_virtual_offset;
  handle->flags |= UV_HANDLE_TTY_SAVED_POSITION;

  if (save_attributes) {
    handle->saved_attributes = info.wAttributes &
        (FOREGROUND_INTENSITY | BACKGROUND_INTENSITY);
    handle->flags |= UV_HANDLE_TTY_SAVED_ATTRIBUTES;
  }

  return 0;
}


static int uv_tty_restore_state(uv_tty_t* handle,
    unsigned char restore_attributes, DWORD* error) {
  CONSOLE_SCREEN_BUFFER_INFO info;
  WORD new_attributes;

  if (*error != ERROR_SUCCESS) {
    return -1;
  }

  if (handle->flags & UV_HANDLE_TTY_SAVED_POSITION) {
    if (uv_tty_move_caret(handle,
                          handle->saved_position.X,
                          0,
                          handle->saved_position.Y,
                          0,
                          error) != 0) {
      return -1;
    }
  }

  if (restore_attributes &&
      (handle->flags & UV_HANDLE_TTY_SAVED_ATTRIBUTES)) {
    if (!GetConsoleScreenBufferInfo(handle->handle, &info)) {
      *error = GetLastError();
      return -1;
    }

    new_attributes = info.wAttributes;
    new_attributes &= ~(FOREGROUND_INTENSITY | BACKGROUND_INTENSITY);
    new_attributes |= handle->saved_attributes;

    if (!SetConsoleTextAttribute(handle->handle, new_attributes)) {
      *error = GetLastError();
      return -1;
    }
  }

  return 0;
}


static int uv_tty_write_bufs(uv_tty_t* handle, uv_buf_t bufs[], int bufcnt,
    DWORD* error) {
  /* We can only write 8k characters at a time. Windows can't handle */
  /* much more characters in a single console write anyway. */
  WCHAR utf16_buf[8192];
  DWORD utf16_buf_used = 0;
  int i;

#define FLUSH_TEXT()                                                \
  do {                                                              \
    if (utf16_buf_used > 0) {                                       \
      uv_tty_emit_text(handle, utf16_buf, utf16_buf_used, error);   \
      utf16_buf_used = 0;                                           \
    }                                                               \
  } while (0)

  /* Cache for fast access */
  unsigned char utf8_bytes_left = handle->utf8_bytes_left;
  unsigned int utf8_codepoint = handle->utf8_codepoint;
  unsigned char previous_eol = handle->previous_eol;
  unsigned char ansi_parser_state = handle->ansi_parser_state;

  /* Store the error here. If we encounter an error, stop trying to do i/o */
  /* but keep parsing the buffer so we leave the parser in a consistent */
  /* state. */
  *error = ERROR_SUCCESS;

  EnterCriticalSection(&uv_tty_output_lock);

  for (i = 0; i < bufcnt; i++) {
    uv_buf_t buf = bufs[i];
    unsigned int j;

    for (j = 0; j < buf.len; j++) {
      unsigned char c = buf.base[j];

      /* Run the character through the utf8 decoder We happily accept non */
      /* shortest form encodings and invalid code points - there's no real */
      /* harm that can be done. */
      if (utf8_bytes_left == 0) {
        /* Read utf-8 start byte */
        DWORD first_zero_bit;
        unsigned char not_c = ~c;
#ifdef _MSC_VER /* msvc */
        if (_BitScanReverse(&first_zero_bit, not_c)) {
#else /* assume gcc */
        if (c != 0) {
          first_zero_bit = (sizeof(int) * 8) - 1 - __builtin_clz(not_c);
#endif
          if (first_zero_bit == 7) {
            /* Ascii - pass right through */
            utf8_codepoint = (unsigned int) c;

          } else if (first_zero_bit <= 5) {
            /* Multibyte sequence */
            utf8_codepoint = (0xff >> (8 - first_zero_bit)) & c;
            utf8_bytes_left = (char) (6 - first_zero_bit);

          } else {
            /* Invalid continuation */
            utf8_codepoint = UNICODE_REPLACEMENT_CHARACTER;
          }

        } else {
          /* 0xff -- invalid */
          utf8_codepoint = UNICODE_REPLACEMENT_CHARACTER;
        }

      } else if ((c & 0xc0) == 0x80) {
        /* Valid continuation of utf-8 multibyte sequence */
        utf8_bytes_left--;
        utf8_codepoint <<= 6;
        utf8_codepoint |= ((unsigned int) c & 0x3f);

      } else {
        /* Start byte where continuation was expected. */
        utf8_bytes_left = 0;
        utf8_codepoint = UNICODE_REPLACEMENT_CHARACTER;
        /* Patch buf offset so this character will be parsed again as a */
        /* start byte. */
        j--;
      }

      /* Maybe we need to parse more bytes to find a character. */
      if (utf8_bytes_left != 0) {
        continue;
      }

      /* Parse vt100/ansi escape codes */
      if (ansi_parser_state == ANSI_NORMAL) {
        switch (utf8_codepoint) {
          case '\033':
            ansi_parser_state = ANSI_ESCAPE_SEEN;
            continue;

          case 0233:
            ansi_parser_state = ANSI_CSI;
            handle->ansi_csi_argc = 0;
            continue;
        }

      } else if (ansi_parser_state == ANSI_ESCAPE_SEEN) {
        switch (utf8_codepoint) {
          case '[':
            ansi_parser_state = ANSI_CSI;
            handle->ansi_csi_argc = 0;
            continue;

          case '^':
          case '_':
          case 'P':
          case ']':
            /* Not supported, but we'll have to parse until we see a stop */
            /* code, e.g. ESC \ or BEL. */
            ansi_parser_state = ANSI_ST_CONTROL;
            continue;

          case '\033':
            /* Ignore double escape. */
            continue;

          case 'c':
            /* Full console reset. */
            FLUSH_TEXT();
            uv_tty_reset(handle, error);
            ansi_parser_state = ANSI_NORMAL;
            continue;

          case '7':
            /* Save the cursor position and text attributes. */
            FLUSH_TEXT();
            uv_tty_save_state(handle, 1, error);
            ansi_parser_state = ANSI_NORMAL;
            continue;

           case '8':
            /* Restore the cursor position and text attributes */
            FLUSH_TEXT();
            uv_tty_restore_state(handle, 1, error);
            ansi_parser_state = ANSI_NORMAL;
            continue;

          default:
            if (utf8_codepoint >= '@' && utf8_codepoint <= '_') {
              /* Single-char control. */
              ansi_parser_state = ANSI_NORMAL;
              continue;
            } else {
              /* Invalid - proceed as normal, */
              ansi_parser_state = ANSI_NORMAL;
            }
        }

      } else if (ansi_parser_state & ANSI_CSI) {
        if (!(ansi_parser_state & ANSI_IGNORE)) {
          if (utf8_codepoint >= '0' && utf8_codepoint <= '9') {
            /* Parsing a numerical argument */

            if (!(ansi_parser_state & ANSI_IN_ARG)) {
              /* We were not currently parsing a number */

              /* Check for too many arguments */
              if (handle->ansi_csi_argc >= ARRAY_SIZE(handle->ansi_csi_argv)) {
                ansi_parser_state |= ANSI_IGNORE;
                continue;
              }

              ansi_parser_state |= ANSI_IN_ARG;
              handle->ansi_csi_argc++;
              handle->ansi_csi_argv[handle->ansi_csi_argc - 1] =
                  (unsigned short) utf8_codepoint - '0';
              continue;
            } else {
              /* We were already parsing a number. Parse next digit. */
              uint32_t value = 10 *
                  handle->ansi_csi_argv[handle->ansi_csi_argc - 1];

              /* Check for overflow. */
              if (value > UINT16_MAX) {
                ansi_parser_state |= ANSI_IGNORE;
                continue;
              }

               handle->ansi_csi_argv[handle->ansi_csi_argc - 1] =
                   (unsigned short) value + (utf8_codepoint - '0');
               continue;
            }

          } else if (utf8_codepoint == ';') {
            /* Denotes the end of an argument. */
            if (ansi_parser_state & ANSI_IN_ARG) {
              ansi_parser_state &= ~ANSI_IN_ARG;
              continue;

            } else {
              /* If ANSI_IN_ARG is not set, add another argument and */
              /* default it to 0. */
              /* Check for too many arguments */
              if (handle->ansi_csi_argc >= ARRAY_SIZE(handle->ansi_csi_argv)) {
                ansi_parser_state |= ANSI_IGNORE;
                continue;
              }

              handle->ansi_csi_argc++;
              handle->ansi_csi_argv[handle->ansi_csi_argc - 1] = 0;
              continue;
            }

          } else if (utf8_codepoint >= '@' && utf8_codepoint <= '~' &&
                     (handle->ansi_csi_argc > 0 || utf8_codepoint != '[')) {
            int x, y, d;

            /* Command byte */
            switch (utf8_codepoint) {
              case 'A':
                /* cursor up */
                FLUSH_TEXT();
                y = -(handle->ansi_csi_argc ? handle->ansi_csi_argv[0] : 1);
                uv_tty_move_caret(handle, 0, 1, y, 1, error);
                break;

              case 'B':
                /* cursor down */
                FLUSH_TEXT();
                y = handle->ansi_csi_argc ? handle->ansi_csi_argv[0] : 1;
                uv_tty_move_caret(handle, 0, 1, y, 1, error);
                break;

              case 'C':
                /* cursor forward */
                FLUSH_TEXT();
                x = handle->ansi_csi_argc ? handle->ansi_csi_argv[0] : 1;
                uv_tty_move_caret(handle, x, 1, 0, 1, error);
                break;

              case 'D':
                /* cursor back */
                FLUSH_TEXT();
                x = -(handle->ansi_csi_argc ? handle->ansi_csi_argv[0] : 1);
                uv_tty_move_caret(handle, x, 1, 0, 1, error);
                break;

              case 'E':
                /* cursor next line */
                FLUSH_TEXT();
                y = handle->ansi_csi_argc ? handle->ansi_csi_argv[0] : 1;
                uv_tty_move_caret(handle, 0, 0, y, 1, error);
                break;

              case 'F':
                /* cursor previous line */
                FLUSH_TEXT();
                y = -(handle->ansi_csi_argc ? handle->ansi_csi_argv[0] : 1);
                uv_tty_move_caret(handle, 0, 0, y, 1, error);
                break;

              case 'G':
                /* cursor horizontal move absolute */
                FLUSH_TEXT();
                x = (handle->ansi_csi_argc >= 1 && handle->ansi_csi_argv[0])
                  ? handle->ansi_csi_argv[0] - 1 : 0;
                uv_tty_move_caret(handle, x, 0, 0, 1, error);
                break;

              case 'H':
              case 'f':
                /* cursor move absolute */
                FLUSH_TEXT();
                y = (handle->ansi_csi_argc >= 1 && handle->ansi_csi_argv[0])
                  ? handle->ansi_csi_argv[0] - 1 : 0;
                x = (handle->ansi_csi_argc >= 2 && handle->ansi_csi_argv[1])
                  ? handle->ansi_csi_argv[1] - 1 : 0;
                uv_tty_move_caret(handle, x, 0, y, 0, error);
                break;

              case 'J':
                /* Erase screen */
                FLUSH_TEXT();
                d = handle->ansi_csi_argc ? handle->ansi_csi_argv[0] : 0;
                if (d >= 0 && d <= 2) {
                  uv_tty_clear(handle, d, 1, error);
                }
                break;

              case 'K':
                /* Erase line */
                FLUSH_TEXT();
                d = handle->ansi_csi_argc ? handle->ansi_csi_argv[0] : 0;
                if (d >= 0 && d <= 2) {
                  uv_tty_clear(handle, d, 0, error);
                }
                break;

              case 'm':
                /* Set style */
                FLUSH_TEXT();
                uv_tty_set_style(handle, error);
                break;

              case 's':
                /* Save the cursor position. */
                FLUSH_TEXT();
                uv_tty_save_state(handle, 0, error);
                break;

              case 'u':
                /* Restore the cursor position */
                FLUSH_TEXT();
                uv_tty_restore_state(handle, 0, error);
                break;
            }

            /* Sequence ended - go back to normal state. */
            ansi_parser_state = ANSI_NORMAL;
            continue;

          } else {
            /* We don't support commands that use private mode characters or */
            /* intermediaries. Ignore the rest of the sequence. */
            ansi_parser_state |= ANSI_IGNORE;
            continue;
          }
        } else {
          /* We're ignoring this command. Stop only on command character. */
          if (utf8_codepoint >= '@' && utf8_codepoint <= '~') {
            ansi_parser_state = ANSI_NORMAL;
          }
          continue;
        }

      } else if (ansi_parser_state & ANSI_ST_CONTROL) {
        /* Unsupported control code */
        /* Ignore everything until we see BEL or ESC \ */
        if (ansi_parser_state & ANSI_IN_STRING) {
          if (!(ansi_parser_state & ANSI_BACKSLASH_SEEN)) {
            if (utf8_codepoint == '"') {
              ansi_parser_state &= ~ANSI_IN_STRING;
            } else if (utf8_codepoint == '\\') {
              ansi_parser_state |= ANSI_BACKSLASH_SEEN;
            }
          } else {
            ansi_parser_state &= ~ANSI_BACKSLASH_SEEN;
          }
        } else {
          if (utf8_codepoint == '\007' || (utf8_codepoint == '\\' &&
              (ansi_parser_state & ANSI_ESCAPE_SEEN))) {
            /* End of sequence */
            ansi_parser_state = ANSI_NORMAL;
          } else if (utf8_codepoint == '\033') {
            /* Escape character */
            ansi_parser_state |= ANSI_ESCAPE_SEEN;
          } else if (utf8_codepoint == '"') {
             /* String starting */
            ansi_parser_state |= ANSI_IN_STRING;
            ansi_parser_state &= ~ANSI_ESCAPE_SEEN;
            ansi_parser_state &= ~ANSI_BACKSLASH_SEEN;
          } else {
            ansi_parser_state &= ~ANSI_ESCAPE_SEEN;
          }
        }
        continue;
      } else {
        /* Inconsistent state */
        abort();
      }

      /* We wouldn't mind emitting utf-16 surrogate pairs. Too bad, the */
      /* windows console doesn't really support UTF-16, so just emit the */
      /* replacement character. */
      if (utf8_codepoint > 0xffff) {
        utf8_codepoint = UNICODE_REPLACEMENT_CHARACTER;
      }

      if (utf8_codepoint == 0x0a || utf8_codepoint == 0x0d) {
        /* EOL conversion - emit \r\n, when we see either \r or \n. */
        /* If a \n immediately follows a \r or vice versa, ignore it. */
        if (previous_eol == 0 || utf8_codepoint == previous_eol) {
          /* If there's no room in the utf16 buf, flush it first. */
          if (2 > ARRAY_SIZE(utf16_buf) - utf16_buf_used) {
            uv_tty_emit_text(handle, utf16_buf, utf16_buf_used, error);
            utf16_buf_used = 0;
          }

          utf16_buf[utf16_buf_used++] = L'\r';
          utf16_buf[utf16_buf_used++] = L'\n';
          previous_eol = (char) utf8_codepoint;
        } else {
          /* Ignore this newline, but don't ignore later ones. */
          previous_eol = 0;
        }

      } else if (utf8_codepoint <= 0xffff) {
        /* Encode character into utf-16 buffer. */

        /* If there's no room in the utf16 buf, flush it first. */
        if (1 > ARRAY_SIZE(utf16_buf) - utf16_buf_used) {
          uv_tty_emit_text(handle, utf16_buf, utf16_buf_used, error);
          utf16_buf_used = 0;
        }

        utf16_buf[utf16_buf_used++] = (WCHAR) utf8_codepoint;
        previous_eol = 0;
      }
    }
  }

  /* Flush remaining characters */
  FLUSH_TEXT();

  /* Copy cached values back to struct. */
  handle->utf8_bytes_left = utf8_bytes_left;
  handle->utf8_codepoint = utf8_codepoint;
  handle->previous_eol = previous_eol;
  handle->ansi_parser_state = ansi_parser_state;

  LeaveCriticalSection(&uv_tty_output_lock);

  if (*error == STATUS_SUCCESS) {
    return 0;
  } else {
    return -1;
  }

#undef FLUSH_TEXT
}


int uv_tty_write(uv_loop_t* loop, uv_write_t* req, uv_tty_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_write_cb cb) {
  DWORD error;

  if ((handle->flags & UV_HANDLE_SHUTTING) ||
      (handle->flags & UV_HANDLE_CLOSING)) {
    uv__set_sys_error(loop, WSAESHUTDOWN);
    return -1;
  }

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_WRITE;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;

  handle->reqs_pending++;
  handle->write_reqs_pending++;

  req->queued_bytes = 0;

  if (!uv_tty_write_bufs(handle, bufs, bufcnt, &error)) {
    SET_REQ_SUCCESS(req);
  } else {
    SET_REQ_ERROR(req, error);
  }

  uv_insert_pending_req(loop, (uv_req_t*) req);

  return 0;
}


void uv_process_tty_write_req(uv_loop_t* loop, uv_tty_t* handle,
  uv_write_t* req) {

  handle->write_queue_size -= req->queued_bytes;

  if (req->cb) {
    uv__set_sys_error(loop, GET_REQ_ERROR(req));
    ((uv_write_cb)req->cb)(req, loop->last_err.code == UV_OK ? 0 : -1);
  }

  handle->write_reqs_pending--;
  if (handle->flags & UV_HANDLE_SHUTTING &&
      handle->write_reqs_pending == 0) {
    uv_want_endgame(loop, (uv_handle_t*)handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_tty_close(uv_tty_t* handle) {
  uv_tty_read_stop(handle);
  CloseHandle(handle->handle);

  if (handle->reqs_pending == 0) {
    uv_want_endgame(handle->loop, (uv_handle_t*) handle);
  }
}


void uv_tty_endgame(uv_loop_t* loop, uv_tty_t* handle) {
  if (handle->flags & UV_HANDLE_CONNECTION &&
      handle->flags & UV_HANDLE_SHUTTING &&
      !(handle->flags & UV_HANDLE_SHUT) &&
      handle->write_reqs_pending == 0) {
    handle->flags |= UV_HANDLE_SHUT;

    /* TTY shutdown is really just a no-op */
    if (handle->shutdown_req->cb) {
      handle->shutdown_req->cb(handle->shutdown_req, 0);
    }

    DECREASE_PENDING_REQ_COUNT(handle);
    return;
  }

  if (handle->flags & UV_HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    /* The console handle duplicate used for line reading should be destroyed */
    /* by uv_tty_read_stop. */
    assert(handle->read_line_handle == NULL);

    /* The wait handle used for raw reading should be unregistered when the */
    /* wait callback runs. */
    assert(handle->read_raw_wait == NULL);

    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle);
    }

    uv_unref(loop);
  }
}


/* TODO: remove me */
void uv_process_tty_accept_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_req_t* raw_req) {
  abort();
}


/* TODO: remove me */
void uv_process_tty_connect_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_connect_t* req) {
  abort();
}


void uv_tty_reset_mode() {
  /* Not necessary to do anything. */
  ;
}
