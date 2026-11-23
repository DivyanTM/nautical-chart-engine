#include "iso8211/leader_parser.h"

#include <stdlib.h>
#include <string.h>

#define LEADER_SIZE 24

static int parse_decimal(const char *raw, size_t length,
                         unsigned long *result) {
  char buffer[6];

  if (raw == NULL || result == NULL || length == 0 ||
      length >= sizeof(buffer)) {
    return -1;
  }

  memcpy(buffer, raw, length);
  buffer[length] = '\0';

  for (size_t i = 0; i < length; i++) {
    if (buffer[i] < '0' || buffer[i] > '9') {
      return -1;
    }
  }

  *result = strtoul(buffer, NULL, 10);

  return 0;
}

int iso8211_parse_leader(const char *raw,
                         iso8211_leader *leader) {
  if (raw == NULL || leader == NULL) {
    return -1;
  }

  iso8211_leader temp = {0};
  unsigned long value;

  // RECORD LENGTH
  if (parse_decimal(raw, 5, &value) != 0) {
    return -1;
  }
  temp.record_length = value;

  // INTERCHANGE LEVEL
  if (parse_decimal(raw + 5, 1, &value) != 0) {
    return -1;
  }
  temp.interchange_level = value;

  // LEADER IDENTIFIER
  if (raw[6] != 'L' && raw[6] != 'D') {
    return -1;
  }
  temp.leader_identifier = raw[6];

  // INLINE CODE EXTENSION
  temp.inline_code_extension = raw[7];

  // VERSION NUMBER
  if (parse_decimal(raw + 8, 1, &value) != 0) {
    return -1;
  }
  temp.version_number = value;

  // APPLICATION INDICATOR
  temp.application_indicator = raw[9];

  // FIELD CONTROL LENGTH
  if (parse_decimal(raw + 10, 2, &value) != 0) {
    return -1;
  }
  temp.field_control_length = value;

  // BASE ADDRESS OF FIELD AREA
  if (parse_decimal(raw + 12, 5, &value) != 0) {
    return -1;
  }
  temp.base_address_field_area = value;

  // EXTENDED CHARACTER SET INDICATOR
  memcpy(temp.extended_char_set, raw + 17, 3);
  temp.extended_char_set[3] = '\0';

  // ENTRY MAP
  // SIZE OF FIELD LENGTH
  if (parse_decimal(raw + 20, 1, &value) != 0) {
    return -1;
  }
  temp.entry_map.size_of_field_length = value;

  // SIZE OF FIELD POSITION
  if (parse_decimal(raw + 21, 1, &value) != 0) {
    return -1;
  }
  temp.entry_map.size_of_field_position = value;

  // RESERVED
  if (parse_decimal(raw + 22, 1, &value) != 0) {
    return -1;
  }
  temp.entry_map.reserved = value;

  // SIZE OF FIELD TAG
  if (parse_decimal(raw + 23, 1, &value) != 0) {
    return -1;
  }
  temp.entry_map.size_of_field_tag = value;

  *leader = temp;

  return 0;
}
