/*
 * Copyright (C) 2007 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "object_heap.h"

#include "assert.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#define ASSERT  assert

#define LAST_FREE   -1
#define ALLOCATED   -2

static int
log2_if_power_of_two(int n)
{
	if (n <= 0 || (n & (n - 1)) != 0)
		return -1;

	int shift = 0;
	while ((1 << shift) < n)
		++shift;
	return shift;
}

/*
 * Expands the heap
 * Return 0 on success, -1 on error
 */
static int
object_heap_expand(object_heap_p heap)
{
	int i;
	int next_free;
	int new_heap_size = heap->heap_size + heap->heap_increment;
	int bucket_index = new_heap_size / heap->heap_increment - 1;

	if (bucket_index >= heap->num_buckets)
	{
		int new_num_buckets = heap->num_buckets + 8;
		void **new_bucket;

		new_bucket = realloc(heap->bucket, new_num_buckets * sizeof(void *));
		if (NULL == new_bucket)
			return -1;

		heap->num_buckets = new_num_buckets;
		heap->bucket = new_bucket;
	}

	/* Allocate storage for the new bucket. */
	char *new_bucket_mem = malloc((size_t)heap->heap_increment * heap->object_size);
	if (NULL == new_bucket_mem)
		return -1;

	heap->bucket[bucket_index] = new_bucket_mem;

	/* Thread new slots onto the head of the free list (highest index first
	 * so that IDs are handed out in ascending order). */
	next_free = heap->next_free;
	for (i = new_heap_size; i-- > heap->heap_size;)
	{
		object_base_p obj = (object_base_p)(new_bucket_mem + (i - heap->heap_size) * heap->object_size);
		obj->id = i + heap->id_offset;
		obj->next_free = next_free;
		next_free = i;
	}

	heap->next_free = next_free;
	heap->heap_size = new_heap_size;
	return 0;
}

/*
 * Return 0 on success, -1 on error
 */
int
object_heap_init(object_heap_p heap, int object_size, int id_offset)
{
	heap->object_size = object_size;
	heap->id_offset = id_offset & OBJECT_HEAP_OFFSET_MASK;
	heap->heap_size = 0;
	heap->heap_increment = OBJECT_HEAP_DEFAULT_INCREMENT;
	heap->heap_shift = log2_if_power_of_two(heap->heap_increment);
	heap->next_free = LAST_FREE;
	heap->num_buckets = 0;
	heap->bucket = NULL;

	if (object_heap_expand(heap) == 0)
	{
		ASSERT(heap->heap_size);
		_i965InitMutex(&heap->mutex);
		return 0;
	}

	ASSERT(heap->heap_size == 0);
	free(heap->bucket);
	heap->bucket = NULL;
	return -1;
}

/*
 * Allocates an object
 * Returns the object ID on success, returns -1 on error
 */
int object_heap_allocate(object_heap_p heap)
{
	object_base_p obj;
	int bucket_index, obj_index;

	if (__atomic_load_n(&heap->next_free, __ATOMIC_RELAXED) == LAST_FREE)
	{
		_i965LockMutex(&heap->mutex);
		if (LAST_FREE == heap->next_free)
		{
			if (-1 == object_heap_expand(heap))
			{
				_i965UnlockMutex(&heap->mutex);
				return -1; /* Out of memory */
			}
		}
		_i965UnlockMutex(&heap->mutex);
	}

	_i965LockMutex(&heap->mutex);
	if (LAST_FREE == heap->next_free)
	{
		if (-1 == object_heap_expand(heap))
		{
			_i965UnlockMutex(&heap->mutex);
			return -1;
		}
	}
	ASSERT(heap->next_free >= 0);

	object_heap_slot_to_indices(heap, heap->next_free, &bucket_index, &obj_index);
	obj = object_heap_bucket_obj(heap, bucket_index, obj_index);

	heap->next_free = obj->next_free;
	_i965UnlockMutex(&heap->mutex);

	obj->next_free = ALLOCATED;
	return obj->id;
}

