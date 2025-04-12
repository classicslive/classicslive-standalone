#ifndef CLS_IDENTIFICATION_METHODS_H
#define CLS_IDENTIFICATION_METHODS_H

#include <stdint.h>

typedef struct
{
  /**
   * The title ID of the game, a 64-bit integer.
   * https://switchbrew.org/wiki/Title_list/Games
   */
  uint64_t title_id;

  /**
   * The version of the game as a string; ie, "1.3.0". The "v" is not actually
   * part of the string. Make sure to fill any unused data with 00s.
   * @todo The length of the string is taken from system version definition and
   * might not be accurate for game update strings.
   */
  char version[0x18];

  /**
   * A 32-bit (?) identifier for the software version.
   * Appears to just be the revision number shifted left by two bytes, not
   * representing the "version number" string.
   * uint64_t version = 0x40000;
   * @todo Currently we are using the string instead. Would there be a reason
   * to need this?
   */
} cl_identify_nx_t;

#endif
