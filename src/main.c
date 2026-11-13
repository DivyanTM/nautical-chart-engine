
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LEADER_SIZE 24

int main(void) {
  char leader_buffer[LEADER_SIZE + 1];

  printf("Nautical Chart Engine v0.1.0\n");

  int file_fd = open("../data/enc/IN43001A.000", O_RDONLY);
  if (file_fd == -1) {
    fprintf(stderr, "Failed to open the file: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  printf("File exists\n");

  // Read ISO 8211 Leader
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

  printf("\n--- ISO 8211 Leader ---\n");

  printf("Leader: %s\n", leader_buffer);

  // Extract record length
  char record_length_str[6];

  memcpy(record_length_str, leader_buffer, 5);
  record_length_str[5] = '\0';

  unsigned long record_length = strtoul(record_length_str, NULL, 10);

  printf("Record length: %lu bytes\n", record_length);

  printf("Interchange level: %c\n", leader_buffer[5]);
  printf("Leader identifier: %c\n", leader_buffer[6]);
  printf("Inline code extension: %c\n", leader_buffer[7]);
  printf("Version: %c\n", leader_buffer[8]);
  printf("Application indicator: %c\n", leader_buffer[9]);

  printf("Field control length: %.2s\n", leader_buffer + 10);
  printf("Base address: %.5s\n", leader_buffer + 12);
  printf("Extended character set: %.3s\n", leader_buffer + 17);
  printf("Entry map: %.4s\n", leader_buffer + 20);

  // Extract Directory entry format
  int length_field_size = leader_buffer[20] - '0';
  int position_field_size = leader_buffer[21] - '0';
  int reserved_field_size = leader_buffer[22] - '0';
  int tag_field_size = leader_buffer[23] - '0';

  int base_address = atoi(leader_buffer + 12);

  int entry_size = length_field_size + position_field_size +
                   reserved_field_size + tag_field_size;

  int directory_size = base_address - LEADER_SIZE - 1;

  int entry_count = directory_size / entry_size;

  printf("\n--- ISO 8211 Directory ---\n");

  printf("Length field size   : %d\n", length_field_size);
  printf("Position field size : %d\n", position_field_size);
  printf("Reserved field size : %d\n", reserved_field_size);
  printf("Tag field size      : %d\n", tag_field_size);

  printf("Directory size      : %d bytes\n", directory_size);
  printf("Entry size          : %d bytes\n", entry_size);
  printf("Entry count         : %d\n", entry_count);

  // Read Directory
  char *directory_buffer = malloc(directory_size + 1);

  if (directory_buffer == NULL) {
    perror("malloc");
    close(file_fd);
    return EXIT_FAILURE;
  }

  ssize_t directory_read = 0;

  while (directory_read < directory_size) {
    ssize_t bytes_read = read(file_fd, directory_buffer + directory_read,
                              directory_size - (size_t)directory_read);

    if (bytes_read == -1) {
      if (errno == EINTR)
        continue;

      perror("Directory read failed");
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

    directory_read += bytes_read;
  }

  // Parse Directory entries
  printf("\n--- Parsed Directory Entries ---\n");

  printf("Entry\tTag\tLength\tPosition\n");

  for (int i = 0; i < entry_count; i++) {

    char tag[5];
    char length_str[7];
    char position_str[7];

    // Point to the beginning of the current entry
    char *entry = directory_buffer + (i * entry_size);

    // Extract Tag
    memcpy(tag, entry, tag_field_size);
    tag[tag_field_size] = '\0';

    // Extract Field Length
    memcpy(length_str, entry + tag_field_size, length_field_size);
    length_str[length_field_size] = '\0';

    // Extract Field Position
    memcpy(position_str,
           entry + tag_field_size + length_field_size + reserved_field_size,
           position_field_size);

    position_str[position_field_size] = '\0';

    // Convert numeric strings into integers
    int field_length = atoi(length_str);
    int field_position = atoi(position_str);

    printf("%02d\t%s\t%d\t%d\n", i + 1, tag, field_length, field_position);
  }

  int dsid_offset = base_address + 166;

  int dsid_length = 165;

  char *dsid_buffer = malloc(dsid_length);

  if (dsid_buffer == NULL) {
    perror("malloc for dsid buffer");
    close(file_fd);
    free(directory_buffer);
    return EXIT_FAILURE;
  }

  lseek(file_fd, dsid_offset, SEEK_SET);
  read(file_fd, dsid_buffer, dsid_length);

  printf("dsid field\n");
  for (int i = 0; i < dsid_length; i++) {
    printf("%02X ", (unsigned char)dsid_buffer[i]);
  }
  printf("\n");

  free(directory_buffer);
  free(dsid_buffer);
  close(file_fd);

  return EXIT_SUCCESS;
}
