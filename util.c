////////////////////////////////////////////////
// utility functions
//
// 2020-04-02 pds    initial cut
// 2020-05-10 pds    add strcpy,strcat,itoa
//

#include <stdint.h>  // for uint32_t
#include <stddef.h>  // for size_t


void tostr (char *str, size_t len, uint32_t val)
{
  int i;
  for (i=1; i<=len; i++)
  {
    str[len - i] = (char) ((val % 10UL) + '0');
    val /= 10;
  }

  str[i - 1] = '\0';
}

size_t strlen (char *str)
{
  size_t len=0;
  while (str[len] != '\0')
  {
    len++;
  }
  return len;
}


void strcpy (char *d, char *s)
{
  while ((*d++ = *s++));
}

void strcat (char *s, char *t)
{
  while (*s) s++;
  while ((*s++ = *t++));
}

void reverse (char s[])
{
  int32_t i, j;
  char c;

  for (i=0, j=strlen(s)-1; i<j; i++, j--)
  {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

void itoa (int32_t n, char s[])
{
  int32_t i, sign;

  if ((sign = n) < 0) n = -n;

  i = 0;
  do
  {
    s[i++] = n  % 10 + '0';
  }
  while ((n /= 10) > 0);

  if (sign < 0) s[i++] = '-';

  s[i] = '\0';
  reverse( s );
}

