
#include "iso8211/record_reader.h"

#include "domain/directory_entry.h"
#include "domain/leader.h"
#include "iso8211/directory_parser.h"
#include "iso8211/leader_parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEADER_SIZE 24

int iso8211_parse_record(const char *raw, size_t raw_size,
                         iso8211_record *record) {
  if (raw == NULL || record == NULL) {
    return -1;
  }

  *record = (iso8211_record){0};

  if (raw_size < LEADER_SIZE) {
    return -1;
  }

  /* Parse Leader */

  char leader_buffer[LEADER_SIZE + 1];

  memcpy(leader_buffer, raw, LEADER_SIZE);
  leader_buffer[LEADER_SIZE] = '\0';

  iso8211_leader leader = {0};

  if (iso8211_parse_leader(leader_buffer, &leader) != 0) {
    fprintf(stderr, "Failed to parse the leader\n");
    return -1;
  }

  if (leader.record_length != raw_size) {
    fprintf(stderr, "Record length mismatch\n");
    return -1;
  }

  if (leader.base_address_field_area < 25 ||
      leader.base_address_field_area > raw_size) {
    fprintf(stderr, "Invalid base address\n");
    return -1;
  }

  /* Parse Directory */

  size_t directory_size = leader.base_address_field_area - 25;

  char *directory_buffer = malloc(directory_size + 1);

  if (directory_buffer == NULL) {
    fprintf(stderr, "Directory allocation failed\n");
    return -1;
  }

  memcpy(directory_buffer, raw + LEADER_SIZE, directory_size);
  directory_buffer[directory_size] = '\0';

  iso8211_directory_entry *parsed_entries = NULL;
  size_t entry_count = 0;

  if (iso8211_parse_directory(directory_buffer, directory_size, &leader,
                              &parsed_entries, &entry_count) != 0) {
    fprintf(stderr, "Failed to parse directory entries\n");
    free(directory_buffer);
    return -1;
  }

  /* Extract Field Area */

  size_t field_area_offset = leader.base_address_field_area;
  size_t field_area_length = raw_size - field_area_offset;

  /* Validate Directory Entry Boundaries */

  for (size_t i = 0; i < entry_count; i++) {
    size_t position = parsed_entries[i].field_position;
    size_t length = parsed_entries[i].field_length;

    if (position > field_area_length || length > field_area_length - position) {
      fprintf(stderr, "Directory entry exceeds field area bounds: %s\n",
              parsed_entries[i].tag);

      iso8211_record temp_record = {0};
      temp_record.directory = parsed_entries;
      temp_record.directory_count = entry_count;

      iso8211_free_record(&temp_record);
      free(directory_buffer);

      return -1;
    }
  }

  /* Allocate and Copy Field Area */

  uint8_t *field_area = NULL;

  if (field_area_length > 0) {
    field_area = malloc(field_area_length);

    if (field_area == NULL) {
      fprintf(stderr, "Field area allocation failed\n");

      iso8211_record temp_record = {0};
      temp_record.directory = parsed_entries;
      temp_record.directory_count = entry_count;

      iso8211_free_record(&temp_record);
      free(directory_buffer);

      return -1;
    }

    memcpy(field_area, raw + field_area_offset, field_area_length);
  }

  /* Populate Record */

  iso8211_record temp_record = {0};

  temp_record.leader = leader;
  temp_record.directory = parsed_entries;
  temp_record.directory_count = entry_count;
  temp_record.field_area = field_area;
  temp_record.field_area_length = field_area_length;

  *record = temp_record;

  free(directory_buffer);

  return 0;
}

void iso8211_free_record(iso8211_record *record) {
  if (record == NULL) {
    return;
  }

  if (record->directory != NULL) {
    for (size_t i = 0; i < record->directory_count; i++) {
      free(record->directory[i].tag);
    }

    free(record->directory);
  }

  free(record->field_area);

  *record = (iso8211_record){0};
}
