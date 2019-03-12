#import <Foundation/Foundation.h>

int main() {
  printf("gc on: %d\n", [NSGarbageCollector defaultCollector] != NULL);
  return 0;
}
