#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"
#include <fcntl.h>
#include <unistd.h>

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

int
get_filename_prefix(char *input, char **prefix_str)
{

  const char *dot = strrchr(input, '.');
  unsigned int filename_prefix_len = 0;

  if (!dot || dot == input) {

    filename_prefix_len = strlen(input) + 1;

  } else {

    filename_prefix_len = (dot - input) + 1;
  }

  *prefix_str = (char *)malloc(filename_prefix_len + (ZR_WORDSIZE_IN_BYTES - filename_prefix_len % ZR_WORDSIZE_IN_BYTES));
  if (*prefix_str == NULL) {

    syslog(LOG_ERR, "Could not populate filename prefix string for %s\n", input);
    return -1;

  }

  memset(*prefix_str, 0, filename_prefix_len);
  snprintf(*prefix_str, filename_prefix_len, "%s", input);

  return 0;

}

int
check_if_file_exists(char *ifilename)
{

  int retval;

  syslog(LOG_DEBUG, "= About to check if %s exists ", ifilename);

  if( access( ifilename, F_OK ) != -1 )
    // file exists
    retval = 0;
  else
    // file does not exist
    retval = FILE_DOES_NOT_EXIST;

  return retval;

}
