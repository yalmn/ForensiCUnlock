#ifndef DISLOCKER_RUNNER_H
#define DISLOCKER_RUNNER_H

#include <stdint.h>

int run_dislocker(const char *image_path, uint64_t start_sector, const char *key, const char *output_path);

#endif
