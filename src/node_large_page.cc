#include <errno.h>
#include <fcntl.h>  // _O_RDWR
#include <limits.h>  // PATH_MAX
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <string>
#include <fstream>
#include <iostream>
 
/*
The functions in this file map the text segment of node into 2M pages.
The algorithm is quite simple
   1. Find the text region of node in memory 
   2. Move the text region to large pages
*/

extern char __executable_start;
extern char __etext;
extern char __nodetext;

namespace node {

  namespace largepages {
 
#define ALIGN(x, a)     (((x) + (a) - 1) & ~((a) - 1))
#define PAGE_ALIGN_UP(x,a)   ALIGN(x,a)
#define PAGE_ALIGN_DOWN(x,a) ((x) & ~((a) - 1))
  
    struct TextRegion {
      void * from;
      void * to;
      long offset;
      char name[PATH_MAX];
    };


    /*
      Finding the text region.
      1. We read the maps file and find the start and end addresss of the loaded node process
      2. Within that start and end address is the .text region is what we are interested in.
      3. We modify the linker script to PROVIDE(__nodetext) which points to this region.
      4. _etext is the end of the .text segment.
      5. We return back a struct of the TextRegion

      The /proc/<pid>/maps looks like this. The first entry is executable of the node process and it's address is from 00400000-020c7000
      00400000-020c7000 r-xp 00000000 08:01 538609 /home/ssuresh/node/out/Release/node
      022c6000-022c7000 r--p 01cc6000 08:01 538609 /home/ssuresh/node/out/Release/node
      022c7000-022e1000 rw-p 01cc7000 08:01 538609 /home/ssuresh/node/out/Release/node
      022e1000-02412000 rw-p 00000000 00:00 0      [heap]

      If we look at the elf header (info file in gdb we see the .text.In the custom linker script we provide an entry to that.

      `/home/ssuresh/node/out/Release/node', file type elf64-x86-64.
      Entry point: 0x847e60
      0x0000000000400270 - 0x000000000040028c is .interp
      0x000000000040028c - 0x00000000004002ac is .note.ABI-tag
      0x00000000004002ac - 0x00000000004002d0 is .note.gnu.build-id
      0x00000000004002d0 - 0x0000000000453ce0 is .gnu.hash
      0x0000000000453ce0 - 0x000000000055be18 is .dynsym
      0x000000000055be18 - 0x0000000000829208 is .dynstr
      0x0000000000829208 - 0x000000000083f222 is .gnu.version
      0x000000000083f228 - 0x000000000083f438 is .gnu.version_r
      0x000000000083f438 - 0x000000000083f660 is .rela.dyn
      0x000000000083f660 - 0x0000000000841d30 is .rela.plt
      0x0000000000841d30 - 0x0000000000841d4f is .init
      0x0000000000841d50 - 0x0000000000843740 is .plt
      0x0000000000843740 - 0x0000000000843748 is .plt.got
      0x0000000000843800 - 0x0000000001680519 is .text
      0x000000000168051c - 0x0000000001680525 is .fini
      0x0000000001681000 - 0x0000000001ed0778 is .rodata
      0x0000000001ed0778 - 0x0000000001ed0780 is .eh_frame_hdr
      0x0000000002000000 - 0x00000000023a97c4 is .eh_frame
      0x00000000025a9d50 - 0x00000000025a9d54 is .tbss
      0x00000000025a9d50 - 0x00000000025a9da0 is .init_array
      0x00000000025a9da0 - 0x00000000025a9db8 is .fini_array
      0x00000000025a9db8 - 0x00000000025a9dc0 is .jcr
      0x00000000025a9dc0 - 0x00000000025a9ff0 is .dynamic
      0x00000000025a9ff0 - 0x00000000025aa000 is .got
      0x00000000025aa000 - 0x00000000025aad08 is .got.plt
      0x00000000025aad20 - 0x00000000025c39f0 is .data
      0x00000000025c3a00 - 0x00000000025dbb28 is .bss

    */
  
    static struct TextRegion find_node_text_region() {
      FILE *f;
      long unsigned int  start, end, offset, inode;
      char perm[5], dev[6], name[256];
      int ret;
      const size_t hugePageSize = 2L * 1024 * 1024;
      struct TextRegion nregion;


      f = fopen("/proc/self/maps", "r");

      ret = fscanf(f, "%lx-%lx %4s %lx %5s %ld %s\n", &start, &end, perm, &offset, dev, &inode, name);
      if (ret == 7 && perm[0] == 'r' && perm[1] == '-' && perm[2] == 'x'
	  && name[0] == '/' && strstr(name, "node") != NULL) {
	// Checking if the region is from node binary and executable
	//  00400000-020c7000 r-xp 00000000 08:01 538609  /home/ssuresh/node/out/Release/node

	// Need to align the from and to to the 2M Boundary 
	//	fprintf(stderr,"exe start %p exe end %p \n", &__executable_start, &__etext);
    
	//	fprintf(stderr, "find_node_text_region %lx-%lx %s\n", start, end, name);
	//    start = (unsigned int long) &__executable_start;
	// end = (unsigned int long) &__etext;
	//    start = 0x0842700;
	//    end =   0x1679e19;
	start = (unsigned int long) &__nodetext;
	end = (unsigned int long) &__etext;
	char *from = (char *)PAGE_ALIGN_UP(start, hugePageSize);
	char *to = (char *)PAGE_ALIGN_DOWN(end, hugePageSize);
	//	fprintf(stderr, "find_node_text_region %lx-%lx %s\n", from, to, name);
	nregion.from = from;
	nregion.to = to;
	nregion.offset = offset;
	strcpy(nregion.name,name);
	if (from > to) {
	  // Handle Error
	  return nregion;
	}
	return nregion;
      }
      else {
	// Handle Error
	return nregion;
      }
  
    }

