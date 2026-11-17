#ifndef UTIL_H__INCLUDED
#define UTIL_H__INCLUDED

#include <stdint.h>
#include <stddef.h>

void tostr (char *str, size_t len, uint32_t val);
size_t strlen (char *str);

void strcpy (char *d, char *s);
void strcat (char *s, char *t);
void itoa (int32_t n, char s[]);

#endif//UTIL_H__INCLUDED
