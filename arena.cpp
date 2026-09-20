// https://www.gingerbill.org/article/2019/02/08/memory-allocation-strategies-002/

#include <cstdint>
#include <cstring>

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)

struct memory_arena
{
	uint8_t *buffer;
	size_t bufferSize;
	size_t currentOffset;
};

void initializeArena(memory_arena *arena, size_t bufferSize, uint8_t *base)
{
	arena->buffer = base;
	arena->bufferSize = bufferSize;
	arena->currentOffset = 0;
}

#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#define PushSize(arena, type) (type *)PushSize_(arena, sizeof(type))
#define PushArray(arena, Count, type) (type *)PushSize_(arena, (Count) * sizeof(type))

bool isPowerOfTwo(uintptr_t x)
{
	return (x & (x - 1)) == 0;
}

uintptr_t alignForward(uintptr_t ptr, size_t align) 
{
	if (!isPowerOfTwo(align)) {
		printf("alignment: %llu is not a power of two\n", align);
	}
	uintptr_t p, a, modulo;
	p = ptr;
	a = (uintptr_t)align;
	// Same as (p % a) but faster as 'a' is a power of two
	modulo = p & (a-1);

	if (modulo != 0) {
		// If 'p' address is not aligned, push the address to the
		// next value which is aligned
		p += a - modulo;
	}
	return p;
}

void *PushSize_(memory_arena *arena, size_t size)
{
    uintptr_t startingOffset = (uintptr_t)(arena->buffer + arena->currentOffset);
    uintptr_t finalOffset = alignForward(startingOffset, DEFAULT_ALIGNMENT);
    finalOffset -= (uintptr_t)arena->buffer; // relative offset from the start
	printf("alloc %llu at offset: %llu\n", size, finalOffset);

    if (finalOffset + size > arena->bufferSize) {
    	printf("arena is out of memory\n");
    	return NULL;
    }
    void *ptr = &arena->buffer[finalOffset];
    arena->currentOffset = finalOffset + size;
    memset(ptr, 0, size);
    return ptr;
}