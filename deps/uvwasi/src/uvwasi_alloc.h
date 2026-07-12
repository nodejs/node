#ifndef __UVWASI_ALLOC_H__
#define __UVWASI_ALLOC_H__

#include "uvwasi.h"

void* uvwasi__malloc(const uvwasi_t* uvwasi, size_t size);
void uvwasi__free(const uvwasi_t* uvwasi, void* ptr);
void* uvwasi__calloc(const uvwasi_t* uvwasi, size_t nmemb, size_t size);
void* uvwasi__realloc(const uvwasi_t* uvwasi, void* ptr, size_t size);

#endif
