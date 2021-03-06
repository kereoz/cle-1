#include <stdio.h>
#include <stdlib.h>
#include <elf.h>
#include <errno.h>
#include <link.h>
#include <string.h>
#include "cle.h"
#include "libcle.h"
#include "libcle_static.h"

#if __ELF_NATIVE_CLASS == 32
    #define ELF32
#else
    #define ELF64
#endif

char *get_str(char*, int);


/* Display basic info contained in the ELF header*/
void print_basic_info(ElfW(Ehdr ehdr))
{
#ifdef ELF64
    printf("Entry point, 0x%lx\n", ehdr.e_entry);
#else
    printf("Entry point, 0x%x\n", ehdr.e_entry);
#endif
    printf("Machine type, %s\n", _get_arch(ehdr));
    printf("Object type, %s, 0x%x\n", _get_type(ehdr), ehdr.e_type);
    printf("Endianness, %s\n", ei_data_tostr(ehdr.e_ident[EI_DATA]));
    printf("Flags, 0x%x\n", ehdr.e_flags);
}


/* Print the program header table */
void print_phdr (ElfW(Phdr) *phdr, size_t size)
{
    int i;
    const char *name;

    if (!phdr || size == 0)
        return;

    printf("\nPHDR, OFFSET, VADDR, FILESZ, MEMSZ, ALIGN, TYPE\n---\n");
    for (i = 0; i < size; i++)
    {
        name = pt_type_tostr(phdr[i].p_type);
#ifdef ELF64
        printf("phdr, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, %s\n", 
                phdr[i].p_offset, phdr[i].p_vaddr, phdr[i].p_filesz,
                phdr[i].p_memsz, phdr[i].p_align, name); 
#else
        printf("phdr, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, %s\n", 
                phdr[i].p_offset, phdr[i].p_vaddr, phdr[i].p_filesz,
                phdr[i].p_memsz, phdr[i].p_align, name);
#endif
    }
}



/* Print the dynamic section */
void print_dynamic(ElfW(Dyn) *dynamic)
{
    int i;

    if(!dynamic)
        return;

    printf("\nDYN, TAG, PTR, VAL, TAG(str)\n---\n");
    //printf("\tINDEX \t TYPE \t\tVADDR \tVALUE \t\tSEGMENT \t\tTYPE(STRING)\n");
    for (i=0; dynamic[i].d_tag != DT_NULL; i++)
    {
#ifdef ELF64
        printf("dyn, 0x%lx, 0x%lx, 0x%lx, %s\n" , 
                dynamic[i].d_tag, dynamic[i].d_un.d_ptr, dynamic[i].d_un.d_val,
                d_tag_tostr(dynamic[i].d_tag));
#else
   printf("dyn, 0x%x, 0x%x, 0x%x, %s\n" , 
                dynamic[i].d_tag, dynamic[i].d_un.d_ptr, dynamic[i].d_un.d_val,
                d_tag_tostr(dynamic[i].d_tag));
#endif

    }
}


/* This function returns a pointer to the real address of the symbol table,
 * i.e., where we loaded it (using malloc). Do not confuse it with the virtual
 * address */
ElfW(Sym) *get_symtab_ptr(ElfW(Dyn) *dynamic, struct segment *s)
{

    ElfW(Addr) vaddr;
    ElfW(Off) offset;

    if (!dynamic || !s)
    {
        printf("ERR, null values for dynamic or segment\n");
        return NULL;
    }

    vaddr = _get_symtab_vaddr(dynamic);

    offset = addr_offset_from_segment(vaddr, s);
        if (offset == 0)
        {
            printf("ERR: NOSYMTAB\n");
            return NULL;
        }

    return (ElfW(Sym)*) (s->img + offset);
}


/* This function returns a pointer to the real address of the string table,
 * i.e., where we loaded it (using malloc). Do not confuse it with the virtual
 * address */
char* get_strtab_ptr(ElfW(Dyn) *dynamic, struct segment *s)
{

    ElfW(Addr) vaddr;
    ElfW(Word) offset;

    vaddr = _get_strtab_vaddr(dynamic);
    offset = addr_offset_from_segment(vaddr, s);
    if (offset == 0) // On error, the offset is 0
    {
        printf("ERR: NOSTRTAB\n");
        return NULL;
    }
    return (char*) (s->img + offset);
}



