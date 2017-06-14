#ifndef PLATFORM_CONFIG_H

#define PLATFORM_CONFIG_H

// Modify this header file to make the HACD data types be compatible with your
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <new>

// This header file provides a brief compatibility layer between the PhysX and APEX SDK foundation header files.
// Modify this header file to your own data types and memory allocation routines and do a global find/replace if necessary


/**
Compiler define
*/
#ifdef _MSC_VER 
#	define HACD_VC
#   if _MSC_VER >= 1500
#       define HACD_VC9
#	elif _MSC_VER >= 1400
#		define HACD_VC8
#	elif _MSC_VER >= 1300
#		define HACD_VC7
#	else
#		define HACD_VC6
#	endif
#elif __GNUC__ || __SNC__
#	define HACD_GNUC
#elif defined(__MWERKS__)
#	define	HACD_CW
#else
#	error "Unknown compiler"
#endif


/**
Platform define
*/
#ifdef HACD_VC
#	ifdef _M_IX86
#		define HACD_X86
#		define HACD_WINDOWS
#   elif defined(_M_X64)
#       define HACD_X64
#       define HACD_WINDOWS
#	elif defined(_M_PPC)
#		define HACD_PPC
#		define HACD_X360
#		define HACD_VMX
#	else
#		error "Unknown platform"
#	endif
#elif defined HACD_GNUC
#   ifdef __CELLOS_LV2__
#	define HACD_PS3
#   elif defined(__arm__)
#	define HACD_LINUX
#	define HACD_ARM
#   elif defined(__i386__)
#       define HACD_X86
#   elif defined(__x86_64__)
#       define HACD_X64
#   elif defined(__ppc__)
#       define HACD_PPC
#   elif defined(__ppc64__)
#       define HACD_PPC
#	define HACD_PPC64
#   else
#	error "Unknown platform"
#   endif
#	if defined(ANDROID)
#   	define HACD_ANDROID
#	elif defined(__linux__)
#   	define HACD_LINUX
#	elif defined(__APPLE__)
#   	define HACD_APPLE
#	elif defined(__CYGWIN__)
#   	define HACD_CYGWIN
#   	define HACD_LINUX
#	endif
#elif defined HACD_CW
#	if defined(__PPCGEKKO__)
#		if defined(RVL)
#			define HACD_WII
#		else
#			define HACD_GC
#		endif
#	else
#		error "Unknown platform"
#	endif
#endif


/**
Inline macro
*/
#if defined(HACD_WINDOWS) || defined(HACD_X360)
#	define HACD_INLINE inline
#	pragma inline_depth( 255 )
#else
#	define HACD_INLINE inline
#endif

/**
Force inline macro
*/
#if defined(HACD_VC)
#define HACD_FORCE_INLINE __forceinline
#elif defined(HACD_LINUX) // Workaround; Fedora Core 3 do not agree with force inline and PxcPool
#define HACD_FORCE_INLINE inline
#elif defined(HACD_GNUC)
#define HACD_FORCE_INLINE inline __attribute__((always_inline))
#else
#define HACD_FORCE_INLINE inline
#endif

/**
Noinline macro
*/
#if defined HACD_WINDOWS
#	define HACD_NOINLINE __declspec(noinline)
#elif defined(HACD_GNUC)
#	define HACD_NOINLINE __attribute__ ((noinline))
#else
#	define HACD_NOINLINE 
#endif


/**
General defines
*/


// avoid unreferenced parameter warning (why not just disable it?)
// PT: or why not just omit the parameter's name from the declaration????
#define HACD_FORCE_PARAMETER_REFERENCE(_P) (void)(_P);
#define HACD_UNUSED(_P) HACD_FORCE_PARAMETER_REFERENCE(_P)

// check that exactly one of NDEBUG and _DEBUG is defined
//#if !(defined NDEBUG ^ defined _DEBUG)
//#error Exactly one of NDEBUG and _DEBUG needs to be defined by preprocessor
//#endif

