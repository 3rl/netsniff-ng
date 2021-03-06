/*
 * netsniff-ng - the packet sniffing beast
 * By Daniel Borkmann <daniel@netsniff-ng.org>
 * Copyright 2011 Daniel Borkmann.
 * Subject to the GPL, version 2.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <assert.h>
#include <unistd.h>

#include "pcap.h"
#include "xmalloc.h"
#include "write_or_die.h"
#include "opt_memcpy.h"
#include "locking.h"

#define PAGE_SIZE         (getpagesize())
#define PAGE_MASK         (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(addr)  (((addr) + PAGE_SIZE - 1) & PAGE_MASK)

#define IOVSIZ   1000
#define ALLSIZ   (PAGE_SIZE * 3)

static struct iovec iov[IOVSIZ];
static unsigned long c = 0;
static struct spinlock lock;
static ssize_t avail, used, iov_used;

static int pcap_sg_pull_file_header(int fd)
{
	ssize_t ret;
	struct pcap_filehdr hdr;

	ret = read(fd, &hdr, sizeof(hdr));
	if (unlikely(ret != sizeof(hdr)))
		return -EIO;
	pcap_validate_header_maybe_die(&hdr);

	return 0;
}

static int pcap_sg_push_file_header(int fd)
{
	ssize_t ret;
	struct pcap_filehdr hdr;

	memset(&hdr, 0, sizeof(hdr));
	pcap_prepare_header(&hdr, LINKTYPE_EN10MB, 0,
			    PCAP_DEFAULT_SNAPSHOT_LEN);
	ret = write_or_die(fd, &hdr, sizeof(hdr));
	if (unlikely(ret != sizeof(hdr))) {
		whine("Failed to write pkt file header!\n");
		return -EIO;
	}

	return 0;
}

static ssize_t pcap_sg_write_pcap_pkt(int fd, struct pcap_pkthdr *hdr,
				      uint8_t *packet, size_t len)
{
	ssize_t ret;
	spinlock_lock(&lock);
	if (unlikely(c == IOVSIZ)) {
		ret = writev(fd, iov, IOVSIZ);
		if (ret < 0)
			panic("writev I/O error!\n");
		c = 0;
	}
	iov[c].iov_len = 0;
	__memcpy_small(iov[c].iov_base, hdr, sizeof(*hdr));
	iov[c].iov_len += sizeof(*hdr);
	__memcpy(iov[c].iov_base + iov[c].iov_len, packet, len);
	iov[c].iov_len += len;
	ret = iov[c].iov_len;
	c++;
	spinlock_unlock(&lock);
	return ret;
}

static int pcap_sg_prepare_reading_pcap(int fd)
{
	spinlock_lock(&lock);
	avail = readv(fd, iov, IOVSIZ);
	if (avail <= 0)
		return -EIO;
	used = iov_used = 0;
	c = 0;
	spinlock_unlock(&lock);
	return 0;
}

static ssize_t pcap_sg_read_pcap_pkt(int fd, struct pcap_pkthdr *hdr,
				     uint8_t *packet, size_t len)
{
	/* In contrast to writing, reading gets really ugly ... */
	spinlock_lock(&lock);
	if (likely(avail - used >= sizeof(*hdr) &&
		   iov[c].iov_len - iov_used >= sizeof(*hdr))) {
		__memcpy_small(hdr, iov[c].iov_base + iov_used, sizeof(*hdr));
		iov_used += sizeof(*hdr);
		used += sizeof(*hdr);
	} else {
		size_t remainder, offset = 0;
		if (avail - used < sizeof(*hdr))
			return -ENOMEM;
		offset = iov[c].iov_len - iov_used;
		remainder = sizeof(*hdr) - offset;
		assert(offset + remainder == sizeof(*hdr));
		__memcpy_small(hdr, iov[c].iov_base + iov_used, offset);
		used += offset;
		iov_used = 0;
		c++;
		if (c == IOVSIZ) {
			/* We need to refetch! */
			c = 0;
			avail = readv(fd, iov, IOVSIZ);
			if (avail < 0)
				return -EIO;
			used = 0;
		}
		/* Now we copy the remainder and go on with business ... */
		__memcpy_small(hdr, iov[c].iov_base + iov_used, remainder);
		iov_used += remainder;
		used += remainder;
	}
	if (likely(avail - used >= hdr->len &&
		   iov[c].iov_len - iov_used >= hdr->len)) {
		__memcpy(packet, iov[c].iov_base + iov_used, hdr->len);
		iov_used += hdr->len;
		used += hdr->len;
	} else {
		size_t remainder, offset = 0;
		if (avail - used < hdr->len)
			return -ENOMEM;
		offset = iov[c].iov_len - iov_used;
		remainder = hdr->len - offset;
		assert(offset + remainder == hdr->len);
		__memcpy(packet, iov[c].iov_base + iov_used, offset);
		used += offset;
		iov_used = 0;
		c++;
		if (c == IOVSIZ) {
			/* We need to refetch! */
			c = 0;
			avail = readv(fd, iov, IOVSIZ);
			if (avail < 0)
				return -EIO;
			used = 0;
		}
		/* Now we copy the remainder and go on with business ... */
		__memcpy(packet, iov[c].iov_base + iov_used, remainder);
		iov_used += remainder;
		used += remainder;
	}
	spinlock_unlock(&lock);
	if (unlikely(hdr->len == 0))
		return -EINVAL; /* Bogus packet */
	return sizeof(*hdr) + hdr->len;
}

static void pcap_sg_fsync_pcap(int fd)
{
	ssize_t ret;
	spinlock_lock(&lock);
	ret = writev(fd, iov, c);
	if (ret < 0)
		panic("writev I/O error!\n");
	c = 0;
	fdatasync(fd);
	spinlock_unlock(&lock);
}

struct pcap_file_ops pcap_sg_ops __read_mostly = {
	.name = "SCATTER/GATHER",
	.pull_file_header = pcap_sg_pull_file_header,
	.push_file_header = pcap_sg_push_file_header,
	.write_pcap_pkt = pcap_sg_write_pcap_pkt,
	.prepare_reading_pcap =  pcap_sg_prepare_reading_pcap,
	.read_pcap_pkt = pcap_sg_read_pcap_pkt,
	.fsync_pcap = pcap_sg_fsync_pcap,
};

int init_pcap_sg(void)
{
	unsigned long i;
	c = 0;
	memset(iov, 0, sizeof(iov));
	for (i = 0; i < IOVSIZ; ++i) {
		iov[i].iov_base = xmalloc_aligned(ALLSIZ, 64);
		iov[i].iov_len = ALLSIZ;
	}
	spinlock_init(&lock);
	return pcap_ops_group_register(&pcap_sg_ops, PCAP_OPS_SG);
}

void cleanup_pcap_sg(void)
{
	unsigned long i;
	spinlock_destroy(&lock);
	for (i = 0; i < IOVSIZ; ++i)
		xfree(iov[i].iov_base);
	pcap_ops_group_unregister(PCAP_OPS_SG);
}

