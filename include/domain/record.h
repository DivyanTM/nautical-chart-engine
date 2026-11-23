
#ifndef ISO8211_RECORD_H
#define ISO8211_RECORD_H

#include <stddef.h>
#include <stdint.h>

#include "directory_entry.h"
#include "leader.h"

typedef struct {
  iso8211_leader leader;

  iso8211_directory_entry *directory;
  size_t directory_count;

  uint8_t *field_area;
  size_t field_area_length;

} iso8211_record;

#endif
