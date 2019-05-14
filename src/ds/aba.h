#pragma once

#include "bits.h"

namespace snmalloc
{
  template<typename T, Construction c = RequiresInit>
  class ABA
  {
  public:
#ifdef PLATFORM_IS_X86
    struct alignas(2 * sizeof(std::size_t)) Linked
    {
      T* ptr;
      uintptr_t aba;
    };

    struct Independent
    {
      std::atomic<T*> ptr;
      std::atomic<uintptr_t> aba;
    };

    static_assert(
      sizeof(Linked) == sizeof(Independent),
      "Expecting identical struct sizes in union");
    static_assert(
      sizeof(Linked) == (2 * sizeof(std::size_t)),
      "Expecting ABA to be the size of two pointers");

    using Cmp = Linked;
#elif defined(__mips__) && defined(__CHERI_PURE_CAPABILITY__)
    using Cmp = T*;
#else
    using Cmp = T*;
#endif

  private:
#ifdef PLATFORM_IS_X86
    union
    {
      alignas(2 * sizeof(std::size_t)) std::atomic<Linked> linked;
      Independent independent;
    };
#elif defined(__mips__) && defined(__CHERI_PURE_CAPABILITY__)
    T* raw;
#else
    std::atomic<T*> raw;
#endif

  public:
    ABA()
    {
      if constexpr (c == RequiresInit)
        init(nullptr);
    }

    void init(T* x)
    {
#ifdef PLATFORM_IS_X86
      independent.ptr.store(x, std::memory_order_relaxed);
      independent.aba.store(0, std::memory_order_relaxed);
#elif defined(__mips__) && defined(__CHERI_PURE_CAPABILITY__)
      raw = x;
#else
      raw.store(x, std::memory_order_relaxed);
#endif
    }

    T* peek()
    {
      return
#ifdef PLATFORM_IS_X86
        independent.ptr.load(std::memory_order_relaxed);
#elif defined(__mips__) && defined(__CHERI_PURE_CAPABILITY__)
        raw;
#else
        raw.load(std::memory_order_relaxed);
#endif
    }

    Cmp read()
    {
#ifdef PLATFORM_IS_X86
      return Cmp{independent.ptr.load(std::memory_order_relaxed),
                 independent.aba.load(std::memory_order_relaxed)};
#elif defined(__mips__) && defined(__CHERI_PURE_CAPABILITY__)
      T* tp;
      asm("cllc %[tp], %[raw]\n"
          : [tp] "=C"(tp)
          : [raw] "C"(&this->raw)
          : "memory");
      return tp;
#else
      return raw.load(std::memory_order_relaxed);
#endif
    }

    static T* ptr(Cmp& from)
    {
#ifdef PLATFORM_IS_X86
      return from.ptr;
#else
      return from;
#endif
    }

    bool compare_exchange(Cmp& expect, T* value)
    {
#ifdef PLATFORM_IS_X86
#  if defined(_MSC_VER) && defined(SNMALLOC_VA_BITS_64)
      return _InterlockedCompareExchange128(
        (volatile __int64*)&linked,
        expect.aba + 1,
        (__int64)value,
        (__int64*)&expect);
#  else
#    if defined(__GNUC__) && !defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16)
#error You must compile with -mcx16 to enable 16-byte atomic compare and swap.
#    endif
      Cmp xchg{value, expect.aba + 1};

      return linked.compare_exchange_weak(
        expect, xchg, std::memory_order_relaxed, std::memory_order_relaxed);
#  endif
#elif defined(__mips__) && defined(__CHERI_PURE_CAPABILITY__)
      int res;
      T* tv = this->raw;
      T* e = ptr(expect);
      asm(
        "cexeq %[res], %[tv], %[expect]\n\t"
        "beqz %[res], 1f\n\t"
        "nop\n\t"
        "cscc %[res], %[value], %[raw]\n\t"
        "1:\n"
        : [res] "=r"(res)
        :
        [raw] "C"(&this->raw), [tv] "C"(tv), [expect] "C"(e), [value] "C"(value)
        : "memory");
      if (!res)
      {
        expect = read();
      }
      return static_cast<bool>(res);
#else
      return raw.compare_exchange_weak(
        expect, value, std::memory_order_relaxed, std::memory_order_relaxed);
#endif
    }
  };
} // namespace snmalloc
