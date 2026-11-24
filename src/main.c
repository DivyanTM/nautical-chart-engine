
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "domain/directory_entry.h"
#include "domain/leader.h"
#include "iso8211/directory_parser.h"
#include "iso8211/leader_parser.h"

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

static void print_leader(const iso8211_leader *leader) {
  printf("\n--- Parsed Leader ---\n");

  printf("Record Length              : %u\n", leader->record_length);
  printf("Interchange Level         : %c\n", leader->interchange_level);
  printf("Leader Identifier         : %c\n", leader->leader_identifier);
  printf("Inline Code Extension    : %c\n", leader->inline_code_extension);
  printf("Version Number           : %c\n", leader->version_number);
  printf("Application Indicator     : %c\n", leader->application_indicator);
  printf("Field Control Length     : %u\n", leader->field_control_length);
  printf("Base Address Field Area : %u\n", leader->base_address_field_area);
  printf("Extended Character Set  : %s\n", leader->extended_char_set);

  printf("\n--- Entry Map ---\n");

  printf("Field Length Width  : %u\n", leader->entry_map.size_of_field_length);

  printf("Field Position Width : %u\n",
         leader->entry_map.size_of_field_position);

  printf("Reserved            : %u\n", leader->entry_map.reserved);

  printf("Field Tag Width     : %u\n", leader->entry_map.size_of_field_tag);
}

static void print_directory(const iso8211_directory_entry *entries,
                            size_t entry_count) {
  printf("\n--- DIRECTORY ENTRIES ---\n\n");
  printf("Entry count: %zu\n\n", entry_count);

  printf("%-10s | %-10s | %-10s\n", "TAG", "LENGTH", "POSITION");
  printf("------------|------------|------------\n");

  for (size_t i = 0; i < entry_count; i++) {
    printf("%-10s | %-10u | %-10u\n", entries[i].tag, entries[i].field_length,
           entries[i].field_position);
  }
}

static void free_directory(iso8211_directory_entry *entries,
                           size_t entry_count) {
  if (entries == NULL)
    return;

  for (size_t i = 0; i < entry_count; i++)
    free(entries[i].tag);

  free(entries);
}

int main(void) {
  printf("ISO8211 PARSER\n");

  int file_fd = open("../data/enc/IN43001A.000", O_RDONLY);

  if (file_fd == -1) {
    fprintf(stderr, "Failed to open file: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  printf("File exists\n");

  // --------------------------------------------------
  // First DDR
  // --------------------------------------------------

  char leader_buffer[LEADER_SIZE + 1];

  if (read_exact(file_fd, leader_buffer, LEADER_SIZE) != 0) {
    perror("DDR Leader read failed");
    close(file_fd);
    return EXIT_FAILURE;
  }

  leader_buffer[LEADER_SIZE] = '\0';

  iso8211_leader leader = {0};

  if (iso8211_parse_leader(leader_buffer, &leader) != 0) {
    fprintf(stderr, "Failed to parse DDR Leader\n");
    close(file_fd);
    return EXIT_FAILURE;
  }

  print_leader(&leader);

  if (leader.base_address_field_area <= 25) {
    fprintf(stderr, "Invalid DDR base address\n");
    close(file_fd);
    return EXIT_FAILURE;
  }

  size_t directory_size = leader.base_address_field_area - 25;

  char *directory_buffer = malloc(directory_size);

  if (directory_buffer == NULL) {
    perror("DDR Directory allocation failed");
    close(file_fd);
    return EXIT_FAILURE;
  }

  if (read_exact(file_fd, directory_buffer, directory_size) != 0) {
    perror("DDR Directory read failed");
    free(directory_buffer);
    close(file_fd);
    return EXIT_FAILURE;
  }

  iso8211_directory_entry *parsed_entries = NULL;
  size_t entry_count = 0;

  if (iso8211_parse_directory(directory_buffer, directory_size, &leader,
                              &parsed_entries, &entry_count) != 0) {
    fprintf(stderr, "Failed to parse DDR Directory\n");
    free(directory_buffer);
    close(file_fd);
    return EXIT_FAILURE;
  }

  print_directory(parsed_entries, entry_count);

  free_directory(parsed_entries, entry_count);
  free(directory_buffer);

  // --------------------------------------------------
  // First DR
  // --------------------------------------------------

  off_t first_dr_offset = (off_t)leader.record_length;

  printf("\nFirst DR offset: %ld\n", (long)first_dr_offset);

  off_t seek_result = lseek(file_fd, first_dr_offset, SEEK_SET);

  if (seek_result == (off_t)-1) {
    perror("Failed to seek to first DR");
    close(file_fd);
    return EXIT_FAILURE;
  }

  printf("Actual file position after seek: %ld\n", (long)seek_result);

  char first_dr_leader_buffer[LEADER_SIZE + 1];

  if (read_exact(file_fd, first_dr_leader_buffer, LEADER_SIZE) != 0) {
    perror("First DR Leader read failed");
    close(file_fd);
    return EXIT_FAILURE;
  }

  first_dr_leader_buffer[LEADER_SIZE] = '\0';

  printf("\n--- Raw First DR Leader ---\n");
  printf("%.*s\n", LEADER_SIZE, first_dr_leader_buffer);

  iso8211_leader first_dr_leader = {0};

  int dr_leader_result =
      iso8211_parse_leader(first_dr_leader_buffer, &first_dr_leader);

  printf("DR Leader parse result: %d\n", dr_leader_result);

  if (dr_leader_result != 0) {
    fprintf(stderr, "Failed to parse first DR Leader\n");
    close(file_fd);
    return EXIT_FAILURE;
  }

  print_leader(&first_dr_leader);

  if (first_dr_leader.leader_identifier != 'D') {
    fprintf(stderr, "Expected a Data Record Leader (D)\n");
    close(file_fd);
    return EXIT_FAILURE;
  }

  if (first_dr_leader.base_address_field_area <= 25) {
    fprintf(stderr, "Invalid DR base address\n");
    close(file_fd);
    return EXIT_FAILURE;
  }

  size_t dr_directory_size = first_dr_leader.base_address_field_area - 25;

  char *dr_directory_buffer = malloc(dr_directory_size);

  if (dr_directory_buffer == NULL) {
    perror("DR Directory allocation failed");
    close(file_fd);
    return EXIT_FAILURE;
  }

  if (read_exact(file_fd, dr_directory_buffer, dr_directory_size) != 0) {
    perror("DR Directory read failed");
    free(dr_directory_buffer);
    close(file_fd);
    return EXIT_FAILURE;
  }

  iso8211_directory_entry *dr_entries = NULL;
  size_t dr_entry_count = 0;

  if (iso8211_parse_directory(dr_directory_buffer, dr_directory_size,
                              &first_dr_leader, &dr_entries,
                              &dr_entry_count) != 0) {
    fprintf(stderr, "Failed to parse first DR Directory\n");
    free(dr_directory_buffer);
    close(file_fd);
    return EXIT_FAILURE;
  }

  print_directory(dr_entries, dr_entry_count);

  // --------------------------------------------------
  // Cleanup
  // --------------------------------------------------

  free_directory(dr_entries, dr_entry_count);
  free(dr_directory_buffer);

  close(file_fd);

  return EXIT_SUCCESS;
}
