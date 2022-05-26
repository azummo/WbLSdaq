// Ed Callaghan
// Licensing statement goes here

#ifndef CONETNetwork__hh
#define CONETNetwork__hh

#include <BoardCommManager.hh>
#include <unistd.h>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <CAENComm.h>

class CONETNetwork: public BoardCommManager {
    public:
        CONETNetwork(int link);
       ~CONETNetwork() noexcept(false);

        inline int getLinkNum() { return link; }

        inline void write32(uint32_t base, uint32_t addr, uint32_t data) {
            int node = static_cast<int>(base);
            int handle = this->handles[node];
            int res = CAENComm_Write32(handle, addr, data);
            if (res) {
                std::stringstream err;
                err << error_codes[-res] << " :: write32 @ node " << node << ":" << std::hex << addr << " : " << data;
                throw std::runtime_error(err.str());
            }
            usleep(10000);
        }

        inline uint32_t read32(uint32_t base, uint32_t addr) {
            int node = static_cast<int>(base);
            int handle = this->handles[node];
            uint32_t read = 0;
            usleep(1);
            int res = CAENComm_Read32(handle, addr, &read);
            if (res) {
                std::stringstream err;
                err << error_codes[-res] << " :: read32 @ node " << node << ":" << std::hex << addr;
                throw std::runtime_error(err.str());
            }
            return read;
        }

        inline void write16(uint32_t base, uint32_t addr, uint32_t data) {
            int node = static_cast<int>(base);
            int handle = this->handles[node];
            int res = CAENComm_Write16(handle, addr, data);
            if (res) {
                std::stringstream err;
                err << error_codes[-res] << " :: write16 @ node " << node << ":" << std::hex << addr << " : " << data;
                throw std::runtime_error(err.str());
            }
            usleep(10000);
        }

        inline uint32_t read16(uint32_t base, uint32_t addr) {
            int node = static_cast<int>(base);
            int handle = this->handles[node];
            uint16_t buff = 0;
            usleep(1);
            int res = CAENComm_Read16(handle, addr, &buff);
            if (res) {
                std::stringstream err;
                err << error_codes[-res] << " :: read16 @ node " << node << ":" << std::hex << addr;
                throw std::runtime_error(err.str());
            }
            uint32_t read = static_cast<uint32_t>(buff);
            return read;
        }

        inline uint32_t readBLT(uint32_t base, uint32_t addr, void *buffer, uint32_t size) {
            int node = static_cast<int>(base);
            int handle = this->handles[node];
            uint32_t nwords;
            // size is in bytes, convert to # of words (i.e. units of 4 bytes)
            int bltsize = size/4;
            if (size % 4 != 0){
                bltsize++;
            }
            usleep(1);
            int res = CAENComm_MBLTRead(handle, addr, (uint32_t*)buffer, bltsize, (int*)&nwords);
            if (res && (res != -1)) { //we ignore bus errors for BLT
                std::stringstream err;
                err << error_codes[-res] << " :: readBLT @ node " << node << ":" << std::hex << addr;
                throw std::runtime_error(err.str());
            }
            uint32_t bytes = nwords*4;
            return bytes;
        }

    protected:
        static int max_handles; // == 8 for a CONET2 network
        static const std::string error_codes[15];

        int link;
        std::vector<int> handles;
        std::vector<bool> valids;
};

#endif
