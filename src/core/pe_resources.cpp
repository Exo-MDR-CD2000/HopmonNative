#include "pe_resources.h"
#include "log.h"
#include <cstdio>
#include <cstring>

static uint16_t rd16(const std::vector<uint8_t>& b, size_t o) { return o + 2 <= b.size() ? (uint16_t)(b[o] | b[o + 1] << 8) : 0; }
static uint32_t rd32(const std::vector<uint8_t>& b, size_t o) {
    return o + 4 <= b.size() ? (uint32_t)(b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (uint32_t)b[o + 3] << 24) : 0;
}

bool PEResources::Load(const std::string& path, std::string* error) {
    FILE* f = OpenFileUtf8(path.c_str(), "rb");
    if (!f) { if (error) *error = "cannot open " + path; return false; }
    std::vector<uint8_t> bytes;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n > 0) {
        bytes.resize((size_t)n);
        if (fread(bytes.data(), 1, (size_t)n, f) != (size_t)n) bytes.clear();
    }
    fclose(f);
    return LoadFromMemory(std::move(bytes), error);
}

uint32_t PEResources::RvaToOffset(uint32_t rva) const {
    for (const auto& s : sections_)
        if (rva >= s.va && rva < s.va + (s.vsize ? s.vsize : s.rawSize)) return rva - s.va + s.rawOff;
    return 0;
}

bool PEResources::LoadFromMemory(std::vector<uint8_t> bytes, std::string* error) {
    file_ = std::move(bytes);
    byInt_.clear();
    byName_.clear();
    sections_.clear();
    auto fail = [&](const char* m) { if (error) *error = m; return false; };
    if (file_.size() < 0x40 || file_[0] != 'M' || file_[1] != 'Z') return fail("not an MZ executable");
    uint32_t pe = rd32(file_, 0x3c);
    if (rd32(file_, pe) != 0x00004550) return fail("not a PE executable");
    uint16_t nsec = rd16(file_, pe + 6);
    uint16_t optSize = rd16(file_, pe + 20);
    uint32_t opt = pe + 24;
    uint16_t magic = rd16(file_, opt);
    uint32_t dirBase = opt + (magic == 0x20b ? 112 : 96);
    uint32_t rsrcRva = rd32(file_, dirBase + 2 * 8);
    uint32_t sec = opt + optSize;
    for (int i = 0; i < nsec; i++) {
        uint32_t o = sec + i * 40;
        Section s{rd32(file_, o + 12), rd32(file_, o + 8), rd32(file_, o + 20), rd32(file_, o + 16)};
        sections_.push_back(s);
    }
    if (!rsrcRva) return fail("executable has no resources");
    rsrcOff_ = RvaToOffset(rsrcRva);
    if (!rsrcOff_) return fail("bad resource directory");
    return Walk(0, 0, 0, "", 0, error);
}

bool PEResources::Walk(uint32_t dirOff, int level, uint32_t typeInt, const std::string& typeName, uint32_t id, std::string* err) {
    uint32_t d = rsrcOff_ + dirOff;
    uint16_t nNamed = rd16(file_, d + 12), nId = rd16(file_, d + 14);
    if (nNamed + nId > 4096) { if (err) *err = "corrupt resource directory"; return false; }
    for (int i = 0; i < nNamed + nId; i++) {
        uint32_t e = d + 16 + i * 8;
        uint32_t name = rd32(file_, e), target = rd32(file_, e + 4);
        uint32_t nInt = 0;
        std::string nStr;
        if (name & 0x80000000u) {
            uint32_t so = rsrcOff_ + (name & 0x7fffffffu);
            uint16_t len = rd16(file_, so);
            for (int k = 0; k < len; k++) nStr += (char)rd16(file_, so + 2 + k * 2);
        } else {
            nInt = name;
        }
        if (target & 0x80000000u) {
            uint32_t sub = target & 0x7fffffffu;
            if (level == 0) {
                if (!Walk(sub, 1, nInt, nStr, 0, err)) return false;
            } else if (level == 1) {
                if (!Walk(sub, 2, typeInt, typeName, nInt, err)) return false;
            }
        } else if (level == 2) {
            uint32_t de = rsrcOff_ + target;
            uint32_t rva = rd32(file_, de), size = rd32(file_, de + 4);
            uint32_t off = RvaToOffset(rva);
            if (!off || off + size > file_.size()) continue;
            std::vector<uint8_t> blob(file_.begin() + off, file_.begin() + off + size);
            if (typeName.empty()) { byInt_[{typeInt, id}] = std::move(blob); offInt_[{typeInt, id}] = off; }
            else byName_[{typeName, id}] = std::move(blob);
        }
    }
    return true;
}

const std::vector<uint8_t>* PEResources::Find(uint32_t type, uint32_t id) const {
    auto it = byInt_.find({type, id});
    return it == byInt_.end() ? nullptr : &it->second;
}

const std::vector<uint8_t>* PEResources::Find(const std::string& type, uint32_t id) const {
    auto it = byName_.find({type, id});
    return it == byName_.end() ? nullptr : &it->second;
}

std::string PEResources::FindCString(uint32_t type, uint32_t id) const {
    auto it = offInt_.find({type, id});
    if (it == offInt_.end()) return std::string();
    size_t o = it->second;
    std::string s;
    // memory image of .rsrc: bytes after the resource are the following data in the section
    while (o < file_.size() && file_[o] != 0) s += (char)file_[o++];
    return s;
}
