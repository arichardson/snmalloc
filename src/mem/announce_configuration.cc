#include "allocconfig.h"
extern "C" {
  #include <string.h>
  #include <unistd.h>
}

namespace snmalloc {
  static void __attribute__((constructor)) snmalloc_announce_configuration(void) {
    const char * verdesc = "snmalloc "
#define VERDESC_STR(x) #x
#define VERDESC_XSTR(x) VERDESC_STR(x)
#if (SNMALLOC_PAGEMAP_REDERIVE == 1)
        "pm+rederive " 
#elif (SNMALLOC_PAGEMAP_POINTERS == 1)
        "pm+pointers " 
#endif
#if (SNMALLOC_CHERI_SETBOUNDS == 1)
        "cheri+bounds "
#elif (SNMALLOC_CHERI_ALIGN == 1)
        "cheri+align "
#endif
#if (SNMALLOC_QUARANTINE_DEALLOC == 1)
        "quar"
        "+paat=" VERDESC_XSTR(SNMALLOC_QUARANTINE_PER_ALLOC_THRESHOLD)
#  ifdef SNMALLOC_QUARANTINE_PER_ALLOC_CHUNK_THRESHOLD
        "+pact=" VERDESC_XSTR(SNMALLOC_QUARANTINE_PER_ALLOC_CHUNK_THRESHOLD)
#  endif
        "+sc=" VERDESC_XSTR(SNMALLOC_QUARANTINE_CHUNK_SIZECLASS)
        "+str=" VERDESC_XSTR(SNMALLOC_QUARANTINE_POLICY_STRICT)
        " "
#endif
#if (SNMALLOC_REVOKE_QUARANTINE == 1)
        "rev"
#  if (SNMALLOC_REVOKE_DRY_RUN == 1)
        "+dry"
#  endif
        " "
#endif
#if (SNMALLOC_UNSAFE_FREES == 1)
        "unsf"
#  if (SNMALLOC_UNSAFE_FREES_CHECK == 1)
        "+check"
#  endif
#endif
    "\n";

    write(2, verdesc, strlen(verdesc));
  }
};
