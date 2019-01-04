#include <stdint.h>
#include <stddef.h>
#include "cobs.h"

#define StartBlock() (code_ptr = dst++, code = 1)
#define FinishBlock() (*code_ptr = code)

/*
 * StuffData byte stuffs "length" bytes of data
 * at the location pointed to by "ptr", writing
 * the output to the location pointed to by "dst".
 *
 * Returns the length of the encoded data.
 */
size_t StuffData(const uint8_t *ptr, size_t length, uint8_t *dst)
{
  const uint8_t *start = dst, *end = ptr + length;
  uint8_t code, *code_ptr; /* Where to insert the leading count */

  StartBlock();
  while (ptr < end)
  {
    if (code != 0xFF)
    {
      uint8_t c = *ptr++;
      if (c != 0)
      {
        *dst++ = c;
        code++;
        continue;
      }
    }
    FinishBlock();
    StartBlock();
  }
  FinishBlock();
  return dst - start;
}

/*
 * UnStuffData decodes "length" bytes of data at
 * the location pointed to by "ptr", writing the
 * output to the location pointed to by "dst".
 *
 * Returns the length of the decoded data
 * (which is guaranteed to be <= length).
 */
size_t UnStuffData(const uint8_t *ptr, size_t length, uint8_t *dst)
{
  const uint8_t *start = dst, *end = ptr + length;
  uint8_t code = 0xFF, copy = 0;

  for (; ptr < end; copy--)
  {
    if (copy != 0)
    {
      *dst++ = *ptr++;
    }
    else
    {
      if (code != 0xFF)
        *dst++ = 0;
      copy = code = *ptr++;
      if (code == 0)
        break; /* Source length too long */
    }
  }
  return dst - start;
}