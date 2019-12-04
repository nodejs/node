# V8 debug helper

This library is for debugging V8 itself, not debugging JavaScript running within
V8. It is designed to be called from a debugger extension running within a
native debugger such as WinDbg or LLDB. It can be used on live processes or
crash dumps, and cannot assume that all memory is available in a dump.