namespace hacd
{
	class PxEmpty;

#define HACD_SIGN_BITMASK		0x80000000

extern size_t gAllocCount;
extern size_t gAllocSize;

HACD_INLINE void * AllocAligned(size_t size,size_t alignment)
{
	gAllocSize+=size;
	gAllocCount++;
	void *ret = NULL;
#ifdef HACD_WINDOWS
	ret = ::_aligned_malloc(size,alignment);
#else
	posix_memalign(&ret,alignment,size);
#endif
	return ret;
}

HACD_INLINE void FreeAligned(void *memory)
{
#ifdef HACD_WINDOWS
	::_aligned_free(memory);
#else
	::free(memory);
#endif
}

#define HACD_ALLOC_ALIGNED(x,y) hacd::AllocAligned(x,y)
#define HACD_ALLOC(x) hacd::AllocAligned(x,16)
#define HACD_FREE(x)  hacd::FreeAligned(x)

#define HACD_ASSERT(x) assert(x)
#define HACD_ALWAYS_ASSERT() assert(0)

#define HACD_PLACEMENT_NEW(p, T)  new(p) T

	class UserAllocated
	{
	public:
		HACD_INLINE void* operator new(size_t size,UserAllocated *t)
		{
			HACD_FORCE_PARAMETER_REFERENCE(size);
			return t;
		}

		HACD_INLINE void* operator new(size_t size,const char *className,const char* fileName, int lineno,size_t classSize)
		{
			HACD_FORCE_PARAMETER_REFERENCE(className);
			HACD_FORCE_PARAMETER_REFERENCE(fileName);
			HACD_FORCE_PARAMETER_REFERENCE(lineno);
			HACD_FORCE_PARAMETER_REFERENCE(classSize);
			return HACD_ALLOC(size);
		}

		inline void* operator new[](size_t size,const char *className,const char* fileName, int lineno,size_t classSize)
		{
			HACD_FORCE_PARAMETER_REFERENCE(className);
			HACD_FORCE_PARAMETER_REFERENCE(fileName);
			HACD_FORCE_PARAMETER_REFERENCE(lineno);
			HACD_FORCE_PARAMETER_REFERENCE(classSize);
			return HACD_ALLOC(size);
		}

		inline void  operator delete(void* p,UserAllocated *t)
		{
			HACD_FORCE_PARAMETER_REFERENCE(p);
			HACD_FORCE_PARAMETER_REFERENCE(t);
			HACD_ALWAYS_ASSERT(); // should never be executed
		}

		inline void  operator delete(void* p)
		{
			HACD_FREE(p);
		}

		inline void  operator delete[](void* p)
		{
			HACD_FREE(p);
		}

		inline void  operator delete(void *p,const char *className,const char* fileName, int line,size_t classSize)
		{
			HACD_FORCE_PARAMETER_REFERENCE(className);
			HACD_FORCE_PARAMETER_REFERENCE(fileName);
			HACD_FORCE_PARAMETER_REFERENCE(line);
			HACD_FORCE_PARAMETER_REFERENCE(classSize);
			HACD_FREE(p);
		}

		inline void  operator delete[](void *p,const char *className,const char* fileName, int line,size_t classSize)
		{
			HACD_FORCE_PARAMETER_REFERENCE(className);
			HACD_FORCE_PARAMETER_REFERENCE(fileName);
			HACD_FORCE_PARAMETER_REFERENCE(line);
			HACD_FORCE_PARAMETER_REFERENCE(classSize);
			HACD_FREE(p);
		}

	};



#define HACD_NEW(T) new(#T,__FILE__,__LINE__,sizeof(T)) T

#ifdef HACD_WINDOWS
#define HACD_SPRINTF_S sprintf_s
#else
#define HACD_SPRINTF_S snprintf
#endif

#ifdef HACD_X64
typedef uint64_t HaSizeT;
#else
typedef uint32_t HaSizeT;
#endif


#ifdef HACD_WINDOWS
#define HACD_ISFINITE(x) _finite(x)
#else
#define HACD_ISFINITE(x) isfinite(x)
#endif

}; // end HACD namespace

#define UANS hacd	// the user allocator namespace

#define HACD_SQRT(x) float (sqrt(x))	
#define HACD_RECIP_SQRT(x) (float (1.0f) / dgSqrt(x))	

#include "PxVector.h"

#endif
