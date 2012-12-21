#ifdef _MSC_VER
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
int foo() {
  return 42;
}
