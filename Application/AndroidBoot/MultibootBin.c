#define INCBIN(symname, sizename, filename, section)					\
	__asm__ (".section " section "; .align 4; .globl "#symname);		\
	__asm__ (""#symname ":\n.incbin \"" filename "\"");					\
	__asm__ (".section " section "; .align 1;");						\
	__asm__ (""#symname "_end:");										\
	__asm__ (".section " section "; .align 4; .globl "#sizename);		\
	__asm__ (""#sizename ": .long "#symname "_end - "#symname " - 1");	\
	extern unsigned char symname[];										\
	extern unsigned int sizename

#define INCFILE(symname, sizename, filename) INCBIN(symname, sizename, filename, ".rodata")

INCFILE(MultibootBin, MultibootSize, EFIDROID_MULTIBOOT_BINARY);
