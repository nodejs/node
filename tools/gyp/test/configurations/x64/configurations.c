#include <stdio.h>

int main(void) {
  if (sizeof(void*) == 4) {
    printf("Running Win32\n");
  } else if (sizeof(void*) == 8) {
    printf("Running x64\n");
  } else {
    printf("Unexpected platform\n");
  }
  return 0;
}
