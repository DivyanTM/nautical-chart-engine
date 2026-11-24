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

int main(void) {
  printf("ISO8211 PARSER\n");

  char leader_buffer[LEADER_SIZE + 1];

  int file_fd = open("../data/enc/IN43001A.000", O_RDONLY);
  if (file_fd == -1) {
    fprintf(stderr, "Failed to open the file: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  printf("File exists\n");

  size_t total_read = 0;

  while (total_read < LEADER_SIZE) {
    ssize_t bytes_read =
        read(file_fd, leader_buffer + total_read, LEADER_SIZE - total_read);

    if (bytes_read == -1) {
      if (errno == EINTR)
        continue;

      perror("Leader read failed");
      close(file_fd);
      return EXIT_FAILURE;
    }

    if (bytes_read == 0) {
      fprintf(stderr, "Unexpected end of file\n");
      close(file_fd);
      return EXIT_FAILURE;
    }

    total_read += (size_t)bytes_read;
  }

  leader_buffer[LEADER_SIZE] = '\0';

  iso8211_leader leader = {0};

  int result = iso8211_parse_leader(leader_buffer, &leader);

  printf("\nParse result: %d\n", result);

  if (result != 0) {
    fprintf(stderr, "Failed to parse Leader\n");
    close(file_fd);
    return EXIT_FAILURE;
  }

  // Display the parsed Leader values.
  printf("\n--- Parsed Leader ---\n");

  printf("Record Length              : %u\n", leader.record_length);
  printf("Interchange Level         : %u\n", leader.interchange_level);
  printf("Leader Identifier         : %c\n", leader.leader_identifier);
  printf("Inline Code Extension    : %c\n", leader.inline_code_extension);
  printf("Version Number           : %u\n", leader.version_number);
  printf("Application Indicator     : %c\n", leader.application_indicator);
  printf("Field Control Length     : %u\n", leader.field_control_length);
  printf("Base Address Field Area : %u\n", leader.base_address_field_area);

  printf("Extended Character Set  : %s\n", leader.extended_char_set);

  printf("\n--- Entry Map ---\n");

  printf("Field Length Width  : %u\n", leader.entry_map.size_of_field_length);

  printf("Field Position Width : %u\n",
         leader.entry_map.size_of_field_position);

  printf("Reserved            : %u\n", leader.entry_map.reserved);

  printf("Field Tag Width     : %u\n", leader.entry_map.size_of_field_tag);

  size_t directory_size = leader.base_address_field_area - 24 - 1;

  char *directory_buffer = malloc(directory_size + 1);

  size_t directory_content_read = 0;

  while (directory_content_read < directory_size) {
    ssize_t bytes_read =
        read(file_fd, directory_buffer + directory_content_read,
             directory_size - (size_t)directory_content_read);
    if (bytes_read == -1) {
      if (errno == EINTR)
        continue;

      perror("Leader read failed");
      free(directory_buffer);
      close(file_fd);
      return EXIT_FAILURE;
    }

    if (bytes_read == 0) {
      fprintf(stderr, "Unexpected end of file\n");
      free(directory_buffer);
      close(file_fd);
      return EXIT_FAILURE;
    }
    directory_content_read += bytes_read;
  }

  directory_buffer[directory_size] = '\0';

  // printf("directory buffer : %s\n", directory_buffer);

  iso8211_directory_entry *parsed_entries = NULL;
  size_t entry_count = 0;

  int entry_parse_result = iso8211_parse_directory(
      directory_buffer, directory_size, &leader, &parsed_entries, &entry_count);

  if (entry_parse_result == 0 && entry_count > 0) {
    printf("\n");
    printf("--- ENTRIES ---\n\n");
    printf("Entry count: %zu\n\n", entry_count);

    printf("%-10s | %-10s | %-10s\n", "TAG", "LENGTH", "POSITION");

    printf("------------|------------|------------\n");

    for (size_t i = 0; i < entry_count; i++) {
      printf("%-10s | %-10u | %-10u\n", parsed_entries[i].tag,
             parsed_entries[i].field_length, parsed_entries[i].field_position);
    }
  }

  free(directory_buffer);
  close(file_fd);

  return EXIT_SUCCESS;
}
