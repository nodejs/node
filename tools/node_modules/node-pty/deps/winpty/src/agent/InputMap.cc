// Copyright (c) 2011-2015 Ryan Prichard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "InputMap.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DebugShowInput.h"
#include "SimplePool.h"
#include "../shared/DebugClient.h"
#include "../shared/UnixCtrlChars.h"
#include "../shared/WinptyAssert.h"
#include "../shared/winpty_snprintf.h"

namespace {

static const char *getVirtualKeyString(int virtualKey)
{
    switch (virtualKey) {
#define WINPTY_GVKS_KEY(x) case VK_##x: return #x;
        WINPTY_GVKS_KEY(RBUTTON)    WINPTY_GVKS_KEY(F9)
        WINPTY_GVKS_KEY(CANCEL)     WINPTY_GVKS_KEY(F10)
        WINPTY_GVKS_KEY(MBUTTON)    WINPTY_GVKS_KEY(F11)
        WINPTY_GVKS_KEY(XBUTTON1)   WINPTY_GVKS_KEY(F12)
        WINPTY_GVKS_KEY(XBUTTON2)   WINPTY_GVKS_KEY(F13)
        WINPTY_GVKS_KEY(BACK)       WINPTY_GVKS_KEY(F14)
        WINPTY_GVKS_KEY(TAB)        WINPTY_GVKS_KEY(F15)
        WINPTY_GVKS_KEY(CLEAR)      WINPTY_GVKS_KEY(F16)
        WINPTY_GVKS_KEY(RETURN)     WINPTY_GVKS_KEY(F17)
        WINPTY_GVKS_KEY(SHIFT)      WINPTY_GVKS_KEY(F18)
        WINPTY_GVKS_KEY(CONTROL)    WINPTY_GVKS_KEY(F19)
        WINPTY_GVKS_KEY(MENU)       WINPTY_GVKS_KEY(F20)
        WINPTY_GVKS_KEY(PAUSE)      WINPTY_GVKS_KEY(F21)
        WINPTY_GVKS_KEY(CAPITAL)    WINPTY_GVKS_KEY(F22)
        WINPTY_GVKS_KEY(HANGUL)     WINPTY_GVKS_KEY(F23)
        WINPTY_GVKS_KEY(JUNJA)      WINPTY_GVKS_KEY(F24)
        WINPTY_GVKS_KEY(FINAL)      WINPTY_GVKS_KEY(NUMLOCK)
        WINPTY_GVKS_KEY(KANJI)      WINPTY_GVKS_KEY(SCROLL)
        WINPTY_GVKS_KEY(ESCAPE)     WINPTY_GVKS_KEY(LSHIFT)
        WINPTY_GVKS_KEY(CONVERT)    WINPTY_GVKS_KEY(RSHIFT)
        WINPTY_GVKS_KEY(NONCONVERT) WINPTY_GVKS_KEY(LCONTROL)
        WINPTY_GVKS_KEY(ACCEPT)     WINPTY_GVKS_KEY(RCONTROL)
        WINPTY_GVKS_KEY(MODECHANGE) WINPTY_GVKS_KEY(LMENU)
        WINPTY_GVKS_KEY(SPACE)      WINPTY_GVKS_KEY(RMENU)
        WINPTY_GVKS_KEY(PRIOR)      WINPTY_GVKS_KEY(BROWSER_BACK)
        WINPTY_GVKS_KEY(NEXT)       WINPTY_GVKS_KEY(BROWSER_FORWARD)
        WINPTY_GVKS_KEY(END)        WINPTY_GVKS_KEY(BROWSER_REFRESH)
        WINPTY_GVKS_KEY(HOME)       WINPTY_GVKS_KEY(BROWSER_STOP)
        WINPTY_GVKS_KEY(LEFT)       WINPTY_GVKS_KEY(BROWSER_SEARCH)
        WINPTY_GVKS_KEY(UP)         WINPTY_GVKS_KEY(BROWSER_FAVORITES)
        WINPTY_GVKS_KEY(RIGHT)      WINPTY_GVKS_KEY(BROWSER_HOME)
        WINPTY_GVKS_KEY(DOWN)       WINPTY_GVKS_KEY(VOLUME_MUTE)
        WINPTY_GVKS_KEY(SELECT)     WINPTY_GVKS_KEY(VOLUME_DOWN)
        WINPTY_GVKS_KEY(PRINT)      WINPTY_GVKS_KEY(VOLUME_UP)
        WINPTY_GVKS_KEY(EXECUTE)    WINPTY_GVKS_KEY(MEDIA_NEXT_TRACK)
        WINPTY_GVKS_KEY(SNAPSHOT)   WINPTY_GVKS_KEY(MEDIA_PREV_TRACK)
        WINPTY_GVKS_KEY(INSERT)     WINPTY_GVKS_KEY(MEDIA_STOP)
        WINPTY_GVKS_KEY(DELETE)     WINPTY_GVKS_KEY(MEDIA_PLAY_PAUSE)
        WINPTY_GVKS_KEY(HELP)       WINPTY_GVKS_KEY(LAUNCH_MAIL)
        WINPTY_GVKS_KEY(LWIN)       WINPTY_GVKS_KEY(LAUNCH_MEDIA_SELECT)
        WINPTY_GVKS_KEY(RWIN)       WINPTY_GVKS_KEY(LAUNCH_APP1)
        WINPTY_GVKS_KEY(APPS)       WINPTY_GVKS_KEY(LAUNCH_APP2)
        WINPTY_GVKS_KEY(SLEEP)      WINPTY_GVKS_KEY(OEM_1)
        WINPTY_GVKS_KEY(NUMPAD0)    WINPTY_GVKS_KEY(OEM_PLUS)
        WINPTY_GVKS_KEY(NUMPAD1)    WINPTY_GVKS_KEY(OEM_COMMA)
        WINPTY_GVKS_KEY(NUMPAD2)    WINPTY_GVKS_KEY(OEM_MINUS)
        WINPTY_GVKS_KEY(NUMPAD3)    WINPTY_GVKS_KEY(OEM_PERIOD)
        WINPTY_GVKS_KEY(NUMPAD4)    WINPTY_GVKS_KEY(OEM_2)
        WINPTY_GVKS_KEY(NUMPAD5)    WINPTY_GVKS_KEY(OEM_3)
        WINPTY_GVKS_KEY(NUMPAD6)    WINPTY_GVKS_KEY(OEM_4)
        WINPTY_GVKS_KEY(NUMPAD7)    WINPTY_GVKS_KEY(OEM_5)
        WINPTY_GVKS_KEY(NUMPAD8)    WINPTY_GVKS_KEY(OEM_6)
        WINPTY_GVKS_KEY(NUMPAD9)    WINPTY_GVKS_KEY(OEM_7)
        WINPTY_GVKS_KEY(MULTIPLY)   WINPTY_GVKS_KEY(OEM_8)
        WINPTY_GVKS_KEY(ADD)        WINPTY_GVKS_KEY(OEM_102)
        WINPTY_GVKS_KEY(SEPARATOR)  WINPTY_GVKS_KEY(PROCESSKEY)
        WINPTY_GVKS_KEY(SUBTRACT)   WINPTY_GVKS_KEY(PACKET)
        WINPTY_GVKS_KEY(DECIMAL)    WINPTY_GVKS_KEY(ATTN)
        WINPTY_GVKS_KEY(DIVIDE)     WINPTY_GVKS_KEY(CRSEL)
        WINPTY_GVKS_KEY(F1)         WINPTY_GVKS_KEY(EXSEL)
        WINPTY_GVKS_KEY(F2)         WINPTY_GVKS_KEY(EREOF)
        WINPTY_GVKS_KEY(F3)         WINPTY_GVKS_KEY(PLAY)
        WINPTY_GVKS_KEY(F4)         WINPTY_GVKS_KEY(ZOOM)
        WINPTY_GVKS_KEY(F5)         WINPTY_GVKS_KEY(NONAME)
        WINPTY_GVKS_KEY(F6)         WINPTY_GVKS_KEY(PA1)
        WINPTY_GVKS_KEY(F7)         WINPTY_GVKS_KEY(OEM_CLEAR)
        WINPTY_GVKS_KEY(F8)
#undef WINPTY_GVKS_KEY
        default:                        return NULL;
    }
}

} // anonymous namespace

