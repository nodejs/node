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

#ifndef INPUT_MAP_H
#define INPUT_MAP_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "SimplePool.h"
#include "../shared/WinptyAssert.h"

class InputMap {
public:
    struct Key {
        uint16_t virtualKey;
        uint32_t unicodeChar;
        uint16_t keyState;

        std::string toString() const;
    };

private:
    struct Node;

    struct Branch {
        Branch() {
            memset(&children, 0, sizeof(children));
        }

        Node *children[256];
    };

    struct Node {
        Node() : childCount(0) {
            Key zeroKey = { 0, 0, 0 };
            key = zeroKey;
        }

        Key key;
        int childCount;
        enum { kTinyCount = 8 };
        union {
            Branch *branch;
            struct {
                unsigned char values[kTinyCount];
                Node *children[kTinyCount];
            } tiny;
        } u;

        bool hasKey() const {
            return key.virtualKey != 0 || key.unicodeChar != 0;
        }
    };

private:
    SimplePool<Node, 256> m_nodePool;
    SimplePool<Branch, 8> m_branchPool;
    Node m_root;

public:
    void set(const char *encoding, int encodingLen, const Key &key);
    int lookupKey(const char *input, int inputSize,
                  Key &keyOut, bool &incompleteOut) const;
    void dumpInputMap() const;

private:
    Node *getChild(Node &node, unsigned char ch) {
        return const_cast<Node*>(getChild(static_cast<const Node&>(node), ch));
    }

    const Node *getChild(const Node &node, unsigned char ch) const {
        if (node.childCount <= Node::kTinyCount) {
            for (int i = 0; i < node.childCount; ++i) {
                if (node.u.tiny.values[i] == ch) {
                    return node.u.tiny.children[i];
                }
            }
            return NULL;
        } else {
            return node.u.branch->children[ch];
        }
    }

    void setHelper(Node &node, const char *encoding, int encodingLen, const Key &key);
    Node &getOrCreateChild(Node &node, unsigned char ch);
    void dumpInputMapHelper(const Node &node, std::string &encoding) const;
};

const InputMap::Key kKeyZero = { 0, 0, 0 };

void dumpInputMap(InputMap &inputMap);

#endif // INPUT_MAP_H
