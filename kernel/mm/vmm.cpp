#include "vmm.h"
void VMM::init() {}
void VMM::map_page(uint32_t virt, uint32_t phys, uint32_t flags) {}
void VMM::unmap_page(uint32_t virt) {}
uint32_t VMM::virt_to_phys(uint32_t virt) { return virt; }
uint32_t* VMM::create_address_space() { return nullptr; }
void VMM::switch_address_space(uint32_t* page_dir) {}
