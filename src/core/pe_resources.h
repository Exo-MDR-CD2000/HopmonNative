// Minimal, portable reader for resources embedded in a PE32 executable.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>

class PEResources {
public:
    bool Load(const std::string& path, std::string* error);
    bool LoadFromMemory(std::vector<uint8_t> bytes, std::string* error);

    // Integer-typed resource (RT_BITMAP = 2, RT_RCDATA = 10, ...)
    const std::vector<uint8_t>* Find(uint32_t type, uint32_t id) const;
    // String-typed resource (e.g. "WAVE")
    const std::vector<uint8_t>* Find(const std::string& type, uint32_t id) const;

    // Resource interpreted as a NUL-terminated C string read from the mapped image,
    // exactly like the original code did (it may run past the resource's declared size).
    std::string FindCString(uint32_t type, uint32_t id) const;
    const std::vector<uint8_t>& FileBytes() const { return file_; }
    size_t Count() const { return byInt_.size() + byName_.size(); }

private:
    struct Section { uint32_t va, vsize, rawOff, rawSize; };
    uint32_t RvaToOffset(uint32_t rva) const;
    bool Walk(uint32_t dirOff, int level, uint32_t typeInt, const std::string& typeName, uint32_t id, std::string* err);

    std::vector<uint8_t> file_;
    std::vector<Section> sections_;
    uint32_t rsrcOff_ = 0;
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> byInt_;
    std::map<std::pair<std::string, uint32_t>, std::vector<uint8_t>> byName_;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> offInt_;
};

enum { RT_BITMAP_ = 2, RT_RCDATA_ = 10 };
