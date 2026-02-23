/* xpcom/xpcom-config.h.  Generated automatically by configure.  */
/* Global defines needed by xpcom clients */

#ifndef _XPCOM_CONFIG_H_
#define _XPCOM_CONFIG_H_

/* Define this to throw() if the compiler complains about 
 * constructors returning NULL
 */
#define CPP_THROW_NEW throw()

/* Define if the c++ compiler supports a 2-byte wchar_t */
/* #undef HAVE_CPP_2BYTE_WCHAR_T */

/* Define if the c++ compiler supports changing access with |using| */
/* #undef HAVE_CPP_ACCESS_CHANGING_USING */

/* Define if the c++ compiler can resolve ambiguity with |using| */
/* #undef HAVE_CPP_AMBIGUITY_RESOLVING_USING */

/* Define if the c++ compiler has builtin Bool type */
#if defined(__cplusplus)
#  ifndef HAVE_CPP_BOOL
#    define HAVE_CPP_BOOL 1
#  endif
#endif

/* Define if a dyanmic_cast to void* gives the most derived object */
/* #undef HAVE_CPP_DYNAMIC_CAST_TO_VOID_PTR */

/* Define if the c++ compiler supports the |explicit| keyword */
#if defined(__cplusplus)
#  ifndef HAVE_CPP_EXPLICIT
#    define HAVE_CPP_EXPLICIT 1
#  endif
#endif

/* Define if the c++ compiler supports the modern template
 * specialization syntax
 */
#if defined(__cplusplus)
#  ifndef HAVE_CPP_MODERN_SPECIALIZE_TEMPLATE_SYNTAX
#    define HAVE_CPP_MODERN_SPECIALIZE_TEMPLATE_SYNTAX 1
#  endif
#endif

/* Define if the c++ compiler supports the |std| namespace */
#if defined(__cplusplus)
#  ifndef HAVE_CPP_NAMESPACE_STD
#    define HAVE_CPP_NAMESPACE_STD 1
#  endif
#endif

/* Define if the c++ compiler supports reinterpret_cast */
#if defined(__cplusplus)
#  ifndef HAVE_CPP_NEW_CASTS
#    define HAVE_CPP_NEW_CASTS 1
#  endif
#endif

/* Define if the c++ compiler supports partial template specialization */
/* #undef HAVE_CPP_PARTIAL_SPECIALIZATION */

/* Define if the c++ compiler has trouble comparing a constant
 * reference to a templatized class to zero
 */
/* #undef HAVE_CPP_TROUBLE_COMPARING_TO_ZERO */

/* Define if the c++ compiler supports the |typename| keyword */
#if defined(__cplusplus)
#  ifndef HAVE_CPP_TYPENAME
#    define HAVE_CPP_TYPENAME 1
#  endif
#endif

/* Define if the stanard template operator!=() is ambiguous */
#if defined(__cplusplus)
#  ifndef HAVE_CPP_UNAMBIGUOUS_STD_NOTEQUAL
#    define HAVE_CPP_UNAMBIGUOUS_STD_NOTEQUAL 1
#  endif
#endif

/* Define if statvfs() is available */
/* #undef HAVE_STATVFS */

/* Define if the c++ compiler requires implementations of 
 * unused virtual methods
 */
/* #undef NEED_CPP_UNUSED_IMPLEMENTATIONS */

/* Define to either <new> or <new.h> */
#define NEW_H <new>

/* Define if binary compatibility with Mozilla 1.x string code is desired */
#define MOZ_V1_STRING_ABI 1

#endif /* _XPCOM_CONFIG_H_ */
