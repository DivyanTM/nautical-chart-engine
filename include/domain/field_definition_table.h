#ifndef ISO8211_FIELD_DEFINITION_TABLE_H
#define ISO8211_FIELD_DEFINITION_TABLE_H

#include <stddef.h>

#include "field_definition.h"

typedef struct {
  iso8211_field_definition *definitions;
  size_t count;
} iso8211_field_definition_table;

#endif