ElfW(Word) *get_hashtab_ptr(ElfW(Dyn) *dynamic, struct segment *s)
{
    ElfW(Addr) vaddr;
    ElfW(Word) offset;

    vaddr = get_dyn_ptr_addr(dynamic, DT_HASH);
    if (vaddr == (ElfW(Addr)) -ENODATA)
    {
        printf("DT_HASH not found :(\n");
        return NULL;
    }


    offset = addr_offset_from_segment(vaddr, s);
    if (offset == 0) // On error, the offset is 0
    {
        printf("ERR: NOHASHTAB\n");
        return NULL;
    }

    return (ElfW(Word)*) (s->img + offset);
}


ElfW(Rela) *get_rela_ptr(ElfW(Dyn) *dynamic, struct segment *s)
{

    ElfW(Addr) vaddr;
    ElfW(Word) offset;

    vaddr = get_dyn_ptr_addr(dynamic, DT_RELA);

    offset = addr_offset_from_segment(vaddr, s);
    if (offset == 0) // On error, the offset is 0
    {
        printf("ERR: NORELA\n");
        return NULL;
    }

    return (ElfW(Rela)*) (s->img + offset);
}


ElfW(Rel) *get_rel_ptr(ElfW(Dyn) *dynamic, struct segment *s)
{

    ElfW(Addr) vaddr;
    ElfW(Word) offset;

    vaddr = get_dyn_ptr_addr(dynamic, DT_REL);

    offset = addr_offset_from_segment(vaddr, s);
    if (offset == 0) // On error, the offset is 0
    {
        printf("ERR: NOREL\n");
        return NULL;
    }

    return (ElfW(Rel)*) (s->img + offset);
}


   /* Address of the relocation entries associated with the PLT */
ElfW(Addr) *get_jmprel_ptr(ElfW(Dyn) *dynamic, struct segment *s)
{
    ElfW(Addr) vaddr;

    ElfW(Word) offset;

    vaddr = get_dyn_ptr_addr(dynamic, DT_JMPREL);

    offset = addr_offset_from_segment(vaddr, s);
    if (offset == 0) // On error, the offset is 0
    {
        printf("ERR: NOJMPREL\n");
        return 0x0;
    }

    return (ElfW(Addr)*) (s->img + offset);
}


void print_jmprel(ElfW(Dyn) *dynamic, struct segment *s)
{
   ElfW(Addr) *addr;
   ElfW(Word) pltrelsz, relatype;
   int count, i; // symidx, stridx;
   ElfW(Rela) *rela;
   ElfW(Rel) *rel;
   //ElfW(Word) symtab_off;
   ElfW(Sym) *symtab;
   char *strtab;
   //unsigned char rtype;
   //char *name;

   if (!dynamic || !s)
       return;

   /* Size of relocation entries associated with the PLT */
   pltrelsz = get_dyn_val(dynamic, DT_PLTRELSZ);

   /* Type of relocation*/
   relatype = get_dyn_val(dynamic, DT_PLTREL);

   /* Address of the JMPREL table */
   addr = get_jmprel_ptr(dynamic, s);

   /* Symbol and string tables */
   symtab = get_symtab_ptr(dynamic, s);
   strtab = get_strtab_ptr(dynamic, s);

   if (!addr || !s)
       return;

   /* The following depends on the type of relocation entries */
   if (relatype == DT_RELA)
   {
       rela = (ElfW(Rela)*) addr;
       count = pltrelsz / sizeof(ElfW(Rela));

	   printf("\nJMPREL, OFFSET, NAME, TYPE, ADDEND\n---\n");
       for(i=0; i<count; i++)
		   print_rela_ent(rela[i], symtab, strtab, "jmprel");
   }

   else if (relatype == DT_REL)
   {
       rel = (ElfW(Rel)*) addr;
       count = pltrelsz / sizeof(ElfW(Rel));

	   printf("\nJMPREL, OFFSET, NAME, TYPE\n---\n");
       for(i=0; i<count; i++)
		   print_rel_ent(rel[i], symtab, strtab, "jmprel");
   }
}

