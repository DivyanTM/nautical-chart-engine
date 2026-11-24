#ifndef ISO8211_DIRECTORY_PARSER_H
#define ISO8211_DIRECTORY_PARSER_H

#include <stddef.h>

#include "domain/directory_entry.h"
#include "domain/leader.h"

int iso8211_parse_directory(const char *raw, size_t raw_size,
                            const iso8211_leader *leader,
                            iso8211_directory_entry **entries,
                            size_t *entry_count);

#endif // ISO8211_DIRECTORY_PARSER_H