/*
 * Lookup an object by object ID
 * Returns a pointer to the object on success, returns NULL on error
 */
object_base_p
object_heap_lookup(object_heap_p heap, int id)
{
	object_base_p obj;
	int slot, bucket_index, obj_index;

	_i965LockMutex(&heap->mutex);

	if (id < heap->id_offset || id >= (heap->heap_size + heap->id_offset))
	{
		_i965UnlockMutex(&heap->mutex);
		return NULL;
	}

	slot = id - heap->id_offset; /* zero-based slot index */
	object_heap_slot_to_indices(heap, slot, &bucket_index, &obj_index);
	obj = object_heap_bucket_obj(heap, bucket_index, obj_index);
	_i965UnlockMutex(&heap->mutex);

	/* Check that the object has actually been allocated. */
	if (obj->next_free != ALLOCATED)
		return NULL;

	return obj;
}

/*
 * Iterate over all objects in the heap.
 * Returns a pointer to the first object on the heap, returns NULL if heap is empty.
 */
object_base_p object_heap_first(object_heap_p heap, object_heap_iterator *iter)
{
	*iter = -1;
	return object_heap_next(heap, iter);
}

/*
 * Iterate over all objects in the heap.
 * Returns a pointer to the next object on the heap, returns NULL if heap is empty.
 */
object_base_p
object_heap_next(object_heap_p heap, object_heap_iterator *iter)
{
	int i = *iter + 1;
	int heap_size;
	int bucket_index, obj_index;
	object_base_p obj;

	_i965LockMutex(&heap->mutex);
	heap_size = heap->heap_size;
	_i965UnlockMutex(&heap->mutex);

	while (i < heap_size)
	{
		int allocated;

		object_heap_slot_to_indices(heap, i, &bucket_index, &obj_index);

		_i965LockMutex(&heap->mutex);
		obj = object_heap_bucket_obj(heap, bucket_index, obj_index);
		allocated = (obj->next_free == ALLOCATED);
		_i965UnlockMutex(&heap->mutex);

		if (allocated)
		{
			*iter = i;
			return obj;
		}
		i++;
	}

	*iter = i;
	return NULL;
}

/*
 * Frees an object
 */
void object_heap_free(object_heap_p heap, object_base_p obj)
{
	/* Don't complain about NULL pointers */
	if (NULL != obj) {
		_i965LockMutex(&heap->mutex);
		ASSERT(obj->next_free == ALLOCATED);
		obj->next_free = heap->next_free;
		heap->next_free = obj->id - heap->id_offset;
		_i965UnlockMutex(&heap->mutex);
	}
}

/*
 * Destroys a heap, the heap must be empty.
 */
void object_heap_destroy(object_heap_p heap)
{
	int i;
	int num_buckets;

	if (!heap->heap_size)
		goto clear;

	_i965DestroyMutex(&heap->mutex);

#ifdef DEBUG
    /* Verify that every slot has been freed... */
    for (i = 0; i < heap->heap_size; i++) {
        int bucket_index, obj_index;
        object_base_p obj;
 
        object_heap_slot_to_indices(heap, i, &bucket_index, &obj_index);
        obj = object_heap_bucket_obj(heap, bucket_index, obj_index);
        ASSERT(obj->next_free != ALLOCATED);
    }
#endif

	/* Ensure heap_size is an exact multiple of heap_increment so the bucket
	 * count calculation below is not truncated. */
	ASSERT(heap->heap_size % heap->heap_increment == 0);
	num_buckets = heap->heap_size / heap->heap_increment;

	for (i = 0; i < num_buckets; i++)
		free(heap->bucket[i]);

	free(heap->bucket);

clear:
	heap->bucket = NULL;
	heap->heap_size = 0;
	heap->next_free = LAST_FREE;
}