std::string InputMap::Key::toString() const {
    std::string ret;
    ret += controlKeyStatePrefix(keyState);
    char buf[256];
    const char *vkString = getVirtualKeyString(virtualKey);
    if (vkString != NULL) {
        ret += vkString;
    } else if ((virtualKey >= 'A' && virtualKey <= 'Z') ||
               (virtualKey >= '0' && virtualKey <= '9')) {
        ret += static_cast<char>(virtualKey);
    } else {
        winpty_snprintf(buf, "%#x", virtualKey);
        ret += buf;
    }
    if (unicodeChar >= 32 && unicodeChar <= 126) {
        winpty_snprintf(buf, " ch='%c'",
                        static_cast<char>(unicodeChar));
    } else {
        winpty_snprintf(buf, " ch=%#x",
                        static_cast<unsigned int>(unicodeChar));
    }
    ret += buf;
    return ret;
}

void InputMap::set(const char *encoding, int encodingLen, const Key &key) {
    ASSERT(encodingLen > 0);
    setHelper(m_root, encoding, encodingLen, key);
}

void InputMap::setHelper(Node &node, const char *encoding, int encodingLen, const Key &key) {
    if (encodingLen == 0) {
        node.key = key;
    } else {
        setHelper(getOrCreateChild(node, encoding[0]), encoding + 1, encodingLen - 1, key);
    }
}

