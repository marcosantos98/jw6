#pragma once

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "types.hpp"

// note: default page size in win32
#define BASE_REGION_CAP 4096

struct ArenaRegion {
    u8 *mem;
    size_t off;
    size_t size;
    ArenaRegion *next;
    ArenaRegion *prev;
};

struct GrowingArena {
    ArenaRegion *current;

    // Only for information
    i32 used;
    i32 total;
    i32 wasted;
    i32 region_cnt;
};

inline ArenaRegion *arena_region_create(GrowingArena *self, size_t required_size) {
    ArenaRegion *region = (ArenaRegion *)malloc(sizeof(ArenaRegion));

    region->size = required_size > BASE_REGION_CAP ? required_size : BASE_REGION_CAP;
    region->mem = (u8 *)malloc(region->size);
    region->off = 0;
    region->next = NULL;

    if (self->current != NULL) {
        region->prev = self->current;
        self->current->next = region;
    } else {
        region->prev = NULL;
    }

    self->region_cnt += 1;

    return region;
}

template <typename T>
static T *arena_alloc(GrowingArena *self, size_t amt) {

    if (self->current == NULL) {
        memset(self, 0, sizeof(GrowingArena));
        self->current = arena_region_create(self, amt);
        self->total += self->current->size;
    }

    if (self->current->off + amt > self->current->size) {
        ArenaRegion *region = self->current;

        while (region != NULL && region->off + amt > region->size) {
            region = region->next;
        }

        if (region == NULL) {
            self->wasted += self->current->size - self->current->off;
            self->current = arena_region_create(self, amt);
            self->total += self->current->size;
        } else {
            self->current = region;
        }
    }

    T *mem = (T *)(self->current->mem + self->current->off);
    self->current->off += amt;

    self->used += amt;

    return mem;
}

template <typename T>
T *arena_realloc(GrowingArena *self, T *old_ptr, size_t old_size, size_t new_size) {
    T *new_ptr = arena_alloc<T>(self, new_size);
    assert(new_ptr != NULL && "realloc requiest memory but is null");
    memcpy(new_ptr, old_ptr, old_size);
    return new_ptr;
}

static void arena_reset(GrowingArena *self) {
    if (self->current == NULL) return; 

    while (self->current->prev != NULL) {
        self->current = self->current->prev;
    }

    ArenaRegion *region = self->current;
    while (region != NULL) {
        region->off = 0; 
        region = region->next;
    }

    self->used = 0;
    self->wasted = 0;
}

static void arena_free(GrowingArena *self) {
    if (self->current != NULL) {
        if (self->current->prev == NULL) {
            free(self->current->mem);
            free(self->current);
        } else {
            assert(self->current->next == NULL && "Looks like the arena didnt reuse all regions!");
            while (self->current->prev != NULL) {
                free(self->current->mem);
                self->current = self->current->prev;
                free(self->current->next);
            }
            free(self->current->mem);
            free(self->current);
        }
    }
}

template<typename ...Args>
static u8* arena_snprintf(GrowingArena* arena, size_t buf_size, const char *const format, Args... args) {
    u8* mem = arena_alloc<u8>(arena, buf_size);
    snprintf(mem, buf_size, format, args...);
    return mem;
}
