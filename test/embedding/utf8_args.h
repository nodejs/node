#ifndef TEST_EMBEDDING_UTF8_ARGS_H_
#define TEST_EMBEDDING_UTF8_ARGS_H_

#ifdef __cplusplus
extern "C" {
#endif

// Windows does not support UTF-8 command line arguments.
// We must get them using an alternative way.
// This function does nothing on non-Windows platforms.
void GetUtf8CommandLineArgs(int* argc, char*** argv);

// Free the memory allocated by GetUtf8CommandLineArgs.
void FreeUtf8CommandLineArgs(int argc, char** argv);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // TEST_EMBEDDING_UTF8_ARGS_H_
