
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LEADER_SIZE 24

int main(void) {
  // ISO 8211 Leader is always 24 bytes.
  // Extra byte is reserved for '\0' so we can print it as a C string.
  char leader_buffer[LEADER_SIZE + 1];

  printf("Nautical Chart Engine v0.1.0\n");

  // Open the ENC file in read-only mode.
  int file_fd = open("../data/enc/IN43001A.000", O_RDONLY);
  if (file_fd == -1) {
    fprintf(stderr, "Failed to open the file: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  printf("File exists\n");

  // ---------------------------------------------------------
  // 1. READ ISO 8211 LEADER
  // ---------------------------------------------------------

  // read() is not guaranteed to return all requested bytes in one call.
  // total_read tracks how many Leader bytes have been received so far.
  size_t total_read = 0;

  while (total_read < LEADER_SIZE) {
    // Read only the remaining bytes.
    // leader_buffer + total_read points to the next available buffer position.
    ssize_t bytes_read =
        read(file_fd, leader_buffer + total_read, LEADER_SIZE - total_read);

    if (bytes_read == -1) {
      // EINTR means the read was interrupted by a signal.
      // Retry instead of treating it as a permanent failure.
      if (errno == EINTR)
        continue;

      perror("Leader read failed");
      close(file_fd);
      return EXIT_FAILURE;
    }

    // read() returns 0 when EOF is reached.
    // We still need Leader bytes, so this is an unexpected EOF.
    if (bytes_read == 0) {
      fprintf(stderr, "Unexpected end of file\n");
      close(file_fd);
      return EXIT_FAILURE;
    }

    // Increase the number of bytes successfully received.
    total_read += (size_t)bytes_read;
  }

  // The Leader contains exactly 24 bytes, not a C string.
  // Add '\0' so printf("%s") knows where the string ends.
  leader_buffer[LEADER_SIZE] = '\0';

  printf("\n--- ISO 8211 Leader ---\n");
  printf("Leader: %s\n", leader_buffer);

  // ---------------------------------------------------------
  // 2. EXTRACT LEADER INFORMATION
  // ---------------------------------------------------------

  // Record length occupies Leader bytes 0-4 (5 characters).
  // Copy those characters into a separate string.
  char record_length_str[6];

  memcpy(record_length_str, leader_buffer, 5);
  record_length_str[5] = '\0';

  // Convert the numeric string into an integer.
  // For this file, "01641" becomes 1641.
  unsigned long record_length = strtoul(record_length_str, NULL, 10);

  printf("Record length: %lu bytes\n", record_length);

  // These Leader values are single-character fields.
  printf("Interchange level: %c\n", leader_buffer[5]);
  printf("Leader identifier: %c\n", leader_buffer[6]);
  printf("Inline code extension: %c\n", leader_buffer[7]);
  printf("Version: %c\n", leader_buffer[8]);
  printf("Application indicator: %c\n", leader_buffer[9]);

  // Print other fixed-width Leader fields.
  // %.2s, %.5s and %.4s limit how many characters are printed.
  printf("Field control length: %.2s\n", leader_buffer + 10);
  printf("Base address: %.5s\n", leader_buffer + 12);
  printf("Extended character set: %.3s\n", leader_buffer + 17);
  printf("Entry map: %.4s\n", leader_buffer + 20);

  // ---------------------------------------------------------
  // 3. EXTRACT DIRECTORY ENTRY FORMAT
  // ---------------------------------------------------------

  // The Entry Map is at Leader offsets 20-23.
  // Each character specifies the width of one Directory entry component.
  //
  // Our Entry Map is "6604":
  //   6 = Field Length width
  //   6 = Field Position width
  //   0 = Reserved width
  //   4 = Tag width
  //
  // Subtracting '0' converts an ASCII digit into its integer value.
  int length_field_size = leader_buffer[20] - '0';
  int position_field_size = leader_buffer[21] - '0';
  int reserved_field_size = leader_buffer[22] - '0';
  int tag_field_size = leader_buffer[23] - '0';

  // Base address is the position, relative to the beginning of this
  // record, where the Field Area starts.
  // The Leader stores it as a 5-character numeric string.
  int base_address = atoi(leader_buffer + 12);

  // One Directory entry contains:
  // Tag + Field Length + Field Position + Reserved bytes.
  //
  // For Entry Map "6604":
  // 4 + 6 + 6 + 0 = 16 bytes per entry.
  int entry_size = length_field_size + position_field_size +
                   reserved_field_size + tag_field_size;

  // Directory starts immediately after the 24-byte Leader.
  // It ends with a one-byte Field/Directory terminator.
  //
  // Base address = 281
  // Directory size = 281 - 24 - 1 = 256 bytes.
  int directory_size = base_address - LEADER_SIZE - 1;

  // Every Directory entry has the same size.
  // Divide the total Directory size by the entry size.
  int entry_count = directory_size / entry_size;

  printf("\n--- ISO 8211 Directory ---\n");

  printf("Length field size   : %d\n", length_field_size);
  printf("Position field size : %d\n", position_field_size);
  printf("Reserved field size : %d\n", reserved_field_size);
  printf("Tag field size      : %d\n", tag_field_size);

  printf("Directory size      : %d bytes\n", directory_size);
  printf("Entry size          : %d bytes\n", entry_size);
  printf("Entry count         : %d\n", entry_count);

  // ---------------------------------------------------------
  // 4. READ DIRECTORY
  // ---------------------------------------------------------

  // Allocate memory to hold the entire Directory.
  // +1 provides extra space if we later want to add a '\0'.
  char *directory_buffer = malloc(directory_size + 1);

  if (directory_buffer == NULL) {
    perror("malloc");
    close(file_fd);
    return EXIT_FAILURE;
  }

  // Read the Directory after the Leader.
  // The file descriptor is already positioned at offset 24
  // because we previously read the 24-byte Leader.
  ssize_t directory_read = 0;

  while (directory_read < directory_size) {
    // Read the remaining Directory bytes into the next free buffer position.
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

    // Update the number of Directory bytes received.
    directory_read += bytes_read;
  }

  // ---------------------------------------------------------
  // 5. PARSE DIRECTORY ENTRIES
  // ---------------------------------------------------------

  printf("\n--- Parsed Directory Entries ---\n");
  printf("Entry\tTag\tLength\tPosition\n");

  for (int i = 0; i < entry_count; i++) {
    // Each tag has 4 bytes + 1 byte for the C string terminator.
    char tag[5];

    // Length and Position each have 6 bytes + 1 byte for '\0'.
    char length_str[7];
    char position_str[7];

    // Find the beginning of the current Directory entry.
    //
    // Entry 0 starts at offset 0.
    // Entry 1 starts at offset 16.
    // Entry 2 starts at offset 32.
    //
    // Formula: current entry offset = i * entry_size.
    char *entry = directory_buffer + (i * entry_size);

    // Extract Tag.
    // The first tag_field_size bytes belong to the Tag.
    memcpy(tag, entry, tag_field_size);
    tag[tag_field_size] = '\0';

    // Extract Field Length.
    // It starts immediately after the Tag.
    memcpy(length_str, entry + tag_field_size, length_field_size);
    length_str[length_field_size] = '\0';

    // Extract Field Position.
    // It starts after Tag + Field Length + Reserved bytes.
    memcpy(position_str,
           entry + tag_field_size + length_field_size + reserved_field_size,
           position_field_size);

    position_str[position_field_size] = '\0';

    // Directory stores Length and Position as numeric text.
    // Convert them into integers for calculations.
    int field_length = atoi(length_str);
    int field_position = atoi(position_str);

    printf("%02d\t%s\t%d\t%d\n", i + 1, tag, field_length, field_position);
  }

  // ---------------------------------------------------------
  // 6. EXTRACT THE 0000 FIELD FROM THE DDR
  // ---------------------------------------------------------

  // The first Directory entry describes the 0000 field:
  //
  // Tag      = 0000
  // Length   = 123 bytes
  // Position = 0
  //
  // Absolute file offset = record start + base address + field position.
  // The DDR record starts at file offset 0.
  // Therefore: 0 + 281 + 0 = 281.
  //
  // These values are currently hardcoded for this experiment.
  int field_offset = base_address;
  int field_length = 123;

  // Allocate enough memory for all 123 bytes of the 0000 field.
  char *field_buffer = malloc(field_length);

  if (field_buffer == NULL) {
    perror("malloc");
    close(file_fd);
    free(directory_buffer);
    return EXIT_FAILURE;
  }

  // Move the file descriptor to the beginning of the 0000 field.
  // SEEK_SET means the offset is measured from the beginning of the file.
  lseek(file_fd, field_offset, SEEK_SET);

  // Read the complete field into field_buffer.
  read(file_fd, field_buffer, field_length);

  printf("0000 field\n");

  // Print the field as characters.
  // Control bytes such as UT and FT may not display as normal characters.
  for (int i = 0; i < field_length; i++) {
    printf("%c ", field_buffer[i]);
  }

  printf("\n");

  // ---------------------------------------------------------
  // 7. PRINT 0000 FIELD AS HEXADECIMAL
  // ---------------------------------------------------------

  printf("\n--- 0000 Field (Hex Dump) ---\n");

  for (int i = 0; i < field_length; i++) {
    // Convert to unsigned char before printing with %02X.
    // This avoids sign-extension issues when char is signed.
    //
    // %02X prints each byte as two hexadecimal digits.
    printf("%02X ", (unsigned char)field_buffer[i]);

    // Print a newline after every 16 bytes for readability.
    if ((i + 1) % 16 == 0)
      printf("\n");
  }

  printf("\n");

  // ---------------------------------------------------------
  // 8. EXTRACT PARENT/CHILD TAG PAIRS FROM THE 0000 FIELD
  // ---------------------------------------------------------

  printf("\n--- 0000 Field Tag Pairs ---\n");

  // The 0000 field has this structure in our current file:
  //
  // Offset 0-5   : Field controls ("0000;&")
  // Offset 6-8   : Optional external file title (3 spaces here)
  // Offset 9     : UT separator (0x1F)
  // Offset 10-121: Parent/child tag pairs
  // Offset 122   : Field Terminator (FT, 0x1E)
  //
  // Therefore, the first tag pair begins at offset 10.
  // These offsets are based on the current field layout.
  int pair_start = 10;

  // Each pair consists of two tags:
  //
  // Parent tag = tag_field_size bytes
  // Child tag  = tag_field_size bytes
  //
  // Our Leader says tag_field_size = 4.
  // So one complete pair occupies 4 * 2 = 8 bytes.
  int pair_size = tag_field_size * 2;

  // Calculate how many complete pairs are present.
  //
  // field_length = 123 bytes total
  // pair_start   = 10 bytes before the pair list
  // 1            = final FT byte, which is not a pair
  // pair_size    = 8 bytes per pair
  //
  // (123 - 10 - 1) / 8 = 14 pairs.
  int pair_count = (field_length - pair_start - 1) / pair_size;

  // Process one parent/child pair in each loop iteration.
  for (int i = 0; i < pair_count; i++) {

    // A tag contains 4 characters.
    // Allocate 5 bytes so we can append the C string terminator '\0'.
    char parent[5];
    char child[5];

    // Calculate the starting offset of the current pair.
    //
    // Pair 0: 10 + (0 * 8) = 10
    // Pair 1: 10 + (1 * 8) = 18
    // Pair 2: 10 + (2 * 8) = 26
    //
    // Each pair starts 8 bytes after the previous pair.
    int offset = pair_start + (i * pair_size);

    // Extract the parent tag.
    // Copy tag_field_size bytes from the current pair's starting offset.
    memcpy(parent, field_buffer + offset, tag_field_size);

    // memcpy() copies only the 4 tag bytes.
    // Add '\0' to make the result a valid C string.
    parent[tag_field_size] = '\0';

    // Extract the child tag.
    // It begins immediately after the parent tag.
    //
    // Child offset = pair offset + parent tag width.
    memcpy(child, field_buffer + offset + tag_field_size, tag_field_size);

    // Add the C string terminator to the child tag too.
    child[tag_field_size] = '\0';

    // Display the relationship represented by this pair.
    printf("%s -> %s\n", parent, child);
  }

  // ---------------------------------------------------------
  // 9. READ FIRST DR LEADER
  // ---------------------------------------------------------
  unsigned long first_dr_offset = record_length;
  char first_dr_leader_buffer[LEADER_SIZE + 1];

  lseek(file_fd, first_dr_offset, SEEK_SET);

  total_read = 0;

  while (total_read < LEADER_SIZE) {
    ssize_t bytes_read = read(file_fd, first_dr_leader_buffer + total_read,
                              LEADER_SIZE - total_read);

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
  first_dr_leader_buffer[LEADER_SIZE] = '\0';
  printf("\n---- ISO 8211 DR LEADER ---\n");
  printf("%s\n", first_dr_leader_buffer);
  printf("\n");

  char dr_record_legth_str[6];

  memcpy(dr_record_legth_str, first_dr_leader_buffer, 5);
  dr_record_legth_str[5] = '\0';

  unsigned long dr_record_length = strtoul(dr_record_legth_str, NULL, 10);
  printf("Record length: %lu bytes\n", dr_record_length);

  printf("Interchange level: %c\n", first_dr_leader_buffer[5]);
  printf("Leader identifier: %c\n", first_dr_leader_buffer[6]);
  printf("Inline code extension: %c\n", first_dr_leader_buffer[7]);
  printf("Version: %c\n", first_dr_leader_buffer[8]);
  printf("Application indicator: %c\n", first_dr_leader_buffer[9]);

  // Print other fixed-width Leader fields.
  printf("Field control length: %.2s\n", first_dr_leader_buffer + 10);
  printf("Base address: %.5s\n", first_dr_leader_buffer + 12);
  printf("Extended character set: %.3s\n", first_dr_leader_buffer + 17);
  printf("Entry map: %.4s\n", first_dr_leader_buffer + 20);

  int dr_base_address = atoi(first_dr_leader_buffer + 12);
  int dr_length_field_size = first_dr_leader_buffer[20] - '0';
  int dr_position_field_size = first_dr_leader_buffer[21] - '0';
  int dr_reserved_field_size = first_dr_leader_buffer[22] - '0';
  int dr_tag_field_size = first_dr_leader_buffer[23] - '0';

  int dr_entry_size = dr_length_field_size + dr_position_field_size +
                      dr_reserved_field_size + dr_tag_field_size;

  int dr_directory_size = dr_base_address - LEADER_SIZE - 1;

  int dr_entry_count = dr_directory_size / dr_entry_size;

  // printf("directory size : %d\nentry count : %d\nentry size : %d\n",
  //        dr_directory_size, dr_entry_count, dr_entry_size);
  //

  printf("\n--- ISO 8211 DR Directory ---\n");

  printf("Length field size   : %d\n", dr_length_field_size);
  printf("Position field size : %d\n", dr_position_field_size);
  printf("Reserved field size : %d\n", dr_reserved_field_size);
  printf("Tag field size      : %d\n", dr_tag_field_size);

  printf("Directory size      : %d bytes\n", dr_directory_size);
  printf("Entry size          : %d bytes\n", dr_entry_size);
  printf("Entry count         : %d\n", dr_entry_count);

  char *dr_directory_buffer = malloc(dr_directory_size + 1);

  if (dr_directory_buffer == NULL) {
    perror("malloc");
    close(file_fd);
    free(directory_buffer);
    free(field_buffer);
    return EXIT_FAILURE;
  }

  ssize_t dr_directory_read = 0;

  while (dr_directory_read < dr_directory_size) {
    ssize_t bytes_read = read(file_fd, dr_directory_buffer + dr_directory_read,
                              dr_directory_size - (ssize_t)dr_directory_read);

    if (bytes_read == -1) {
      if (errno == EINTR)
        continue;

      perror("Directory read failed");
      close(file_fd);
      free(dr_directory_buffer);
      free(directory_buffer);
      free(field_buffer);
      return EXIT_FAILURE;
    }

    if (bytes_read == 0) {
      fprintf(stderr, "Unexpected end of file\n");
      close(file_fd);
      free(dr_directory_buffer);
      free(directory_buffer);
      free(field_buffer);
      return EXIT_FAILURE;
    }
    dr_directory_read += bytes_read;
  }

  printf("\n--- Parsed Directory Entries ---\n");
  printf("Entry\tTag\tLength\tPosition\n");

  int dsid_length = 0;
  int dsid_position = 0;
  for (int i = 0; i < dr_entry_count; i++) {
    char tag[5];
    char length_str[7];
    char position_str[7];

    char *entry = dr_directory_buffer + (i * dr_entry_size);
    memcpy(tag, entry, dr_tag_field_size);
    tag[dr_tag_field_size] = '\0';

    memcpy(length_str, entry + dr_tag_field_size, dr_length_field_size);
    length_str[dr_length_field_size] = '\0';

    memcpy(position_str,
           entry + dr_tag_field_size + dr_length_field_size +
               dr_reserved_field_size,
           dr_position_field_size);
    position_str[dr_position_field_size] = '\0';

    int field_length = atoi(length_str);
    int field_position = atoi(position_str);

    if (strcmp(tag, "DSID") == 0) {
      dsid_length = field_length;
      dsid_position = field_position;
    }

    printf("%02d\t%s\t%d\t%d\n", i + 1, tag, field_length, field_position);
  }

  printf("DISD Position : %d\n DSIS Length : %d\n", dsid_position, dsid_length);

  // ---------------------------------------------------------
  // 10. EXTRACT DSID FIELD FROM FIRST DR
  // ---------------------------------------------------------

  // The DR Field Area begins at dr_base_address.
  // DSID position is relative to the beginning of that Field Area.
  //
  // Absolute file offset:
  // DR record start + DR base address + DSID position
  unsigned long dsid_offset = first_dr_offset + dr_base_address + dsid_position;

  printf("\n--- DSID Field ---\n");
  printf("DSID length   : %d bytes\n", dsid_length);
  printf("DSID position : %d\n", dsid_position);
  printf("File offset   : %lu\n", dsid_offset);

  // Allocate memory to hold the complete DSID field.
  char *dsid_buffer = malloc(dsid_length);

  if (dsid_buffer == NULL) {
    perror("malloc");
    close(file_fd);
    free(dr_directory_buffer);
    free(directory_buffer);
    free(field_buffer);
    return EXIT_FAILURE;
  }

  // Move to the DSID field's absolute file offset.
  if (lseek(file_fd, dsid_offset, SEEK_SET) == -1) {
    perror("lseek");
    free(dsid_buffer);
    free(dr_directory_buffer);
    free(directory_buffer);
    free(field_buffer);
    close(file_fd);
    return EXIT_FAILURE;
  }

  // Read the DSID field.
  ssize_t dsid_read = 0;

  while (dsid_read < dsid_length) {
    ssize_t bytes_read =
        read(file_fd, dsid_buffer + dsid_read, dsid_length - (size_t)dsid_read);

    if (bytes_read == -1) {
      if (errno == EINTR)
        continue;

      perror("DSID read failed");
      free(dsid_buffer);
      free(dr_directory_buffer);
      free(directory_buffer);
      free(field_buffer);
      close(file_fd);
      return EXIT_FAILURE;
    }

    if (bytes_read == 0) {
      fprintf(stderr, "Unexpected end of file while reading DSID\n");
      free(dsid_buffer);
      free(dr_directory_buffer);
      free(directory_buffer);
      free(field_buffer);
      close(file_fd);
      return EXIT_FAILURE;
    }

    dsid_read += bytes_read;
  }

  // Print the raw DSID bytes.
  for (int i = 0; i < dsid_length; i++) {
    printf("%02X ", (unsigned char)dsid_buffer[i]);

    if ((i + 1) % 16 == 0)
      printf("\n");
  }

  printf("\n");

  // ---------------------------------------------------------
  // PRINT DSID AS READABLE CHARACTERS
  // ---------------------------------------------------------
  printf("\n--- DSID (Readable) ---\n");

  for (int i = 0; i < dsid_length; i++) {
    unsigned char byte = (unsigned char)dsid_buffer[i];

    // ISO 8211 control characters.
    if (byte == 0x1F) {
      printf("<UT>");
    } else if (byte == 0x1E) {
      printf("<FT>");
    }

    // Printable ASCII characters.
    else if (byte >= 0x20 && byte <= 0x7E) {
      printf("%c", byte);
    }

    // Binary or other non-printable bytes.
    else {
      printf("<0x%02X>", byte);
    }
  }

  printf("\n");
  // ---------------------------------------------------------
  // 10. CLEAN UP
  // ---------------------------------------------------------

  // Release dynamically allocated memory.
  free(field_buffer);
  free(directory_buffer);
  free(dr_directory_buffer);
  free(dsid_buffer);
  // Close the file descriptor.
  close(file_fd);

  return EXIT_SUCCESS;
}

/*
 * CONCEPT NOTES
 *
 * 1. ISO 8211 RECORD LAYOUT
 *
 *    Each record has three main parts:
 *
 *       Leader -> Directory -> Field Area
 *
 *    The DDR (Leader identifier 'L') describes the data structure.
 *    The DR  (Leader identifier 'D') contains data records.
 *
 * 2. DIRECTORY ENTRY
 *
 *    Each Directory entry contains:
 *
 *       Tag | Field Length | Field Position
 *
 *    Field Position is relative to the beginning of the Field Area,
 *    not the beginning of the entire file.
 *
 *    Absolute field offset:
 *
 *       Record start + Base address + Field position
 *
 * 3. FIELD LENGTH
 *
 *    Field Length includes the Field Terminator (FT).
 *
 * 4. C STRINGS AND RAW FILE BYTES
 *
 *    ISO 8211 fields are byte sequences, not automatically C strings.
 *    memcpy() copies bytes but does not append '\0'.
 *    Add '\0' only when you want to treat extracted text as a C string.
 *
 * 5. CURRENT IMPLEMENTATION LIMITATION
 *
 *    The 0000 field's offsets (10 and 122) and its length (123)
 *    are currently specific to the ENC file being tested.
 *
 *    A more general parser will locate the separators and terminators
 *    from the field contents and use the Directory values instead
 *    of relying on hardcoded values.
 */
