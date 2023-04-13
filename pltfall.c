#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <elf.h>


enum methods {
	RELA_OFFSET = 1,
	RELA_INFO = 2,
	PLT_PUSH = 4,
	PLT_GOT = 8,
	GOT = 16,
};

struct func
{
	char *target_name;
	char *source_name;
	int method;
	struct func *next;
};

void free_func_list(struct func *func_list)
{
	struct func *current_func = func_list;
	while (current_func) {
		struct func *next = current_func->next;
		free(current_func->target_name);
		free(current_func->source_name);
		free(current_func);
		current_func = next;
	}

}

struct func *load_func_list(char *path, int *func_count)
{
	FILE *file = fopen(path, "r");
	if (!file) {
		fprintf(stderr, "error: failed to open the file: %s\n", path);
		return 0;
	}

	struct func *func_list = 0;
	struct func *current_func = 0;
	char *line = 0;
	size_t count = 0;
	size_t length = 0;
	int line_nr = 1;
	while ((count = getline(&line, &length, file)) != -1) {
		strtok(line, "\n");

		struct func f = {0};
		int token_count = 0;
		char *token = strtok(line, ",");
		int swap = 0;
		while (token) {
			switch (token_count) {
				case 0: {
					f.target_name = strdup(token);
					break;
				}
				case 1: {
					f.source_name = strdup(token);
					break;
				}
				case 2: {
					if (!strcmp(token, "copy")) {
						swap = 0;
					} else if (!strcmp(token, "swap")) {
						swap = 1;
					} else {
						free(f.target_name);
						free(f.source_name);
						free_func_list(func_list);
						fprintf(stderr, "error: unknown type at line %d: %s\n", line_nr, token);
						return 0;
					}
					break;
				}
				default: {
					if (!strcmp(token, "rela-offset")) {
						f.method |= RELA_OFFSET;
					} else if (!strcmp(token, "rela-info")) {
						f.method |= RELA_INFO;
					} else if (!strcmp(token, "plt-push")) {
						f.method |= PLT_PUSH;
					} else if (!strcmp(token, "plt-got")) {
						f.method |= PLT_GOT;
					} else if (!strcmp(token, "got")) {
						f.method |= GOT;
					} else {
						free(f.target_name);
						free(f.source_name);
						free_func_list(func_list);
						fprintf(stderr, "error: unknown method at line %d: %s\n", line_nr, token);
						return 0;
					}
					break;
				}
			}

			token = strtok(0, ",");
			token_count++;
		}

		if (token_count < 4) {
			free(f.target_name);
			free(f.source_name);
			free_func_list(func_list);
			fprintf(stderr, "error: too few arguments at line %d\n", line_nr);
			return 0;
		}

		if (!func_list) {
			func_list = malloc(sizeof(struct func));
			current_func = func_list;
		} else {
			current_func->next = malloc(sizeof(struct func));
			current_func = current_func->next;
		}
		memcpy(current_func, &f, sizeof(struct func));

		if (swap) {
			struct func *f2 = malloc(sizeof(struct func));
			f2->target_name = strdup(current_func->source_name);
			f2->source_name = strdup(current_func->target_name);
			f2->method = current_func->method;
			f2->next = 0;

			current_func->next = f2;
			current_func = current_func->next;
		}

		line_nr++;
	};

	free(line);
	fclose(file);

	return func_list;
}

long get_file_size(FILE *file)
{
	long tmp = ftell(file);
	long size = 0;

	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, tmp, SEEK_SET);

	return size;
}

void *read_file(char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "error: failed to open the file: %s\n", path);
		return 0;
	}

	*size = get_file_size(file);
	char *buffer = malloc(*size);
	fread(buffer, *size, 1, file);
	fclose(file);

	return buffer;
}

int write_file(char *path, void *data, size_t size)
{
	FILE *file = fopen(path, "wb");
	if (!file) {
		fprintf(stderr, "error: failed to open the file: %s\n", path);
		return 0;
	}

	fwrite(data, size, 1, file);
	fclose(file);

	return 1;
}

size_t find_section(Elf64_Ehdr *ehdr, Elf64_Shdr *shdr_start, char *shstr, char *name, void *addr)
{
	*(size_t *)addr = 0;

	for (int i = 0; i < ehdr->e_shnum; i++) {
		Elf64_Shdr *shdr = &shdr_start[i];
		char *section_name = &shstr[shdr->sh_name];

		if (!strcmp(section_name, name)) {
			*(size_t *)addr = (size_t)ehdr + shdr->sh_offset;
			return shdr->sh_size;
		}
	}

	fprintf(stderr, "error: could not find the section: %s\n", name);
	return 0;
}

Elf64_Rela *find_rela(Elf64_Rela *rela_start, size_t rela_size, Elf64_Sym *sym_start, char *symtab, char *name)
{
	for (int i = 0; i < rela_size / sizeof(Elf64_Rela); i++) {
		Elf64_Rela *rela = &rela_start[i];
		int sym_index = ELF64_R_SYM(rela->r_info);
		char *sym_name = &symtab[sym_start[sym_index].st_name];

		if (!strcmp(sym_name, name)) {
			return rela;
		}
	}

	fprintf(stderr, "error: could not find the symbol: %s\n", name);
	return 0;
}

