
#include "iso8211/directory_parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_uint(const char *str, uint32_t width, uint32_t *value) {
  char buffer[16];

  if (str == NULL || value == NULL || width == 0 || width >= sizeof(buffer)) {
    return -1;
  }

  for (uint32_t i = 0; i < width; i++) {
    if (str[i] < '0' || str[i] > '9') {
      return -1;
    }
  }

  memcpy(buffer, str, width);
  buffer[width] = '\0';

  unsigned long parsed = strtoul(buffer, NULL, 10);

  if (parsed > UINT32_MAX) {
    return -1;
  }

  *value = (uint32_t)parsed;
  return 0;
}

int iso8211_parse_directory(const char *raw, size_t raw_size,
                            const iso8211_leader *leader,
                            iso8211_directory_entry **entries,
                            size_t *entry_count) {

  if (raw == NULL || raw_size == 0 || leader == NULL || entries == NULL ||
      entry_count == NULL) {
    fprintf(stderr, "input validation failed\n");
    return -1;
  }

  *entries = NULL;
  *entry_count = 0;

  if (leader->base_address_field_area <= 25) {
    fprintf(stderr, "invalid base address\n");
    return -1;
  }

  size_t directory_size = leader->base_address_field_area - 25;

  if (raw_size < directory_size) {
    fprintf(stderr, "raw size validation failed\n");
    return -1;
  }

  uint32_t size_of_field_length = leader->entry_map.size_of_field_length;
  uint32_t size_of_field_position = leader->entry_map.size_of_field_position;
  uint32_t reserved = leader->entry_map.reserved;
  uint32_t size_of_field_tag = leader->entry_map.size_of_field_tag;

  if (size_of_field_length == 0 || size_of_field_position == 0 ||
      size_of_field_tag == 0) {
    fprintf(stderr, "invalid entry map widths\n");
    return -1;
  }

  size_t entry_width = (size_t)size_of_field_length + size_of_field_position +
                       size_of_field_tag + reserved;

  if (entry_width == 0 || directory_size % entry_width != 0) {
    fprintf(stderr, "invalid directory entry width\n");
    return -1;
  }

  size_t count = directory_size / entry_width;

  if (count > SIZE_MAX / sizeof(iso8211_directory_entry)) {
    fprintf(stderr, "directory allocation size overflow\n");
    return -1;
  }

  iso8211_directory_entry *parsed_entries =
      calloc(count, sizeof(*parsed_entries));

  if (parsed_entries == NULL) {
    fprintf(stderr, "parsed entries allocation failed\n");
    return -1;
  }

  for (size_t i = 0; i < count; i++) {
    const char *entry_str = raw + (i * entry_width);

    parsed_entries[i].tag = malloc(size_of_field_tag + 1);

    if (parsed_entries[i].tag == NULL) {
      for (size_t j = 0; j < i; j++) {
        free(parsed_entries[j].tag);
      }

      free(parsed_entries);

      fprintf(stderr, "tag allocation failed at entry %zu\n", i);
      return -1;
    }

    memcpy(parsed_entries[i].tag, entry_str, size_of_field_tag);
    parsed_entries[i].tag[size_of_field_tag] = '\0';

    if (parse_uint(entry_str + size_of_field_tag, size_of_field_length,
                   &parsed_entries[i].field_length) != 0) {
      goto parse_failure;
    }

    if (parse_uint(entry_str + size_of_field_tag + size_of_field_length,
                   size_of_field_position,
                   &parsed_entries[i].field_position) != 0) {
      goto parse_failure;
    }

    continue;

  parse_failure:
    for (size_t j = 0; j <= i; j++) {
      free(parsed_entries[j].tag);
    }

    free(parsed_entries);

    fprintf(stderr, "invalid numeric value at directory entry %zu\n", i);
    return -1;
  }

  *entries = parsed_entries;
  *entry_count = count;

  return 0;
}