    /*

      Moving the text region to large pages. We need to be very careful. 
      a) This function itself should not be moved. 
      We use a gcc option to put it outside the .text area
      b) This function should not call any functions that might be moved.

      1. We map a new area and copy the original code there
      2. We mmap using HUGE_TLB 
      3. If we are successful we copy the code there and unmap the original region.

    */
  
    void
    __attribute__((__section__(".eh_frame"))) 
    __attribute__((__aligned__(2 * 1024 * 1024))) 
    __attribute__((__noinline__))
    __attribute__((__optimize__("2")))
    move_text_region_to_large_pages(struct TextRegion r) {
      size_t size = (intptr_t)r.to - (intptr_t)r.from;
      void *nmem, *ret;
      void *start = r.from;
      const size_t hugePageSize = 2L * 1024 * 1024;
      const size_t fourkPageSize = 4L * 1024;

      //      fprintf(stderr,"exe start %p exe end %p %p \n", &__executable_start, &__etext, &move_text_region_to_large_pages);

      //      fprintf(stderr, "move_text_region_to_large_pages %p %p %d %d\n", start, r.to, size/hugePageSize, size/fourkPageSize);
      //  nmem = malloc(size);
      nmem = mmap(NULL, size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,-1, 0);
      memcpy(nmem, r.from, size);

      //  if (nmem == MAP_FAILED) {
      //    fprintf(stderr, "mmap failed\n");
      //  }
      //  memcpy(nmem, r.from, size);
      // fprintf(stderr, "move_text_region_to_large_pages %p %lx %p\n", start, size, nmem);

#if 0 // use for explicit huge pages
      mmap(start, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_HUGETLB, -1 , 0);
#endif
#if 1 // use for transparent huge pages
      mmap(start, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1 , 0);
      madvise(start, size, MADV_HUGEPAGE);
#endif
      memcpy(start, nmem, size);
      mprotect(start, size, PROT_READ | PROT_EXEC);
      munmap(nmem, size);
      //  free(nmem);
  
    }
    bool transHugePagesPresent=false;
    bool explicitHugePagesPresent = false;
    /*

      You’ll see a list of all possible options ( always, madvise, never ), with 
      the currently active option being enclosed in brackets.madvise is the default.
      This means transparent hugepages are only enabled for memory regions that 
      explicitly request hugepages using madvise(2).

      Applications that gets a lot of benefit from hugepages and that don't
      risk to lose memory by using hugepages, should use
      madvise(MADV_HUGEPAGE) on their critical mmapped regions.
     */
    static bool isTransparentHugePagesEnabled() {

       std::ifstream ifs;
       ifs.open("/sys/kernel/mm/transparent_hugepage/enabled");
       std::string always, madvise, never;
        if (ifs.is_open()) {
	  while(ifs >> always >> madvise >> never) ;
	    //	    std::cout << always << madvise << never;
        }
       if (always.compare("[always]") == 0)
	 return true;
       if (madvise.compare("[madvise]") == 0)
	 return true;

       return false;
    }
    static bool isExplicitHugePagesEnabled() {
     std::string kw;
     std::ifstream file("/proc/meminfo");
     while(file >> kw) {
        if(kw == "HugePages_Total:") {
            unsigned long hp_tot;
	    file >> hp_tot;
	    if (hp_tot > 0)
	      return true;
	    else
	      return false;
        }
     }
     return false; // HugePages not found
    }

    static int howManyExplicitHugePagesFree() {
     std::string kw;
     std::ifstream file("/proc/meminfo");
     while(file >> kw) {
       if(kw == "HugePages_Free:") {
            unsigned long hp_free;
	    file >> hp_free;
	    return hp_free;
        }
     }
     return 0; // HugePages not found
      
    }
    
    /* This is the primary interface that is exposed */

    bool isLargePagesEnabled() {
      bool trans = isTransparentHugePagesEnabled();
      //      fprintf(stderr, "Transparent Huge Pages = %s\n", trans ? "enabled" : "disabled");
      bool explict = isExplicitHugePagesEnabled();
      //      fprintf(stderr, "Explicit Huge Pages = %s\n", explict ? "enabled" : "disabled");
      
      return isExplicitHugePagesEnabled() || isTransparentHugePagesEnabled();
    }

    void map_static_code_to_large_pages() {
      struct TextRegion n;
      //      fprintf(stderr, "mapping static code to large pages\n");
      // starting and ending address of the region in the node process
      n = find_node_text_region();
      //      fprintf(stderr, "n.from=%p n.to=%p n.name = %s map_static_code_to_large_pages = %p\n", n.from, n.to, n.name, &move_text_region_to_large_pages);
      if (n.to <= (void *) & move_text_region_to_large_pages)
	move_text_region_to_large_pages(n);
  
    }
  }
}
