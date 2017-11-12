#include "node_internals.h"
#include <windows.h>
#include <dbghelp.h>

namespace node {

void DumpBacktrace(FILE* fp) {
  void* frames[256];
  int size = CaptureStackBackTrace(0, arraysize(frames), frames, nullptr);
  HANDLE process = GetCurrentProcess();
  (void)SymInitialize(process, nullptr, true);

  // Ref: https://msdn.microsoft.com/en-en/library/windows/desktop/ms680578(v=vs.85).aspx
  char info_buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
  SYMBOL_INFO* info = reinterpret_cast<SYMBOL_INFO*>(info_buf);
  char demangled[MAX_SYM_NAME];

  for (int i = 1; i < size; i += 1) {
    void* frame = frames[i];
    fprintf(fp, "%2d: ", i);
    info->MaxNameLen = MAX_SYM_NAME;
    info->SizeOfStruct = sizeof(SYMBOL_INFO);
    const bool have_info =
        SymFromAddr(process, reinterpret_cast<DWORD64>(frame), nullptr, info);
    if (!have_info || strlen(info->Name) == 0) {
      fprintf(fp, "%p", frame);
    } else if (UnDecorateSymbolName(info->Name,
                                    demangled,
                                    sizeof(demangled),
                                    UNDNAME_COMPLETE)) {
      fprintf(fp, "%s", demangled);
    } else {
      fprintf(fp, "%s", info->Name);
    }
    fprintf(fp, "\n");
  }
  (void)SymCleanup(process);
}

}  // namespace node
