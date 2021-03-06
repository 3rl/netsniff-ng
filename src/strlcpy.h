/*
 * netsniff-ng - the packet sniffing beast
 * By Daniel Borkmann <daniel@netsniff-ng.org>
 * Copyright 2009, 2010 Daniel Borkmann.
 * Subject to the GPL, version 2.
 */

#ifndef STRLCPY_H
#define STRLCPY_H

extern size_t strlcpy(char *dest, const char *src, size_t size);
extern int slprintf(char *dst, size_t size, const char *fmt, ...);

#endif /* STRLCPY_H */
