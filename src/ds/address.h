#pragma once
#include <cassert>
#include <cstdint>

#ifdef __CHERI_PURE_CAPABILITY__
#  include <cheri/cheric.h>
#endif

namespace snmalloc
{
  /**
   * The type used for an address.  Currently, all addresses are assumed to be
   * provenance-carrying values and so it is possible to cast back from the
   * result of arithmetic on an address_t.  Eventually, this will want to be
   * separated into two types, one for raw addresses and one for addresses that
   * can be cast back to pointers.
   */
  using address_t = uintptr_t;

  /**
   * Perform pointer arithmetic and return the adjusted pointer.
   */
  template<typename T>
  inline T* pointer_offset(T* base, size_t diff)
  {
    T* r = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(base) + diff);
#ifdef __CHERI_PURE_CAPABILITY__
    assert(cheri_gettag(base) == cheri_gettag(r));
#endif
    return r;
  }

  /**
   * Cast from a pointer type to an address.
   */
  template<typename T>
  inline address_t address_cast(T* ptr)
  {
    return reinterpret_cast<address_t>(ptr);
  }

  /**
   * Cast from an address back to a pointer of the specified type.  All uses of
   * this will eventually need auditing for CHERI compatibility.
   */
  template<typename T>
  inline T* pointer_cast(address_t address)
  {
    return reinterpret_cast<T*>(address);
  }

  template<size_t granule, typename T = void>
  inline T* pointer_align_down(void* p)
  {
    static_assert(granule > 0);
    static_assert((granule & (granule - 1)) == 0);
#if defined(__has_builtin) && __has_builtin(__builtin_align_down)
    return reinterpret_cast<T*>(__builtin_align_down(p, granule));
#else
    /* XREF bits::align_down; can't use here for cyclic deps */
    size_t align_1 = granule - 1;
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(p) & ~align_1);
#endif
  }

  template<size_t granule, typename T = void>
  inline T* pointer_align_up(void* p)
  {
    static_assert(granule > 0);
    static_assert((granule & (granule - 1)) == 0);
#if defined(__has_builtin) && __has_builtin(__builtin_align_up)
    return reinterpret_cast<T*>(__builtin_align_up(p, granule));
#else
    /* XREF bits::align_up; can't use here for cyclic deps */
    size_t align_1 = granule - 1;
    return reinterpret_cast<T*>(
      reinterpret_cast<uintptr_t>(pointer_offset(p, align_1)) & ~align_1);
#endif
  }

  template<typename T = void>
  inline T* pointer_align_up_dyn(void* p, size_t granule)
  {
    assert(granule > 0);
    assert((granule & (granule - 1)) == 0);
#if defined(__has_builtin) && __has_builtin(__builtin_align_up)
    return reinterpret_cast<T*>(__builtin_align_up(p, granule));
#else
    /* XREF bits::align_up; can't use here for cyclic deps */
    size_t align_1 = granule - 1;
    return reinterpret_cast<T*>(
      reinterpret_cast<uintptr_t>(pointer_offset(p, align_1)) & ~align_1);
#endif
  }

  inline size_t pointer_diff(void* base, void* cursor)
  {
    assert(cursor >= base);
    return static_cast<size_t>(
      static_cast<uint8_t*>(cursor) - static_cast<uint8_t*>(base));
  }

#if defined(__CHERI_PURE_CAPABILITY__)
  inline size_t pointer_diff_without_provenance(void* base, void* cursor)
  {
    vaddr_t vb = reinterpret_cast<vaddr_t>(base);
    vaddr_t vc = reinterpret_cast<vaddr_t>(cursor);
    assert(vc >= vb);
    return static_cast<size_t>(vc - vb);
  }
#endif

} // namespace snmalloc
