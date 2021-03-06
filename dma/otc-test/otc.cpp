#include "otc.h"
#include <io.h>

using namespace DMA;

void waitForDma(Channel ch) {
    volatile CHCR* control = (CHCR*)(0x1F801088 + 0x10 * (int)ch);

    while (
        control->startTrigger == CHCR::StartTrigger::manual && 
        control->enabled != CHCR::Enabled::completed
    );
}

// Separate compilation unit for these functions to prevent inlining
void setupDmaOtc(uint32_t address, uint16_t wordCount, CHCR control) {
    MADDR addr;
    addr.address = address;

    BCR block = BCR::mode0(wordCount);

    waitForDma(Channel::OTC);

    write32(DMA::baseAddr(Channel::OTC),    addr._reg);
    write32(DMA::blockAddr(Channel::OTC),   block._reg);
    write32(DMA::controlAddr(Channel::OTC), control._reg);
}

void dmaMasterEnable(Channel ch, bool enabled) {
    uint32_t dmaControl = read32(CONTROL_ADDR);
    uint32_t mask = 0b1000 << ((int)ch * 4);
    if (enabled) {
        dmaControl |= mask;
    } else {
        dmaControl &= ~mask;
    }
    write32(CONTROL_ADDR, dmaControl);
}