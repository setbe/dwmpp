/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <cstdint>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "util.h"

namespace {

struct Arena {
	struct ArenaChunk *head;
};

struct ArenaChunk {
	char *base;
	size_t size;
	size_t used;
	ArenaChunk *next;
};

static Arena persistent_arena = {NULL};
static Arena transient_arena = {NULL};

static size_t align_up(size_t value, size_t alignment) {
	size_t rem;
	if (alignment == 0 || value > SIZE_MAX - (alignment - 1))
		die("arena alignment overflow");
	rem = value % alignment;
	if (rem == 0)
		return value;
	return value + (alignment - rem);
}

static size_t mul_checked(size_t a, size_t b) {
	if (a != 0 && b > SIZE_MAX / a)
		die("allocation size overflow");
	return a * b;
}

static void *arena_alloc(Arena *arena, size_t bytes, size_t min_reserve) {
	size_t page, aligned, alloc_bytes, used_aligned;
	void *p;
	ArenaChunk *chunk;

	if (bytes == 0)
		return NULL;
	alloc_bytes = align_up(bytes, alignof(max_align_t));
	chunk = arena->head;
	used_aligned = chunk ? align_up(chunk->used, alignof(max_align_t)) : 0;
	if (!chunk || used_aligned > chunk->size || alloc_bytes > (chunk->size - used_aligned)) {
		page = (size_t)sysconf(_SC_PAGESIZE);
		aligned = align_up(alloc_bytes, page);
		if (aligned < min_reserve)
			aligned = min_reserve;
		chunk = (ArenaChunk *)mmap(NULL, sizeof(ArenaChunk), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (chunk == MAP_FAILED)
			die("mmap:");
		chunk->base = (char *)mmap(NULL, aligned, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (chunk->base == MAP_FAILED)
			die("mmap:");
		chunk->size = aligned;
		chunk->used = 0;
		chunk->next = arena->head;
		arena->head = chunk;
		used_aligned = 0;
	}
	p = chunk->base + used_aligned;
	memset(p, 0, bytes);
	chunk->used = used_aligned + alloc_bytes;
	return p;
}

} // namespace

void
die(const char *fmt, ...)
{
	va_list ap;
	int saved_errno;

	saved_errno = errno;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':')
		fprintf(stderr, " %s", strerror(saved_errno));
	fputc('\n', stderr);

	exit(1);
}

void *
ecalloc(size_t nmemb, size_t size)
{
	return arena_alloc(&persistent_arena, mul_checked(nmemb, size), (1u << 20));
}

void *
tcalloc(size_t nmemb, size_t size)
{
	return arena_alloc(&transient_arena, mul_checked(nmemb, size), (1u << 16));
}

void
wm_transient_reset(void)
{
	for (ArenaChunk *chunk = transient_arena.head; chunk; chunk = chunk->next)
		chunk->used = 0;
}