InputMap::Node &InputMap::getOrCreateChild(Node &node, unsigned char ch) {
    Node *ret = getChild(node, ch);
    if (ret != NULL) {
        return *ret;
    }
    if (node.childCount < Node::kTinyCount) {
        // Maintain sorted order for the sake of the InputMap dumping.
        int insertIndex = node.childCount;
        for (int i = 0; i < node.childCount; ++i) {
            if (ch < node.u.tiny.values[i]) {
                insertIndex = i;
                break;
            }
        }
        for (int j = node.childCount; j > insertIndex; --j) {
            node.u.tiny.values[j] = node.u.tiny.values[j - 1];
            node.u.tiny.children[j] = node.u.tiny.children[j - 1];
        }
        node.u.tiny.values[insertIndex] = ch;
        node.u.tiny.children[insertIndex] = ret = m_nodePool.alloc();
        ++node.childCount;
        return *ret;
    }
    if (node.childCount == Node::kTinyCount) {
        Branch *branch = m_branchPool.alloc();
        for (int i = 0; i < node.childCount; ++i) {
            branch->children[node.u.tiny.values[i]] = node.u.tiny.children[i];
        }
        node.u.branch = branch;
    }
    node.u.branch->children[ch] = ret = m_nodePool.alloc();
    ++node.childCount;
    return *ret;
}

// Find the longest matching key and node.
int InputMap::lookupKey(const char *input, int inputSize,
                        Key &keyOut, bool &incompleteOut) const {
    keyOut = kKeyZero;
    incompleteOut = false;

    const Node *node = &m_root;
    InputMap::Key longestMatch = kKeyZero;
    int longestMatchLen = 0;

    for (int i = 0; i < inputSize; ++i) {
        unsigned char ch = input[i];
        node = getChild(*node, ch);
        if (node == NULL) {
            keyOut = longestMatch;
            return longestMatchLen;
        } else if (node->hasKey()) {
            longestMatchLen = i + 1;
            longestMatch = node->key;
        }
    }
    keyOut = longestMatch;
    incompleteOut = node->childCount > 0;
    return longestMatchLen;
}

void InputMap::dumpInputMap() const {
    std::string encoding;
    dumpInputMapHelper(m_root, encoding);
}

void InputMap::dumpInputMapHelper(
        const Node &node, std::string &encoding) const {
    if (node.hasKey()) {
        trace("%s -> %s",
            encoding.c_str(),
            node.key.toString().c_str());
    }
    for (int i = 0; i < 256; ++i) {
        const Node *child = getChild(node, i);
        if (child != NULL) {
            size_t oldSize = encoding.size();
            if (!encoding.empty()) {
                encoding.push_back(' ');
            }
            char ctrlChar = decodeUnixCtrlChar(i);
            if (ctrlChar != '\0') {
                encoding.push_back('^');
                encoding.push_back(static_cast<char>(ctrlChar));
            } else if (i == ' ') {
                encoding.append("' '");
            } else {
                encoding.push_back(static_cast<char>(i));
            }
            dumpInputMapHelper(*child, encoding);
            encoding.resize(oldSize);
        }
    }
}
