
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "domain/leader.h"
#include "domain/record.h"
#include "iso8211/leader_parser.h"
#include "iso8211/record_reader.h"

#define LEADER_SIZE 24

static int read_exact(int fd, char *buffer, size_t size) {
  size_t total_read = 0;

  while (total_read < size) {
    ssize_t bytes_read = read(fd, buffer + total_read, size - total_read);

    if (bytes_read == -1) {
      if (errno == EINTR)
        continue;

      return -1;
    }

    if (bytes_read == 0)
      return -1;

    total_read += (size_t)bytes_read;
  }

  return 0;
}

/*
 * Read a complete ISO 8211 record at a given file offset.
 *
 * First read the Leader to determine record_length.
 * Then seek back and read the complete record.
 */
static int read_raw_record(int fd, off_t offset, char **raw, size_t *raw_size) {
  if (raw == NULL || raw_size == NULL)
    return -1;

  *raw = NULL;
  *raw_size = 0;

  if (lseek(fd, offset, SEEK_SET) == (off_t)-1)
    return -1;

  char leader_buffer[LEADER_SIZE];

  if (read_exact(fd, leader_buffer, LEADER_SIZE) != 0)
    return -1;

  iso8211_leader leader = {0};

  if (iso8211_parse_leader(leader_buffer, &leader) != 0)
    return -1;

  if (leader.record_length < LEADER_SIZE ||
      leader.base_address_field_area < 25 ||
      leader.base_address_field_area >= leader.record_length) {
    return -1;
  }

  size_t record_size = leader.record_length;

  char *buffer = malloc(record_size);

  if (buffer == NULL)
    return -1;

  /*
   * Reposition to the beginning of this record.
   */
  if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
    free(buffer);
    return -1;
  }

  /*
   * Read the entire record, including its Leader.
   */
  if (read_exact(fd, buffer, record_size) != 0) {
    free(buffer);
    return -1;
  }

  // printf("Last raw-record byte: 0x%02X\n",
  // (unsigned char)buffer[record_size - 1]);

  *raw = buffer;
  *raw_size = record_size;

  return 0;
}

static void print_leader(const iso8211_leader *leader) {
  printf("\n--- Parsed Leader ---\n");

  printf("Record Length             : %u\n", leader->record_length);
  printf("Interchange Level         : %c\n", leader->interchange_level);
  printf("Leader Identifier         : %c\n", leader->leader_identifier);
  printf("Inline Code Extension     : %c\n", leader->inline_code_extension);
  printf("Version Number            : %c\n", leader->version_number);
  printf("Application Indicator     : %c\n", leader->application_indicator);
  printf("Field Control Length      : %u\n", leader->field_control_length);
  printf("Base Address Field Area  : %u\n", leader->base_address_field_area);
  printf("Extended Character Set   : %s\n", leader->extended_char_set);

  printf("\n--- Entry Map ---\n");

  printf("Field Length Width        : %u\n",
         leader->entry_map.size_of_field_length);

  printf("Field Position Width      : %u\n",
         leader->entry_map.size_of_field_position);

  printf("Reserved                  : %u\n", leader->entry_map.reserved);

  printf("Field Tag Width           : %u\n",
         leader->entry_map.size_of_field_tag);
}

static void print_directory(const iso8211_record *record) {
  printf("\n--- DIRECTORY ENTRIES ---\n\n");
  printf("Entry count: %zu\n\n", record->directory_count);

  printf("%-10s | %-10s | %-10s\n", "TAG", "LENGTH", "POSITION");

  printf("------------|------------|------------\n");

  for (size_t i = 0; i < record->directory_count; i++) {
    printf("%-10s | %-10u | %-10u\n", record->directory[i].tag,
           record->directory[i].field_length,
           record->directory[i].field_position);
  }
}

static void print_field_area(const iso8211_record *record) {
  printf("\n--- FIELD AREA ---\n");
  printf("Field area length: %zu bytes\n\n", record->field_area_length);

  for (size_t i = 0; i < record->field_area_length; i++) {
    printf("%02X ", record->field_area[i]);

    if ((i + 1) % 16 == 0)
      printf("\n");
  }

  printf("\n");
}

static int process_record(int fd, off_t offset, const char *label) {
  char *raw_record = NULL;
  size_t record_size = 0;

  if (read_raw_record(fd, offset, &raw_record, &record_size) != 0) {
    fprintf(stderr, "Failed to read %s\n", label);
    return -1;
  }

  iso8211_record record = {0};

  if (iso8211_parse_record(raw_record, record_size, &record) != 0) {
    fprintf(stderr, "Failed to parse %s\n", label);
    free(raw_record);
    return -1;
  }

  printf("\n========== %s ==========\n", label);

  print_leader(&record.leader);
  print_directory(&record);
  print_field_area(&record);

  iso8211_free_record(&record);
  free(raw_record);

  return 0;
}

int main(void) {
  printf("ISO8211 PARSER\n");

  int file_fd = open("../data/enc/IN43001A.000", O_RDONLY);

  if (file_fd == -1) {
    fprintf(stderr, "Failed to open file: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  /*
   * Read DDR Leader to determine where the first DR begins.
   */
  char *ddr_raw = NULL;
  size_t ddr_size = 0;

  if (read_raw_record(file_fd, 0, &ddr_raw, &ddr_size) != 0) {
    fprintf(stderr, "Failed to read DDR\n");
    close(file_fd);
    return EXIT_FAILURE;
  }

  iso8211_record ddr_record = {0};

  if (iso8211_parse_record(ddr_raw, ddr_size, &ddr_record) != 0) {
    fprintf(stderr, "Failed to parse DDR\n");
    free(ddr_raw);
    close(file_fd);
    return EXIT_FAILURE;
  }

  // printf("\nDDR boundary bytes:\n");
  //
  // for (size_t i = ddr_size - 8; i < ddr_size; i++) {
  //   printf("%zu: 0x%02X\n", i, (unsigned char)ddr_raw[i]);
  // }

  off_t first_dr_offset = (off_t)ddr_record.leader.record_length;

  printf("\nDDR record length: %u\n", ddr_record.leader.record_length);

  // printf("First DR offset: %jd\n", (intmax_t)first_dr_offset);

  printf("\n========== DDR ==========\n");

  print_leader(&ddr_record.leader);
  print_directory(&ddr_record);
  print_field_area(&ddr_record);

  iso8211_free_record(&ddr_record);
  free(ddr_raw);

  /*
   * Parse the first Data Record.
   */
  if (process_record(file_fd, first_dr_offset, "FIRST DATA RECORD") != 0) {
    close(file_fd);
    return EXIT_FAILURE;
  }

  close(file_fd);

  return EXIT_SUCCESS;
  // }