size_t addr_to_offset(Elf64_Phdr *phdr_start, size_t phdr_count, size_t addr)
{
	for (int i = 0; i < phdr_count; i++) {
		Elf64_Phdr *phdr = &phdr_start[i];
		if (phdr->p_type != PT_LOAD) {
			continue;
		}
		if (!(phdr->p_vaddr <= addr && addr < phdr->p_vaddr + phdr->p_memsz)) {
			continue;
		}

		size_t segment_offset = addr - phdr->p_vaddr;
		if (phdr->p_filesz <= segment_offset) {
			fprintf(stderr, "error: cannot translate the address to offset (probably in .bss): %lx\n", addr);
			return -1;
		}
		return phdr->p_offset + segment_offset;
	}

	fprintf(stderr, "error: the address is outisde elf binary: %lx\n", addr);
	return -1;
}

void *find_plt_got(uint8_t *func_plt)
{
	if (func_plt[0] == 0xff && func_plt[1] == 0x25) {
		// jmp qword [rip+...];
		return &func_plt[2];
	} else if (func_plt[0] == 0xf2 && func_plt[1] == 0xff && func_plt[2] == 0x25) {
		// -z bndplt: bnd jmp qword [ip+...]
		return &func_plt[3];
	} else if (func_plt[0] == 0xf3 && func_plt[1] == 0x0f && func_plt[2] == 0x1e && func_plt[3] == 0xfa && func_plt[4] == 0xf2 && func_plt[5] == 0xff && func_plt[6] == 0x25) {
		// -fcf-protection: endbr64; bnd jmp qword [ip+...]
		return &func_plt[7];
	}

	fprintf(stderr, "error: unknown plt type\n");
	return 0;
}

void *find_plt_push(uint8_t *func_plt)
{
	if (func_plt[0] == 0xff && func_plt[1] == 0x25 && func_plt[6] == 0x68) {
		// jmp qword [rip+...]; push ...; ...
		return &func_plt[6];
	} else if (func_plt[0] == 0x68) {
		// -z bndplt: push ...; ...
		return &func_plt[0];
	} else if (func_plt[0] == 0xf3 && func_plt[1] == 0x0f && func_plt[2] == 0x1e && func_plt[3] == 0xfa && func_plt[4] == 0x68) {
		// -fcf-protection: endbr64; push ...; ...
		return &func_plt[4];
	}

	fprintf(stderr, "error: unknown plt type\n");
	return 0;
}

