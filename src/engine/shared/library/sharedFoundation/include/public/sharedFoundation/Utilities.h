#ifndef INCLUDED_SharedFoundation_Utilities_H
#define INCLUDED_SharedFoundation_Utilities_H

// ============================================================================
//
// Utilities.h
// Copyright (c) 2024
//
// A lightweight collection of helper routines that were historically provided
// by the legacy Utilities header.  Some parts of the codebase – in particular
// the Windows specific build of the sharedFile library – still include the
// original header even though only a handful of small helpers are required.
//
// This replacement focuses on the operations that the old header was used for
// inside TreeFileEncryption.cpp: byte order conversions and bit rotation.
// Keeping these helpers here avoids sprinkling ad-hoc implementations through
// the rest of the codebase and restores compatibility with the existing
// include.
//
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace Utilities
{
        // ------------------------------------------------------------------
        //  Helper traits
        // ------------------------------------------------------------------

        template <typename T>
        struct is_uint_like : std::integral_constant<bool,
                std::is_integral<T>::value && !std::is_signed<T>::value>
        {
        };

        // ------------------------------------------------------------------
        //  Memory helpers
        // ------------------------------------------------------------------

        template <typename T>
        inline void zero(T &value)
        {
                std::memset(&value, 0, sizeof(T));
        }

        inline void zero(void *destination, std::size_t size)
        {
                std::memset(destination, 0, size);
        }

        template <typename Destination, typename Source>
        inline void copy(Destination &destination, Source const &source)
        {
                static_assert(sizeof(Destination) == sizeof(Source),
                        "Utilities::copy requires objects of identical size");
                std::memcpy(&destination, &source, sizeof(Destination));
        }

        inline void copy(void *destination, void const *source, std::size_t size)
        {
                std::memcpy(destination, source, size);
        }

        // ------------------------------------------------------------------
        //  Generic helpers
        // ------------------------------------------------------------------

        template <typename T>
        inline constexpr T clamp(T value, T minimum, T maximum)
        {
                return value < minimum ? minimum : (value > maximum ? maximum : value);
        }

        template <typename T, std::size_t N>
        inline constexpr std::size_t arrayLength(T const (&)[N])
        {
                return N;
        }

        // ------------------------------------------------------------------
        //  Bit rotation helpers
        // ------------------------------------------------------------------

        template <typename T>
        inline typename std::enable_if<is_uint_like<T>::value, T>::type
        rotateLeft(T value, unsigned int amount)
        {
                amount &= (sizeof(T) * 8u - 1u);
                return static_cast<T>((value << amount) | (value >> ((sizeof(T) * 8u) - amount)));
        }

        template <typename T>
        inline typename std::enable_if<is_uint_like<T>::value, T>::type
        rotateRight(T value, unsigned int amount)
        {
                amount &= (sizeof(T) * 8u - 1u);
                return static_cast<T>((value >> amount) | (value << ((sizeof(T) * 8u) - amount)));
        }

        // ------------------------------------------------------------------
        //  Endian helpers
        // ------------------------------------------------------------------

        template <typename T>
        inline typename std::enable_if<is_uint_like<T>::value, T>::type
        swapEndian(T value)
        {
                T result = 0;
                for (std::size_t i = 0; i < sizeof(T); ++i)
                {
                        result |= ((value >> (i * 8u)) & static_cast<T>(0xff))
                                << ((sizeof(T) - 1u - i) * 8u);
                }
                return result;
        }

        template <typename T>
        inline typename std::enable_if<is_uint_like<T>::value, T>::type
        fromLittleEndian(void const *data)
        {
                T result = 0;
                auto const *bytes = static_cast<unsigned char const *>(data);
                for (std::size_t i = 0; i < sizeof(T); ++i)
                {
                        result |= static_cast<T>(bytes[i]) << (i * 8u);
                }
                return result;
        }

        template <typename T>
        inline typename std::enable_if<is_uint_like<T>::value, void>::type
        toLittleEndian(T value, void *data)
        {
                auto *bytes = static_cast<unsigned char *>(data);
                for (std::size_t i = 0; i < sizeof(T); ++i)
                {
                        bytes[i] = static_cast<unsigned char>((value >> (i * 8u)) & 0xffu);
                }
        }

        template <typename T>
        inline typename std::enable_if<is_uint_like<T>::value, T>::type
        fromBigEndian(void const *data)
        {
                T result = 0;
                auto const *bytes = static_cast<unsigned char const *>(data);
                for (std::size_t i = 0; i < sizeof(T); ++i)
                {
                        result |= static_cast<T>(bytes[i]) << ((sizeof(T) - 1u - i) * 8u);
                }
                return result;
        }

        template <typename T>
        inline typename std::enable_if<is_uint_like<T>::value, void>::type
        toBigEndian(T value, void *data)
        {
                auto *bytes = static_cast<unsigned char *>(data);
                for (std::size_t i = 0; i < sizeof(T); ++i)
                {
                        bytes[i] = static_cast<unsigned char>((value >> ((sizeof(T) - 1u - i) * 8u)) & 0xffu);
                }
        }
}

#endif // INCLUDED_SharedFoundation_Utilities_H