void _print_reloc_rela(ElfW(Dyn) *dynamic, struct segment *s)
{
	char *strtab;
	ElfW(Sym) *symtab;
	ElfW(Word) relaent, relasz;
	ElfW(Rela) *rela = NULL;
	//char *name;
	//unsigned char rtype;
	int i, size;

	if (!dynamic || !s)
	{
		printf("No dynamic relocations for this binary\n");
		return;
	}

	printf("\nrela_type, DT_RELA\n");

	relaent = get_dyn_val(dynamic, DT_RELAENT);
	relasz = get_dyn_val(dynamic, DT_RELASZ);
	rela = get_rela_ptr(dynamic, s);
	size = relasz / relaent;


	if (!rela)
		return;

	symtab = get_symtab_ptr(dynamic, s);
	strtab = get_strtab_ptr(dynamic, s);

	printf ("\nRELOC, OFFSET, NAME, TYPE, ADDEND\n---\n");
	for(i=0 ;i<size; i++)
		print_rela_ent(rela[i], symtab, strtab, "reloc");
}

void _print_reloc_rel(ElfW(Dyn) *dynamic, struct segment *s)
{
	char *strtab;
    ElfW(Rel) *rel = NULL;
	ElfW(Sym) *symtab;
	ElfW(Word) relent, relsz;
	//unsigned char rtype;
	//char *name;
	int i, size;

	printf("\nrela_type, DT_REL\n");

	relent = get_dyn_val(dynamic, DT_RELENT);
	relsz = get_dyn_val(dynamic, DT_RELSZ);
	rel = get_rel_ptr(dynamic, s);
	size = relsz / relent;

	if (!rel)
		return;

	symtab = get_symtab_ptr(dynamic, s);
	strtab = get_strtab_ptr(dynamic, s);

	printf ("\n RELOC, OFFSET, NAME, TYPE\n---\n");

    /* Size of the table / size of individual entries */
    size = relsz / relent;

    /* Print either rel or rela, depending on whichever is set */
    for(i=0 ;i<size; i++)
		print_rel_ent(rel[i], symtab, strtab, "reloc");
}

/* Print relocation table */
void print_reloc(ElfW(Dyn) *dynamic, struct segment *s)
{

    ElfW(Word) pltrel, rel;

    if (!dynamic || !s)
        return;

	printf("\n---\nReloc stuff\n---\n");

    /* Relocation entries can be ElfW(Rela) of ElfW(Rel) structures*/
    pltrel = get_dyn_val(dynamic, DT_PLTREL);

    if (pltrel == DT_REL)
		return _print_reloc_rel(dynamic, s);
	else if (pltrel == DT_RELA)
		return _print_reloc_rela(dynamic,s);
	else
		/* We reach this point if we could not determine the type of reloc.
		 * This happens on MIPS */
	{
		if ((rel = get_dyn_val(dynamic, DT_REL)) != -ENODATA)
			return _print_reloc_rel(dynamic, s);

		else if ((rel = get_dyn_val(dynamic, DT_RELA)) != -ENODATA)
			return _print_reloc_rela(dynamic, s);

		else
			printf("ERR, no relocation information\n");
	}
}


void print_rpath(ElfW(Dyn) *dynamic, struct segment *s)
{
    int off;
    char *strtab;

    if (!dynamic || !s)
        return;

    off = (int)get_dyn_val(dynamic, DT_RPATH);

    strtab = get_strtab_ptr(dynamic, s);
    if (strtab)
        printf("rpath, %s\n", &strtab[off]);

}


/* In case there is no hash table in the binary, we have no way to know the
 * size of the symbol table. This is a simple heuristic to recover this value:
 * the st_name field must point to a valid entry in the string table */
int guess_symtab_sz(ElfW(Dyn) *dynamic, struct segment *s)
{
    ElfW(Sym) *symtab;
    size_t strsz;
    int i;
    //char *strtab;

    symtab = get_symtab_ptr(dynamic, s);
    strsz = _get_strtab_sz(dynamic);
    //strtab = get_strtab_ptr(dynamic, s);

    for (i=0; symtab[i].st_name < strsz; i++)
        continue;
    return i;
}


/* Print the string table */
void print_dyn_strtab(ElfW(Dyn) *dynamic, struct segment *s)
{
    char *strtab;
    size_t strsz;
    int i;
    int eos = 0;

    strtab = get_strtab_ptr(dynamic, s);
    strsz = _get_strtab_sz(dynamic);

	if(strsz <=0)
		return;

	printf("strtabsz, %zu\n", strsz);

    printf("\nSTRTAB, OFFSET, STR\n---");
    for(i=0; i<strsz; i++)
    {
        if (strtab[i] == '\0')
        {
            if (eos == 0)
            {
                eos=1;
                printf("\nstrtab, 0x%x, ", i);
            }
            else
                continue;
        }
        else
        {
            printf("%c", strtab[i]);
            eos=0;
        }
    }
	printf("\n");
}


