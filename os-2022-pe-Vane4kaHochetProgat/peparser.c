#include <stdio.h>
#include <string.h>

#define POSITION_OFFSET 0x3C
#define SECTION_NUMBER_OFFSET 6
#define PE00_SIZE 4
#define COFF_SIZE 20
#define OPTIONAL_HEADER_OFFSET PE00_SIZE + COFF_SIZE
#define IMPORT_TABLE_RVA_OFFSET OPTIONAL_HEADER_OFFSET +0x78
#define OPTIONAL_HEADER_SIZE 240
#define IMPORT_TABLE_OFFSET PE00_SIZE + COFF_SIZE + OPTIONAL_HEADER_SIZE
#define FLAG_BIT_MASK (1L << 30)

struct pe_info {
    FILE *file;
    int end;
    int position;
    int sections_number;
} default_pe = {NULL, -1, -1, 0};

void fill_end(struct pe_info *pe) {
    fseek(pe->file, 0L, SEEK_END);
    pe->end = (int) ftell(pe->file);
}

void fill_position(struct pe_info *pe) {
    if (fseek(pe->file, POSITION_OFFSET, SEEK_SET) != 0) {
        pe->position = pe->end;
        return;
    }
    if (fread(&pe->position, 4, 1, pe->file) != 1) {
        pe->position = pe->end;
    }
}

void fill_sections_number(struct pe_info *pe) {
    if (fseek(pe->file, pe->position + SECTION_NUMBER_OFFSET, SEEK_SET) != 0) {
        pe->sections_number = -1;
        return;
    }
    if (fread(&pe->sections_number, 2, 1, pe->file) != 1) {
        pe->sections_number = -1;
    }
}

int find_raw(struct pe_info *pe, const int rva) {
    char section_buffer[40];
    int section_rva, section_vsize, section_raw;
    fseek(pe->file, pe->position + IMPORT_TABLE_OFFSET, SEEK_SET);
    for (int i = 0; i < pe->sections_number; i++) {
        fread(section_buffer, 1, 40, pe->file);
        section_rva = *((int *) &section_buffer[12]), section_vsize = *((int *) &section_buffer[8]), section_raw = *((int *) &section_buffer[20]);
        if (section_rva <= rva && rva <= section_rva + section_vsize) {
            return rva - section_rva + section_raw;
        }
    }
    return pe->end;
}

int is_pe(struct pe_info *pe) {
    char buf[4];

    if (fseek(pe->file, pe->position, SEEK_SET) != 0) {
        return 0;
    }

    if (fread(buf, 1, 4, pe->file) != 4) {
        return 0;
    }
    return buf[0] == 'P' && buf[1] == 'E' && buf[2] == '\0' && buf[3] == '\0';
}

void import_functions(struct pe_info *pe) {
    int import_table_rva = 0, rva_table_size = 0, import_table_raw;
    fseek(pe->file, pe->position + IMPORT_TABLE_RVA_OFFSET, SEEK_SET);
    fread(&import_table_rva, 4, 1, pe->file);
    fread(&rva_table_size, 4, 1, pe->file);
    fseek(pe->file, pe->position + IMPORT_TABLE_OFFSET, SEEK_SET);
    import_table_raw = find_raw(pe, import_table_rva);
    for (int j = 0; j < rva_table_size; j += 20) {
        int lookup_table_rva = 0, name_rva = 0, flag = 0, func_name_rva = 0, lookup_table_raw;
        char name[64];
        fseek(pe->file, import_table_raw + j, SEEK_SET);
        fread(&lookup_table_rva, 1, 4, pe->file);
        fseek(pe->file, import_table_raw + j + 0xC, SEEK_SET);
        fread(&name_rva, 4, 1, pe->file);
        fseek(pe->file, import_table_raw - import_table_rva + name_rva, SEEK_SET);
        fread(name, 1, 64, pe->file);
        if (name[0] == '\0') {
            break;
        }
        lookup_table_raw = find_raw(pe, lookup_table_rva);
        fseek(pe->file, lookup_table_raw, SEEK_SET);
        fread(&func_name_rva, 4, 1, pe->file);
        fread(&flag, 1, 4, pe->file);
        printf("%s\n", name);
        while (1) {
            if ((flag & FLAG_BIT_MASK) == 0) {
                int lookup_pos, func_name_raw;
                lookup_pos = (int) ftell(pe->file);
                func_name_raw = find_raw(pe, 2 + func_name_rva & (int) (FLAG_BIT_MASK - 1));
                fseek(pe->file, func_name_raw, SEEK_SET);
                if (fread(name, 1, 64, pe->file) != 64 || name[0] == '\0') {
                    break;
                }
                fseek(pe->file, lookup_pos, SEEK_SET);
                printf("    %s\n", name);
            }
            fread(&func_name_rva, 4, 1, pe->file);
            fread(&flag, 4, 1, pe->file);
        }
    }
}

void export_functions(struct pe_info *pe) {
    int export_table_rva = 0, export_table_size = 0, export_raw, export_count, names_table_rva, names_raw, name_rva;
    char name[64];
    fseek(pe->file, pe->position + 24 + 112, SEEK_SET);
    fread(&export_table_rva, 4, 1, pe->file);
    fread(&export_table_size, 4, 1, pe->file);
    export_raw = find_raw(pe, export_table_rva);
    fseek(pe->file, export_raw + 24, SEEK_SET);
    fread(&export_count, 4, 1, pe->file);
    fseek(pe->file, export_raw + 32, SEEK_SET);
    fread(&names_table_rva, 4, 1, pe->file);
    names_raw = find_raw(pe, names_table_rva);
    fseek(pe->file, names_raw, SEEK_SET);
    for (int i = 0; i < export_count * 4; i += 4) {
        fread(&name_rva, 4, 1, pe->file);
        int name_raw = find_raw(pe, name_rva);
        fseek(pe->file, name_raw, SEEK_SET);
        fread(name, 64, 1, pe->file);
        printf("%s\n", name);
        fseek(pe->file, names_raw + i + 4, SEEK_SET);
    }
}

int main(int argc, char **argv) {
    if (argc == 2) {
        fprintf(stderr, "Passed %d parameters. Need two parameters: the mode and the file\n", argc);
        return 1;
    }
    char *mode = argv[1];
    char *target = argv[2];
    struct pe_info pe = default_pe;
    pe.file = fopen(target, "rb");
    if (pe.file == NULL) {
        fprintf(stderr, "Can't open file %s\n", target);
        return 1;
    }
    fill_end(&pe);
    fill_position(&pe);
    if (strcmp(mode, "is-pe") == 0) {
        if (is_pe(&pe) == 1) {
            printf("PE\n");
            return 0;
        } else {
            printf("Not PE\n");
            return 1;
        }
    } else if (strcmp(mode, "import-functions") == 0) {
        fill_sections_number(&pe);
        import_functions(&pe);
        return 0;
    } else if (strcmp(mode, "export-functions") == 0) {
        fill_sections_number(&pe);
        export_functions(&pe);
        return 0;
    } else {
        fprintf(stderr, "Mode %s isn't supposed to be implemented\n", mode);
        return 1;
    }
}