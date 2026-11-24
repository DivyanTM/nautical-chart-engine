#ifndef ISO8211_DIRECOTORY_ENTRY_H
#define ISO8211_DIRECOTORY_ENTRY_H

#include <stdint.h>

typedef struct {
  char *tag;
  uint32_t field_length;
  uint32_t field_position;
} iso8211_directory_entry;

#endif // ISO8211_DIRECOTORY_ENTRY_H
