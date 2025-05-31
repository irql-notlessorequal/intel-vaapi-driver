#ifndef _INTEL_COMPILER_H_
#define _INTEL_COMPILER_H_

/**
 * Function inlining
 */
#if defined(__GNUC__)
#  define INLINE __inline__
#elif (__STDC_VERSION__ >= 199901L) /* C99 */
#  define INLINE inline
#else
#  define INLINE
#endif

/**
 * Function visibility
 */
#if defined(__GNUC__)
#  define DLL_HIDDEN __attribute__((visibility("hidden")))
#  define DLL_EXPORT __attribute__((visibility("default")))
#else
#  define DLL_HIDDEN
#  define DLL_EXPORT
#endif

/**
 * FALLTHROUGH macro
 */
#if defined(__has_cpp_attribute) && defined(__clang__)
/* We do not do the same trick as __has_attribute because parsing
 * clang::fallthrough in the preprocessor fails in GCC. */
#  define HAS_CLANG_FALLTHROUGH  __has_cpp_attribute(clang::fallthrough)
#else
#  define HAS_CLANG_FALLTHROUGH 0
#endif

#if (defined(__cplusplus) && (__cplusplus >= 201703L)) || \
    (defined(__STDC_VERSION__) && (__STDC_VERSION__ > 201710L))
/* Standard C++17/C23 attribute */
#define FALLTHROUGH [[fallthrough]]
#elif HAS_CLANG_FALLTHROUGH
/* Clang++ specific */
#define FALLTHROUGH [[clang::fallthrough]]
#elif __has_attribute(fallthrough)
/* Non-standard but supported by at least gcc and clang */
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH do { } while(0)
#endif

#endif /* _INTEL_COMPILER_H_ */
