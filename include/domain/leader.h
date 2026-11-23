#ifndef ISO8211_LEADER_H
#define ISO8211_LEADER_H

#include <stdint.h>

typedef struct {
  uint32_t record_length;
  uint8_t interchange_level;
  char leader_identifier;
  char inline_code_extension;
  uint8_t version_number;
  char application_indicator;
  uint16_t field_control_length;
  uint32_t base_address_field_area;
  char extended_char_set[4];

  struct {
    uint8_t size_of_field_length;
    uint8_t size_of_field_position;
    uint8_t reserved;
    uint8_t size_of_field_tag;
  } entry_map;

} iso8211_leader;

#endif
