/*
 * Copyright (C) 2014-2023 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_TAG "dma_allocator"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>


#include "dma_allocator.h"

#include <cutils/log.h>

#define DEVPATH "/dev/dma_heap"


int aml_dmabuf_heap_open(char *name)
{
	int ret, fd;
	char buf[256];
	ALOGI("[%s: %d ] %s", __func__, __LINE__, name);

	ret = snprintf(buf, 256, "%s/%s", DEVPATH, name);
	if (ret < 0) {
		ALOGE("[%s: %d ] snprintf failed!", __func__, __LINE__);
		return ret;
	}

	ALOGI("[%s: %d ] %s", __func__, __LINE__, buf);
	fprintf(stderr, "%s : %s", __FUNCTION__, buf);

	fd = open(buf, O_RDWR);
	if (fd < 0)
	ALOGE("[%s: %d ] open %s failed!", __func__, __LINE__, buf);
	return fd;
}


static int dmabuf_heap_alloc_fdflags(int heap_fd, size_t len, unsigned int fd_flags,
				     unsigned int heap_flags, int *dmabuf_fd)
{
	struct dma_heap_allocation_data data = {
		.len = len,
		.fd = 0,
		.fd_flags = fd_flags,
		.heap_flags = heap_flags,
	};
	int ret;

	if (!dmabuf_fd) {
	ALOGE("[%s: %d ] dmabuf_fd:%p invalid!", __func__, __LINE__, dmabuf_fd);
		return -EINVAL;
	}

	ret = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data);
	if (ret < 0) {
		ALOGE("[%s: %d ] ioctl failed, ret=%d!", __func__, __LINE__, ret);
		return ret;
	}
	*dmabuf_fd = (int)data.fd;
	ALOGI("[%s: %d ] ret=%d, heap_fd=%d, len=%ld, dmabuf_fd=%d", __func__, __LINE__,ret, heap_fd, len, *dmabuf_fd);
	return ret;
}



int aml_dmabuf_heap_alloc(int heap_fd, size_t len, int *dmabuf_fd)
{
	int ret;
	ret = dmabuf_heap_alloc_fdflags(heap_fd, len, O_RDWR | O_CLOEXEC, 0,
					 dmabuf_fd);
	ALOGI("[%s: %d ] ret=%d, heap_fd=%d, len=%ld, dmabuf_fd=%d", __func__, __LINE__, ret, heap_fd, len, *dmabuf_fd);
	return ret;
}



int aml_dmabuf_heap_free(int dmabuf_fd)
{
	ALOGI("[%s: %d ] dmabuf_fd=%d", __func__, __LINE__, dmabuf_fd);
	return close(dmabuf_fd);
}


// DMA_BUF_SYNC_START
// DMA_BUF_SYNC_END
int aml_dmabuf_sync(int dmabuf_fd, int start_stop)
{
	struct dma_buf_sync sync = {
		.flags = start_stop | DMA_BUF_SYNC_RW,
	};

	ALOGI("[%s: %d ] dmabuf_fd=%d, start_stop=%d", __func__, __LINE__, dmabuf_fd, start_stop);

	return ioctl(dmabuf_fd, DMA_BUF_IOCTL_SYNC, &sync);
}

