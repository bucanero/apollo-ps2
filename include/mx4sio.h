#ifndef MX4SIO_H
#define MX4SIO_H

#include <stddef.h>

// External Saves source for the MX4SIO SD card, resolved to its "massN:/" unit
#define MX4SIO_PATH		"mx4sio:/"

// Loads the MX4SIO driver if needed and finds the SD card: "massN:/" in path, 0 if not found
int mx4sio_get_path(char* path, size_t len);

#endif
