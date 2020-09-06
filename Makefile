TOPTARGETS = build clean

IMAGES = common \
		cdrom/disc-swap \
		cdrom/getloc \
		cdrom/timing \
		cpu/access-time \
		cpu/code-in-scratchpad \
		cpu/cop \
		dma/chain-looping \
		dma/chopping \
		dma/otc-test \
		gpu/bandwidth \
		gpu/benchmark \
		gpu/clipping \
		gpu/clut-cache \
		gpu/gp0-e1 \
		gpu/lines \
		gpu/mask-bit \
		gpu/quad \
		gpu/rectangles \
		gpu/texture-overflow \
		gpu/transparency \
		gpu/triangle \
		gpu/vram-to-vram-overlap \
		gpu/version-detect \
		gte-fuzz \
		mdec \
		spu/memory-transfer \
		spu/ram-sandbox \
		spu/stereo \
		spu/test \
		spu/toolbox \
		timers

all: $(TOPTARGETS)

$(TOPTARGETS): $(IMAGES)
$(IMAGES):
	@$(MAKE) --no-print-directory -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTAGETS) $(IMAGES)
