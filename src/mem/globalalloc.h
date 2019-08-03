#pragma once

#include "../ds/helpers.h"
#include "alloc.h"
#include "pool.h"

namespace snmalloc
{
  inline void* lazy_replacement(void*);
  using Alloc =
    Allocator<GlobalVirtual, SNMALLOC_DEFAULT_PAGEMAP, true, lazy_replacement>;

  template<class MemoryProvider>
  class AllocPool : Pool<
                      Allocator<
                        MemoryProvider,
                        SNMALLOC_DEFAULT_PAGEMAP,
                        true,
                        lazy_replacement>,
                      MemoryProvider>
  {
    using Alloc = Allocator<
      MemoryProvider,
      SNMALLOC_DEFAULT_PAGEMAP,
      true,
      lazy_replacement>;
    using Parent = Pool<Alloc, MemoryProvider>;

  public:
    static AllocPool* make(MemoryProvider& mp)
    {
      static_assert(
        sizeof(AllocPool) == sizeof(Parent),
        "You cannot add fields to this class.");
      // This cast is safe due to the static assert.
      return static_cast<AllocPool*>(Parent::make(mp));
    }

    static AllocPool* make() noexcept
    {
      return make(default_memory_provider);
    }

    Alloc* acquire()
    {
      return Parent::acquire(Parent::memory_provider);
    }

    void release(Alloc* a)
    {
      Parent::release(a);
    }

  public:
    void aggregate_stats(Stats& stats)
    {
      auto* alloc = Parent::iterate();

      while (alloc != nullptr)
      {
        stats.add(alloc->stats());
        alloc = Parent::iterate(alloc);
      }
    }

#ifdef USE_SNMALLOC_STATS
    void print_all_stats(std::ostream& o, uint64_t dumpid = 0)
    {
      auto alloc = Parent::iterate();

      while (alloc != nullptr)
      {
        alloc->stats().template print<Alloc>(o, dumpid, alloc->id());
        alloc = Parent::iterate(alloc);
      }
    }
#else
    void print_all_stats(void*& o, uint64_t dumpid = 0)
    {
      UNUSED(o);
      UNUSED(dumpid);
    }
#endif

    void cleanup_unused()
    {
#ifndef USE_MALLOC
      // Call this periodically to free and coalesce memory allocated by
      // allocators that are not currently in use by any thread.
      // One atomic operation to extract the stack, another to restore it.
      // Handling the message queue for each stack is non-atomic.
      auto* first = Parent::extract();
      auto* alloc = first;
      decltype(alloc) last;

      if (alloc != nullptr)
      {
        while (alloc != nullptr)
        {
          alloc->handle_message_queue();
          last = alloc;
          alloc = Parent::extract(alloc);
        }

        restore(first, last);
      }
#endif
    }

    void debug_check_empty()
    {
#ifndef USE_MALLOC
      // This is a debugging function. It checks that all memory from all
      // allocators has been freed.
      size_t alloc_count = 0;
      size_t empty_count;

      auto* alloc = Parent::iterate();

      // Count the linked allocators.
      while (alloc != nullptr)
      {
        alloc = Parent::iterate(alloc);
        alloc_count++;
      }

      bool done = false;

      while (!done)
      {
        empty_count = 0;
        done = true;
        alloc = Parent::iterate();

        while (alloc != nullptr)
        {
          // Destroy the message queue so that it has no stub message.
          //
          // Even if we have SNMALLOC_QUARANTINE_DEALLOC on, these will
          // be immediately returned to circulation
          Remote* p = alloc->message_queue().destroy();

          while (p != nullptr)
          {
            Remote* next = p->non_atomic_next;
            alloc->handle_dealloc_remote(p);
            p = next;
          }

#  if SNMALLOC_QUARANTINE_DEALLOC == 1
          /*
           * Force the allocator to consider its quarantine released
           * and to free its pending quarantine metadata node(s).  We'll
           * reinitialize the quarantine in just a moment!  It's essential
           * that no frees happen between here and there, and that means,
           * among other things, that C++isms should be avoided including
           * stats printout (which uses heap-allocated C++ strings).
           */
          alloc->quarantine.debug_drain_all(alloc, true);
#  endif

          if (alloc->stats().is_empty())
            empty_count++;

#  if SNMALLOC_QUARANTINE_DEALLOC == 1
          alloc->quarantine.init(alloc);
#  endif

          // Place the stub message on the queue.
          alloc->init_message_queue();

          // Post all remotes, including forwarded ones. If any allocator posts,
          // repeat the loop.
          if (alloc->remote.size > 0)
          {
            alloc->stats().remote_post();
            alloc->remote.post(alloc->id());
            done = false;
          }

          alloc = Parent::iterate(alloc);
        }
      }

      if (alloc_count != empty_count)
        error("Incorrect number of allocators");
#endif
    }
  };

  inline AllocPool<GlobalVirtual>*& current_alloc_pool()
  {
    return Singleton<
      AllocPool<GlobalVirtual>*,
      AllocPool<GlobalVirtual>::make>::get();
  }

  template<class MemoryProvider>
  inline AllocPool<MemoryProvider>* make_alloc_pool(MemoryProvider& mp)
  {
    return AllocPool<MemoryProvider>::make(mp);
  }

} // namespace snmalloc
