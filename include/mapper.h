// include/mapper.h
#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>

int create_mapping_file(const char *loop_orig, const char *loop_decrypted, uint64_t bdp_start, uint64_t bdp_length);
int setup_dm_device();

#endif