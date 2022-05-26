// Ed Callaghan
// Licensing statement goes here

#ifndef BoardCommManager__hh
#define BoardCommManager__hh

#include <unistd.h>
#include <stdexcept>

class BoardCommManager {
    public:
        virtual void write32(uint32_t base, uint32_t addr, uint32_t data) = 0;
        virtual uint32_t read32(uint32_t base, uint32_t addr) = 0;
        virtual void write16(uint32_t base, uint32_t addr, uint32_t data) = 0;
        virtual uint32_t read16(uint32_t base, uint32_t addr) = 0;
        virtual uint32_t readBLT(uint32_t base, uint32_t addr, void *buffer, uint32_t size) = 0;
};

#endif