/* Print the symbol table */
void print_symtab(ElfW(Dyn) *dynamic, struct segment *s)
{
    //size_t sz, strsz;
    int i;
    ElfW(Sym) *symtab;
    const char *type_s, *bind_s, *shn_type, *strtab, *name;
    ElfW(Word) *hash;
    int nchain;
    unsigned char type_v, bind_v;

    if (!s || !dynamic)
        return;

    symtab = get_symtab_ptr(dynamic, s);

    if (!symtab)
        return;

    //sz = get_symtab_syment(dynamic);
    strtab = get_strtab_ptr(dynamic, s);
    //strsz = _get_strtab_sz(dynamic);
    hash = get_hashtab_ptr(dynamic, s);

    /* The second value of the hash table is the size of the chain array, and
     * it has the same size as the symbol table according to the ELF
     * specification*/
    if (hash)
        nchain = hash[1];
    else
        nchain = guess_symtab_sz(dynamic, s);

    printf("\nSYMTAB, VALUE, SIZE, BIND, TYPE, SHTYPE, NAME\n---\n");
    for (i=0; i<nchain; i++) 
    {
        if (!addr_belongs_to_mem((unsigned long)&symtab[i], (unsigned
                        long)s->img, (unsigned long) s->memsz))
        {
            printf("Value out of segment: 0x%lx\n", (unsigned long)&symtab[i]);
            break;
        }

        /*
        else if (symtab[i].st_name == STN_UNDEF)
            continue;
            */

        /*
           else if (symtab[i].st_name > strsz) // outside of the string table
           break;
           */

        type_v = ST_TYPE(symtab[i].st_info);
        bind_v = ST_BIND(symtab[i].st_info);


        bind_s = symb_bind_tostr(bind_v);
        type_s = symb_type_tostr(type_v);

        shn_type = sh_index_tostr(symtab[i].st_shndx);

        name = &strtab[symtab[i].st_name];

        /*
        printf("symtab, %d, 0x%lx, %d, %d, %d, %d, %d, %s, %s, %s, %s\n",
                symtab[i].st_name, symtab[i].st_value, symtab[i].st_size,
                symtab[i].st_info, symtab[i].st_other,
                symtab[i].st_shndx, bind_s, type_s, shn_type, name);
                */
#ifdef ELF64
        printf("symtab, 0x%lx, 0x%lx, %s, %s, %s, %s\n", symtab[i].st_value,
                symtab[i].st_size, bind_s, type_s, shn_type, name);
#else
        printf("symtab, 0x%x, 0x%x, %s, %s, %s, %s\n", symtab[i].st_value,
                symtab[i].st_size, bind_s, type_s, shn_type, name);
#endif

    }
}


/* Print the names of the dependencies of the binary, i.e., which libraries it is
 * linked against */
void print_needed(ElfW(Dyn) *dynamic, struct segment *s)
{
    int i;
    char *strtab;

    if (!dynamic || !s)
        return;


    strtab = get_strtab_ptr(dynamic, s);
    if (!strtab){
        printf("ERR: NOSTRTAB");
        return;
    }

    printf("needed");
    for (i=0; dynamic[i].d_tag != DT_NULL; i++)
    {
        if (dynamic[i].d_tag == DT_NEEDED)
            printf(", %s", &strtab[dynamic[i].d_un.d_ptr]);
    }
    printf("\n");
}


void print_mips_reloc(ElfW(Dyn) *dynamic, struct segment *s)
{
    int i;

    if (!dynamic || !s)
        return;

    for (i=0; dynamic[i].d_tag != DT_NULL; i++)
    {
        if (dynamic[i].d_tag == DT_MIPS_GOTSYM)
#ifdef ELF64
            printf("MIPS first got entry in dynsym: 0x%lx\n",
                    dynamic[i].d_un.d_ptr);
#else
            printf("MIPS first got entry in dynsym: 0x%x\n",
                    dynamic[i].d_un.d_ptr);

#endif
    }

}


/* Just a stub to allocate and load segments into memory */
int load_stub(ElfW(Phdr) *phdr, int pt_index, struct segment *s, FILE *f)
{
    int ret;
    if((ret = alloc_segment(pt_index, phdr, s)) != 0)
        return ret;

    if ((ret = load_segment(s, f)) != 0)
        return ret;

    return 0;
}


