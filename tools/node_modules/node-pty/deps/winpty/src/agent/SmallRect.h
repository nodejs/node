// Copyright (c) 2011-2012 Ryan Prichard
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

#ifndef SMALLRECT_H
#define SMALLRECT_H

#include <windows.h>

#include <algorithm>
#include <string>

#include "../shared/winpty_snprintf.h"
#include "Coord.h"

struct SmallRect : SMALL_RECT
{
    SmallRect()
    {
        Left = Right = Top = Bottom = 0;
    }

    SmallRect(SHORT x, SHORT y, SHORT width, SHORT height)
    {
        Left = x;
        Top = y;
        Right = x + width - 1;
        Bottom = y + height - 1;
    }

    SmallRect(const COORD &topLeft, const COORD &size)
    {
        Left = topLeft.X;
        Top = topLeft.Y;
        Right = Left + size.X - 1;
        Bottom = Top + size.Y - 1;
    }

    SmallRect(const SMALL_RECT &other)
    {
        *(SMALL_RECT*)this = other;
    }

    SmallRect(const SmallRect &other)
    {
        *(SMALL_RECT*)this = *(const SMALL_RECT*)&other;
    }

    SmallRect &operator=(const SmallRect &other)
    {
        *(SMALL_RECT*)this = *(const SMALL_RECT*)&other;
        return *this;
    }

    bool contains(const SmallRect &other) const
    {
        return other.Left >= Left &&
               other.Right <= Right &&
               other.Top >= Top &&
               other.Bottom <= Bottom;
    }

    bool contains(const Coord &other) const
    {
        return other.X >= Left &&
               other.X <= Right &&
               other.Y >= Top &&
               other.Y <= Bottom;
    }

    SmallRect intersected(const SmallRect &other) const
    {
        int x1 = std::max(Left, other.Left);
        int x2 = std::min(Right, other.Right);
        int y1 = std::max(Top, other.Top);
        int y2 = std::min(Bottom, other.Bottom);
        return SmallRect(x1,
                         y1,
                         std::max(0, x2 - x1 + 1),
                         std::max(0, y2 - y1 + 1));
    }

    SmallRect ensureLineIncluded(SHORT line) const
    {
        const SHORT h = height();
        if (line < Top) {
            return SmallRect(Left, line, width(), h);
        } else if (line > Bottom) {
            return SmallRect(Left, line - h + 1, width(), h);
        } else {
            return *this;
        }
    }

    SHORT top() const               { return Top;                       }
    SHORT left() const              { return Left;                      }
    SHORT width() const             { return Right - Left + 1;          }
    SHORT height() const            { return Bottom - Top + 1;          }
    void setTop(SHORT top)          { Top = top;                        }
    void setLeft(SHORT left)        { Left = left;                      }
    void setWidth(SHORT width)      { Right = Left + width - 1;         }
    void setHeight(SHORT height)    { Bottom = Top + height - 1;        }
    Coord size() const              { return Coord(width(), height());  }

    bool operator==(const SmallRect &other) const
    {
        return Left == other.Left &&
               Right == other.Right &&
               Top == other.Top &&
               Bottom == other.Bottom;
    }

    bool operator!=(const SmallRect &other) const
    {
        return !(*this == other);
    }

    std::string toString() const
    {
        char ret[64];
        winpty_snprintf(ret, "(x=%d,y=%d,w=%d,h=%d)",
                        Left, Top, width(), height());
        return std::string(ret);
    }
};

#endif // SMALLRECT_H
