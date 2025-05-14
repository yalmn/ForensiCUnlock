// include/loop_device.h
#ifndef LOOP_DEVICE_H
#define LOOP_DEVICE_H

int setup_loop_device(const char *file_path, char *loop_path, size_t max_len);

#endif