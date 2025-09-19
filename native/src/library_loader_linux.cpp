#ifdef __linux__
    #include <elf.h>
    #include <link.h>

    #include <cstring>
    #include <string>
    #include <vector>

    #include "library_loader.h"

namespace jvmtool {

struct LoadContext {
    const char* target;  // basename to match
    CodeCache* result;
};

static bool match_basename(const char* path, const char* base) {
    if (path == nullptr || base == nullptr)
        return false;
    const char* p = std::strrchr(path, '/');
    p = p ? p + 1 : path;
    return std::strcmp(p, base) == 0;
}

// Basic ELF symbol table parser (SYMTAB + STRTAB) for shared object loaded at info->dlpi_addr.
static void parse_elf_symbols(const dl_phdr_info* info, CodeCache* cache) {
    if (info->dlpi_addr == 0)
        return;
    const unsigned char* base = reinterpret_cast<const unsigned char*>(info->dlpi_addr);
    const ElfW(Ehdr)* ehdr = reinterpret_cast<const ElfW(Ehdr)*>(base);
    if (std::memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0)
        return;

    const ElfW(Shdr)* shdrs = reinterpret_cast<const ElfW(Shdr)*>(base + ehdr->e_shoff);
    if (ehdr->e_shentsize == 0)
        return;

    const ElfW(Shdr)* symtab = nullptr;
    const ElfW(Shdr)* strtab = nullptr;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const ElfW(Shdr) & sh = shdrs[i];
        if (sh.sh_type == SHT_SYMTAB) {
            symtab = &sh;
        } else if (sh.sh_type == SHT_STRTAB && i != ehdr->e_shstrndx) {
            // Non-section-name string table (likely symbol strings)
            if (!strtab || sh.sh_size > strtab->sh_size) {
                strtab = &sh;
            }
        }
    }

    if (!symtab || !strtab)
        return;

    const char* strings = reinterpret_cast<const char*>(base + strtab->sh_offset);
    const ElfW(Sym)* symbols = reinterpret_cast<const ElfW(Sym)*>(base + symtab->sh_offset);
    size_t count = symtab->sh_size / symtab->sh_entsize;

    for (size_t i = 0; i < count; i++) {
        const ElfW(Sym) & s = symbols[i];
        if (ELF64_ST_TYPE(s.st_info) == STT_FUNC && s.st_value != 0) {
            const char* name = strings + s.st_name;
            // filter out empty and local if desired (binding check optional)
            if (name && *name) {
                const void* addr = reinterpret_cast<const void*>(info->dlpi_addr + s.st_value);
                cache->add(addr, s.st_size, name);
            }
        }
    }
    cache->sort();
}

static int dl_iterate_callback(struct dl_phdr_info* info, size_t, void* data) {
    LoadContext* ctx = reinterpret_cast<LoadContext*>(data);
    if (ctx->result != nullptr)
        return 0;  // already found

    if (info->dlpi_name && match_basename(info->dlpi_name, ctx->target)) {
        // Determine address bounds using program headers
        const void* min_addr = reinterpret_cast<const void*>(~(uintptr_t)0);
        const void* max_addr = nullptr;
        for (int i = 0; i < info->dlpi_phnum; i++) {
            if (info->dlpi_phdr[i].p_type == PT_LOAD) {
                const unsigned char* seg_start = reinterpret_cast<const unsigned char*>(
                    info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
                const unsigned char* seg_end = seg_start + info->dlpi_phdr[i].p_memsz;
                if (seg_start < min_addr)
                    min_addr = seg_start;
                if (seg_end > max_addr)
                    max_addr = seg_end;
            }
        }
        if (max_addr == nullptr)
            max_addr = min_addr;  // fallback

        CodeCache* cache = new CodeCache(ctx->target, /*lib_index*/ 0, min_addr, max_addr,
                                         (const char*)info->dlpi_addr);
        parse_elf_symbols(info, cache);
        ctx->result = cache;
        return 0;  // stop after found
    }
    return 0;  // continue
}

CodeCache* LibraryLoader::findLibraryByName(const char* libName) {
    if (!libName)
        return nullptr;
    LoadContext ctx{libName, nullptr};
    dl_iterate_phdr(dl_iterate_callback, &ctx);
    return ctx.result;  // may be nullptr if not found
}

}  // namespace jvmtool

#endif  // __linux__
