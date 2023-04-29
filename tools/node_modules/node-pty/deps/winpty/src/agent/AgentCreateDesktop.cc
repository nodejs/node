// Copyright (c) 2016 Ryan Prichard
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

#include "AgentCreateDesktop.h"

#include "../shared/BackgroundDesktop.h"
#include "../shared/Buffer.h"
#include "../shared/DebugClient.h"
#include "../shared/StringUtil.h"

#include "EventLoop.h"
#include "NamedPipe.h"

namespace {

static inline WriteBuffer newPacket() {
    WriteBuffer packet;
    packet.putRawValue<uint64_t>(0); // Reserve space for size.
    return packet;
}

class CreateDesktopLoop : public EventLoop {
public:
    CreateDesktopLoop(LPCWSTR controlPipeName);

protected:
    virtual void onPipeIo(NamedPipe &namedPipe) override;

private:
    void writePacket(WriteBuffer &packet);

    BackgroundDesktop m_desktop;
    NamedPipe &m_pipe;
};

CreateDesktopLoop::CreateDesktopLoop(LPCWSTR controlPipeName) :
        m_pipe(createNamedPipe()) {
    m_pipe.connectToServer(controlPipeName, NamedPipe::OpenMode::Duplex);
    auto packet = newPacket();
    packet.putWString(m_desktop.desktopName());
    writePacket(packet);
}

void CreateDesktopLoop::writePacket(WriteBuffer &packet) {
    const auto &bytes = packet.buf();
    packet.replaceRawValue<uint64_t>(0, bytes.size());
    m_pipe.write(bytes.data(), bytes.size());
}

void CreateDesktopLoop::onPipeIo(NamedPipe &namedPipe) {
    if (m_pipe.isClosed()) {
        shutdown();
    }
}

} // anonymous namespace

void handleCreateDesktop(LPCWSTR controlPipeName) {
    try {
        CreateDesktopLoop loop(controlPipeName);
        loop.run();
        trace("Agent exiting...");
    } catch (const WinptyException &e) {
        trace("handleCreateDesktop: internal error: %s",
            utf8FromWide(e.what()).c_str());
    }
}