void *translate_addr(void *dest_base, void *src_base, void *src_addr)
{
	return (void *)((size_t)dest_base + (size_t)src_addr - (size_t)src_base);
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("usage: %s <elf-file> <func-list-file>\n", argv[0]);
		return 1;
	}

	// load func list
	int func_count = 0;
	struct func *func_list = load_func_list(argv[2], &func_count);
	if (!func_list) {
		return 1;
	}

	// load elf binary
	size_t elf_size = 0;
	Elf64_Ehdr *ehdr = read_file(argv[1], &elf_size);
	if (!ehdr) {
		return 1;
	}

	// find program header
	Elf64_Phdr *phdr_start = (void *)((size_t)ehdr + ehdr->e_phoff);

	// find section header and shstr section
	Elf64_Shdr *shdr_start = (void *)((size_t)ehdr + ehdr->e_shoff);
	char *shstr = (void *)((size_t)ehdr + shdr_start[ehdr->e_shstrndx].sh_offset);

	// find other required sections

	Elf64_Sym *dynsym = 0;
	find_section(ehdr, shdr_start, shstr, ".dynsym", &dynsym);
	if (!dynsym) {
		return 1;
	}

	char *dynstr = 0;
	find_section(ehdr, shdr_start, shstr, ".dynstr", &dynstr);
	if (!dynstr) {
		return 1;
	}

	uint8_t *plt = 0;
	find_section(ehdr, shdr_start, shstr, ".plt", &plt);
	if (!plt) {
		return 1;
	}

	Elf64_Rela *rela_plt = 0;
	size_t rela_plt_size = find_section(ehdr, shdr_start, shstr, ".rela.plt", &rela_plt);
	if (!rela_plt) {
		return 1;
	}

	// .plt.sec is optional
	uint8_t *plt_sec = 0;
	find_section(ehdr, shdr_start, shstr, ".plt.sec", &plt_sec);

	int plt_sec_stub_size = 8;
	if (plt_sec && plt_sec[0] == 0xf3 && plt_sec[1] == 0x0f && plt_sec[2] == 0x1e && plt_sec[3] == 0xfa) {
		plt_sec_stub_size = 16;
	}


	// create a copy of the binary in memory where all the change will be made
	void *out_bin = malloc(elf_size);
	memcpy(out_bin, ehdr, elf_size);


	// start modifying PLT/GOT/RELA values
	struct func *current_func = func_list;
	while (current_func) {
		printf("%s <- %s:\n", current_func->target_name, current_func->source_name);

		// find target/source RELA
		Elf64_Rela *target_rela = find_rela(rela_plt, rela_plt_size, dynsym, dynstr, current_func->target_name);
		if (!target_rela) {
			return 1;
		}
		Elf64_Rela *source_rela = find_rela(rela_plt, rela_plt_size, dynsym, dynstr, current_func->source_name);
		if (!source_rela) {
			return 1;
		}

		// get PLT index and PLT adddress
		size_t target_index = ((size_t)target_rela - (size_t)rela_plt) / sizeof(Elf64_Rela);
		size_t source_index = ((size_t)source_rela - (size_t)rela_plt) / sizeof(Elf64_Rela);

		// PLT stub uses 16 bytes for each library function and the first stub is used for resolving symbols from other PLT stubs
		uint8_t *target_plt = plt + (1 + target_index) * 16;
		uint8_t *source_plt = plt + (1 + source_index) * 16;

		uint8_t *target_plt_sec = plt_sec + target_index * plt_sec_stub_size;
		uint8_t *source_plt_sec = plt_sec + source_index * plt_sec_stub_size;

		// get GOT value of target/source from RELA
		size_t target_got_offset = addr_to_offset(phdr_start, ehdr->e_phnum, target_rela->r_offset);
		size_t source_got_offset = addr_to_offset(phdr_start, ehdr->e_phnum, source_rela->r_offset);

		size_t *target_got = (void *)((size_t)ehdr + target_got_offset);
		size_t *source_got = (void *)((size_t)ehdr + source_got_offset);

		// print .plt and .plt.sec bytes
		printf("\t%s .plt:\n\t\t", current_func->target_name);
		for (int i = 0; i < 16; i++) {
			printf("%02x ", target_plt[i]);
		}
		printf("\n");
		if (plt_sec) {
			printf("\t%s .plt.sec:\n\t\t", current_func->target_name);
			for (int i = 0; i < plt_sec_stub_size; i++) {
				printf("%02x ", target_plt_sec[i]);
			}
			printf("\n");
		}

		printf("\t%s .plt:\n\t\t", current_func->source_name);
		for (int i = 0; i < 16; i++) {
			printf("%02x ", source_plt[i]);
		}
		printf("\n");
		if (plt_sec) {
			printf("\t%s .plt.sec:\n\t\t", current_func->source_name);
			for (int i = 0; i < plt_sec_stub_size; i++) {
				printf("%02x ", source_plt_sec[i]);
			}
			printf("\n");
		}

		// modify value based on specified method

		if (current_func->method & RELA_OFFSET) {
			memcpy(translate_addr(out_bin, ehdr, &target_rela->r_offset), &source_rela->r_offset, sizeof(size_t));
			printf("\tmodified rela r_offset\n");
		}
		if (current_func->method & RELA_INFO) {
			memcpy(translate_addr(out_bin, ehdr, &target_rela->r_info), &source_rela->r_info, sizeof(size_t));
			printf("\tmodified rela r_info\n");
		}
		if (current_func->method & PLT_PUSH) {
			void *target_plt_push = find_plt_push(target_plt);
			if (!target_plt_push) {
				return 1;
			}
			void *source_plt_push = find_plt_push(source_plt);
			if (!source_plt_push) {
				return 1;
			}

			memcpy(translate_addr(out_bin, ehdr, target_plt_push), source_plt_push, 5);
			printf("\tmodified plt push\n");
		}
		if (current_func->method & PLT_GOT) {
			if (plt_sec) {
				// if .plt.sec exists, plt-got is in there
				target_plt = target_plt_sec;
				source_plt = source_plt_sec;
			}

			uint32_t *target_plt_got = find_plt_got(target_plt);
			if (!target_plt_got) {
				return 1;
			}
			uint32_t *source_plt_got = find_plt_got(source_plt);
			if (!source_plt_got) {
				return 1;
			}

			uint32_t *target_addr = translate_addr(out_bin, ehdr, target_plt_got);
			memcpy(target_addr, source_plt_got, 4);

			// fix GOT relative address
			size_t delta = (size_t)target_plt_got - (size_t)source_plt_got;
			*target_addr -= delta;

			printf("\tmodified plt got\n");
		}
		if (current_func->method & GOT) {
			memcpy(translate_addr(out_bin, ehdr, target_got), source_got, sizeof(size_t));
			printf("\tmodified got\n");
		}

		current_func = current_func->next;
	}

	// save modified binary

	int path_len = strlen(argv[1]) + 5;
	char *out_path = malloc(path_len);
	strncpy(out_path, argv[1], path_len);
	strncat(out_path, ".obf", path_len);
	if (!write_file(out_path, out_bin, elf_size)) {
		return 1;
	}
	printf("written binary to: %s\n", out_path);

	// clean up shits

	free_func_list(func_list);
	free(ehdr);

	return 0;
}
