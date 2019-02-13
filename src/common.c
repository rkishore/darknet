#include <string.h>
#include <stdbool.h>

bool starts_with(const char *pre, const char *str)
{
  size_t lenpre = strlen(pre), lenstr = strlen(str);
  return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

bool ends_with(const char *suf, const char *str)
{
  size_t lensuf = strlen(suf), lenstr = strlen(str);
  return lenstr < lensuf ? false : strncmp(str + lenstr - lensuf, suf, lensuf) == 0;
}
