#ifdef __APPLE__
    #include <mach-o/dyld.h>
    #include <mach-o/loader.h>
    #include <mach-o/nlist.h>

    #include <cstring>
    #include <string>
    #include <vector>

    #include "library_loader.h"

namespace jvmtool {

// Minimal Mach-O parser to populate a CodeCache with exported symbols.
class MachOParser {
    CodeCache* _cache;
    const mach_header* _header;
    intptr_t _slide;

  public:
    MachOParser(CodeCache* cache, const mach_header* header, intptr_t slide)
        : _cache(cache), _header(header), _slide(slide) {}

    bool parse() {
        if (_header == nullptr) {
            return false;
        }
    #if defined(MH_MAGIC_64)
        if (_header->magic != MH_MAGIC_64 && _header->magic != MH_CIGAM_64) {
            return false;  // only 64-bit
        }
        const mach_header_64* h64 = reinterpret_cast<const mach_header_64*>(_header);
        const load_command* lc = reinterpret_cast<const load_command*>(h64 + 1);
        const char* link_base = nullptr;
        const symtab_command* symtab = nullptr;

        for (uint32_t i = 0; i < h64->ncmds; i++) {
            switch (lc->cmd) {
                case LC_SEGMENT_64: {
                    const segment_command_64* seg = reinterpret_cast<const segment_command_64*>(lc);
                    if (std::strcmp(seg->segname, "__TEXT") == 0) {
                        _cache->updateBounds(_header,
                                             reinterpret_cast<const char*>(_header) + seg->vmsize);
                        _cache->setTextBase(reinterpret_cast<const char*>(_slide));
                    } else if (std::strcmp(seg->segname, "__LINKEDIT") == 0) {
                        link_base =
                            reinterpret_cast<const char*>(_slide + seg->vmaddr - seg->fileoff);
                    }
                    break;
                }
                case LC_SYMTAB: {
                    symtab = reinterpret_cast<const symtab_command*>(lc);
                    break;
                }
                default: {
                    // Other load commands not needed for our lightweight symbol extraction.
                    break;
                }
            }
            lc = reinterpret_cast<const load_command*>(reinterpret_cast<const char*>(lc) +
                                                       lc->cmdsize);
        }

        if (symtab && link_base) {
            parseSymbols(symtab, link_base);
        }
    #endif
        _cache->sort();
        return true;
    }

  private:
    void parseSymbols(const symtab_command* sym, const char* link_base) {
        const nlist_64* symbols = reinterpret_cast<const nlist_64*>(link_base + sym->symoff);
        const char* strtab = link_base + sym->stroff;
        for (uint32_t i = 0; i < sym->nsyms; i++) {
            const nlist_64& n = symbols[i];
            if ((n.n_type & 0x0e) == 0x0e && n.n_value != 0) {  // external & defined
                const char* name = strtab + n.n_un.n_strx;
                if (name == nullptr || *name == '\0') {
                    continue;
                }
                if (name[0] == '_') {
                    name++;  // strip leading underscore
                }
                const void* addr = reinterpret_cast<const void*>(
                    reinterpret_cast<const char*>(_slide) + n.n_value);
                _cache->add(addr, 0, name);
            }
        }
    }
};

CodeCache* LibraryLoader::findLibraryByName(const char* libName) {
    if (libName == nullptr) {
        return nullptr;
    }
    uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; i++) {
        const char* image_path = _dyld_get_image_name(i);
        if (!image_path) {
            continue;
        }
        // extract basename
        const char* base = std::strrchr(image_path, '/');
        base = base ? base + 1 : image_path;
        if (std::strcmp(base, libName) == 0) {
            const mach_header* header = _dyld_get_image_header(i);
            intptr_t slide = _dyld_get_image_vmaddr_slide(i);
            // Ownership: returned raw pointer managed by caller (AgentManager / VMStructs init).
            CodeCache* cache = new CodeCache(base, static_cast<short>(i), NO_MIN_ADDRESS,
                                             NO_MAX_ADDRESS, reinterpret_cast<const char*>(slide));
            MachOParser parser(cache, header, slide);
            if (!parser.parse()) {
                delete cache;
                return nullptr;
            }
            return cache;
        }
    }
    return nullptr;
}

}  // namespace jvmtool

#endif  // __APPLE__
