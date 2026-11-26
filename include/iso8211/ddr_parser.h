#ifndef ISO8211_DDR_PARSER_H
#define ISO8211_DDR_PARSER_H

#include "domain/field_definition.h"
#include "domain/field_definition_table.h"
#include "domain/record.h"

int iso8211_parse_ddr(const iso8211_record *record,
                      iso8211_field_definition_table *table);
void iso8211_free_ddr(iso8211_field_definition_table *table);

#endif
