#ifndef LILY_UTILS_WRAPPERS_H
#define LILY_UTILS_WRAPPERS_H

#include "utils/types.h"

/*
 *
 * malloc() wrapper that causes a failure should malloc() fail
 *
 */
void *lily_alloc(usize size);

/*
 *
 * aligned_alloc() wrapper that causes a failure should malloc() fail
 *
 */
void *lily_aligned_alloc(usize alignment, usize size);

/*
 *
 * realloc() wrapper that causes a failure should realloc() fail
 *
 */
void *lily_realloc(void *ptr, usize size);

/*
 *
 * realloc() wrapper that causes a failure should realloc() fail
 *
 */
void *lily_calloc(usize number_of_members, usize size_of_member);

void lily_free(void *ptr);

#endif // !LILY_UTILS_WRAPPERS_H
