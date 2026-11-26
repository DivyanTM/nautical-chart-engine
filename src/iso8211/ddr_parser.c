
#include "iso8211/ddr_parser.h"
#include "domain/directory_entry.h"
#include "domain/record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *extract_string(const uint8_t *field, size_t start, size_t end) {
  if (end < start)
    return NULL;

  size_t length = end - start;

  char *result = malloc(length + 1);
  if (result == NULL)
    return NULL;

  memcpy(result, field + start, length);
  result[length] = '\0';

  return result;
}

int iso8211_parse_ddr(const iso8211_record *record,
                      iso8211_field_definition_table *table) {

  if (record == NULL || table == NULL)
    return -1;

  size_t field_control_length = record->leader.field_control_length;

  if (field_control_length == 0)
    return -1;

  for (size_t i = 0; i < record->directory_count; i++) {

    const iso8211_directory_entry *entry = &record->directory[i];

    if (strcmp(entry->tag, "0000") == 0 || strcmp(entry->tag, "0001") == 0) {
      continue;
    }

    if (entry->field_position > record->field_area_length ||
        entry->field_length >
            record->field_area_length - entry->field_position) {
      return -1;
    }

    const uint8_t *field = record->field_area + entry->field_position;

    size_t field_length = entry->field_length;

    if (field_control_length >= field_length)
      return -1;

    size_t name_start = field_control_length;
    size_t name_end = name_start;

    while (name_end < field_length && field[name_end] != 0x1F) {
      name_end++;
    }

    if (name_end == field_length)
      return -1;

    size_t array_start = name_end + 1;
    size_t array_end = array_start;

    while (array_end < field_length && field[array_end] != 0x1F) {
      array_end++;
    }

    if (array_end == field_length)
      return -1;

    size_t format_start = array_end + 1;
    size_t format_end = format_start;

    while (format_end < field_length && field[format_end] != 0x1E) {
      format_end++;
    }

    /*
     * The field terminator must be the final byte
     * included in the directory entry's field length.
     */
    if (format_end != field_length - 1)
      return -1;

    iso8211_field_definition definition = {0};

    strncpy(definition.tag, entry->tag, sizeof(definition.tag) - 1);

    definition.field_name = extract_string(field, name_start, name_end);

    definition.array_descriptor = extract_string(field, array_start, array_end);

    definition.format_controls =
        extract_string(field, format_start, format_end);

    if (definition.field_name == NULL || definition.array_descriptor == NULL ||
        definition.format_controls == NULL) {

      free(definition.field_name);
      free(definition.array_descriptor);
      free(definition.format_controls);

      return -1;
    }

    iso8211_field_definition *new_definitions =
        realloc(table->definitions,
                (table->count + 1) * sizeof(iso8211_field_definition));

    if (new_definitions == NULL) {

      free(definition.field_name);
      free(definition.array_descriptor);
      free(definition.format_controls);

      return -1;
    }

    table->definitions = new_definitions;

    table->definitions[table->count] = definition;
    table->count++;
  }

  return 0;
}

void iso8211_free_ddr(iso8211_field_definition_table *table) {

  if (table == NULL)
    return;

  for (size_t i = 0; i < table->count; i++) {

    free(table->definitions[i].field_name);
    free(table->definitions[i].array_descriptor);
    free(table->definitions[i].format_controls);
  }

  free(table->definitions);

  table->definitions = NULL;
  table->count = 0;
}
