#ifndef ISO8211_RECORD_READER_H
#define ISO8211_RECORD_READER_H

#include "domain/record.h"

#include <stddef.h>

int iso8211_parse_record(const char *raw, size_t raw_size,
                         iso8211_record *record);

void iso8211_free_record(iso8211_record *record);

#endif ISO8211_RECORD_READER_H
