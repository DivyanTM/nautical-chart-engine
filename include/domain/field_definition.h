#ifndef ISO8211_FIELD_DEFINITION_H
#define ISO8211_FIELD_DEFINITION_H

#include <stddef.h>

typedef struct {
  char tag[5];

  char *field_name;
  char *array_descriptor;
  char *format_controls;

} iso8211_field_definition;

#endif
