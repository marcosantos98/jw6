#pragma once

#include "arena.hpp"
#include "types.hpp"
#include <cassert>
#include <cstdio>

#define INITIAL_CAP 1024

template<typename T>
struct daa {
    T* items;
    i32 cap;
    i32 count;    
    GrowingArena* arena;

    void append(T item) {
        if (items == NULL) {
            items = arena_alloc<T>(arena, sizeof(T) * cap);
        }

        if (count >= cap) {
            i32 new_cap = cap * 2;
            items = arena_realloc<T>(arena, items, sizeof(T) * cap, sizeof(T) * new_cap);
            assert(items != NULL && "Memory isnt valid anymore!");
            cap = new_cap;
            
        }
        items[count] = item;
        count += 1;
    }
	
	T pop() {
		return items[--count];
	}

    size_t total_size() const {
        return sizeof(T) * count;
    }

    T *data() const {
        return items;
    }

    void clear() {
        count = 0;
    }
};

template <typename T>
static daa<T> make(GrowingArena* arena, i32 initial_cap = INITIAL_CAP) {
    T* items = arena_alloc<T>(arena, sizeof(T) * initial_cap);
    assert(items != NULL && "Arena allocator failed!");
    return daa<T>{items, initial_cap, 0, arena};
}