/* Load the text segment */
int load_text(ElfW(Ehdr) ehdr, ElfW(Phdr) *phdr, struct segment *s, FILE *f)
{
    int pt_index;

    if((pt_index = find_text_index(ehdr, phdr)) < 0)
        return pt_index;

    return load_stub(phdr, pt_index, s, f);
    }


/* Load the data segment */
int load_data(ElfW(Ehdr) ehdr, ElfW(Phdr) *phdr, struct segment *s, FILE *f)
{
    int pt_index;

    if((pt_index = find_data_index(ehdr, phdr)) < 0)
        return pt_index;

    return load_stub(phdr, pt_index, s, f);
}

/* Output dynamic information */
void dynamic_info(ElfW(Dyn) *dynamic, struct segment *text)
{

	printf("Linking, dynamic\n");
	print_dynamic(dynamic);


	print_symtab(dynamic, text);
	print_dyn_strtab(dynamic, text);
	print_reloc(dynamic, text);
	print_jmprel(dynamic, text);

	printf("\nMISC\n---\n");
	print_needed(dynamic, text);
	print_rpath(dynamic, text);
	print_mips_reloc(dynamic,text);

	printf("gotaddr,0x%x\n", get_dyn_val(dynamic, DT_PLTGOT));

#ifdef ELF64
	printf("strtab_vaddr, 0x%lx\n", _get_strtab_vaddr(dynamic));
#else
	printf("strtab_vaddr, 0x%x\n", _get_strtab_vaddr(dynamic));
#endif

}

int main(int argc, char *argv[])
{
    ElfW(Ehdr) ehdr; // ELF header
    ElfW(Phdr) *phdr; // Program header table
    ElfW(Shdr) *shdr; // Section header
    ElfW(Dyn) *dynamic;
	ElfW(Sym) *sht_symtab = NULL;
    FILE *f;
    const char *binfile;
	char *sht_strtab = NULL; // Section header table's string table
    //char *filename;
    struct segment *data, *text;

    if (argc < 2)
    {
        printf("No filename given\n");
        exit(EXIT_FAILURE);
    }

    binfile = argv[1];

    f = fopen(binfile,"r");
    if (!f)
    {
        printf("Could not open file\n");
        exit(EXIT_FAILURE);
    }

    /* Determine the architecture type*/
    if ((get_elf_class(f) != ELFCLASS64) && (get_elf_class(f) != ELFCLASS32))
    {
        printf("Invalid ELF file\n");
        exit(EXIT_FAILURE);
    }

    /* Get ELF header*/
    rewind(f);
    fread(&ehdr, sizeof(ElfW(Ehdr)), 1, f);
    print_basic_info(ehdr);
    printf("Ehdr Flags: 0x%x\n", ehdr.e_flags);

    /* Get program header table*/
    phdr = get_phdr(ehdr, f);
    print_phdr(phdr, ehdr.e_phnum);

    
    dynamic = get_dynamic(phdr, ehdr.e_phnum, f);

	/* Load text and data segments*/
	data = malloc(sizeof(struct segment));
	text = malloc(sizeof(struct segment));

	if(!data || !text)
	{
		printf("\n***\nERR, cannot malloc()\n***\n");
		exit(EXIT_FAILURE);
	}

	if (load_text(ehdr, phdr, text, f) != 0)
		exit(EXIT_FAILURE);

	if (load_data(ehdr, phdr, data, f) != 0)
	{
		printf("\n***\nWARNING, no data segment\n***\n");
		data = NULL;
	}

	/* Statically linked binary */
	if (dynamic)
		dynamic_info(dynamic, text);
	else
		printf("Linking, static\n");

	/* Section info, if present */
	shdr = get_shdr(ehdr, f);
	if (shdr)
	{
		sht_strtab = alloc_load_sht_strtab(ehdr, shdr, f);
		print_shdr(shdr, ehdr.e_shnum, sht_strtab);
		print_static_strtabs(shdr, ehdr.e_shnum, text, data);

		sht_symtab = alloc_load_sht_symtab(shdr, ehdr.e_shnum, f);
		print_static_symtab(shdr, ehdr.e_shnum, sht_symtab, f);
	}

	if (data)
		free_segment(&data);
	free_segment(&text);

    fclose(f);
    free(phdr);
    free(shdr);
	if (sht_strtab)
		free(sht_strtab);
	if (sht_symtab)
		free(sht_symtab);
    
    return 0;
}
