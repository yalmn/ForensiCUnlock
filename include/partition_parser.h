// partition_parser.h
#ifndef PARTITION_PARSER_H
#define PARTITION_PARSER_H

#include <stdint.h>

typedef struct {
    int slot;
    uint64_t start;
    uint64_t length;
} PartitionInfo;

int find_bdp_partition(const char *image_path, PartitionInfo *info);

#endif
