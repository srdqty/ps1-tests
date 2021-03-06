#include <stdio.h>
#include <stdlib.h>
#include <psxapi.h>
#include <psxgpu.h>
#include <psxetc.h>
#include <psxsio.h>
#include <io.h>

DISPENV disp;
DRAWENV draw;

#define OT_LEN 8
unsigned int ot[2][OT_LEN];
int db = 0;

const int testCount = 11; // First result is omitted


void setResolution(int w, int h) {
    SetDefDispEnv(&disp, 0, 0, w, h);
    SetDefDrawEnv(&draw, 0, 0, w, h);

    PutDispEnv(&disp);
    PutDrawEnv(&draw);
}

void setDisplayResolution(int w, int h) {
    SetDefDispEnv(&disp, 0, 0, w, h);
    PutDispEnv(&disp);
}

void initVideo()
{
    ResetGraph(0);
    setResolution(320, 240);
    SetDispMask(1);
}

void display()
{
    DrawSync(0);
    VSync(0);

    PutDrawEnv(&draw);
    DrawOTag(ot[db] + (OT_LEN - 1));

    db ^= 1;
    ClearOTagR(ot[db], OT_LEN);
    SetDispMask(1);
}

const char *modes[3][4] = {
    {"System clock",
     "Dot clock",
     "System clock",
     "Dot clock"},
    {"System clock",
     "Hblank",
     "System clock",
     "Hblank"},
    {"System clock",
     "System clock",
     "System clock/8",
     "System clock/8"},
};

const char *syncTypes[3][4] = {
        {"0 = Pause counter during Hblanks",
         "1 = Reset counter to 0 at Hblanks",
         "2 = Reset counter to 0 at Hblanks and pause outside of Hblank",
         "3 = Pause until Hblank occurs once, then switch to Free Run"},
        {"0 = Pause counter during Vblanks",
         "1 = Reset counter to 0 at Vblanks",
         "2 = Reset counter to 0 at Vblanks and pause outside of Vblank",
         "3 = Pause until Vblank occurs once, then switch to Free Run"},
        {"0 = Stop counter at current value",
         "1 = Free Run",
         "2 = Free Run",
         "3 = Stop counter at current value"},
};

const int resolutions[5] = {
    256,
    320,
    368,
    512,
    640
};

const char *dotclocksFrequencies[5] = {
    "5.32224 MHz",
    "6.65280 MHz",
    "7.60320 MHz",
    "10.64448 MHz",
    "13.30560 MHz",
};

const char* getDotclockFrequencyForCurrentResolution() {
    switch (disp.disp.w) {
        case 256: return dotclocksFrequencies[0];
        case 320: return dotclocksFrequencies[1];
        case 368: return dotclocksFrequencies[2];
        case 512: return dotclocksFrequencies[3];
        case 640: return dotclocksFrequencies[4];
    }
    return "";
}

void __attribute__((optimize("O1"))) delay(uint32_t cycles)
{
    uint32_t delay = cycles >> 2;
    do {
        __asm__ __volatile__ ("nop");
        --delay;
    } while (delay != 0);
}

volatile int currVsync = 0;
volatile int prevVsync = 0;

void vblank_irq() {
    currVsync = !currVsync;
    // printf("VBlank!\n");
}

void waitVsync() {
    while (currVsync == prevVsync) ;
    prevVsync = currVsync;
}

uint16_t initTimer(int timer, int mode) {
    uint32_t timerBase = 0x1f801100 + (timer * 0x10);
    uint16_t prevMode = read16(timerBase+4);
    write16(timerBase + 4, mode << 8);

    return prevMode;
}

uint16_t initTimerWithSync(int timer, int mode, int sync) {
    uint32_t timerBase = 0x1f801100 + (timer * 0x10);
    uint16_t prevMode = read16(timerBase+4);
    write16(timerBase + 4, (mode << 8) | ((sync&3) << 1) | 1);

    return prevMode;
}

void restoreTimer(int timer, uint16_t prevMode) {
    uint32_t timerBase = 0x1f801100 + (timer * 0x10);
    write16(timerBase+4, prevMode);
}

void testTimerWithCyclesDelay(int timer, int mode, int cycles) {
    uint32_t results[testCount];
    const uint32_t timerBase = 0x1f801100 + (timer * 0x10);
    uint16_t prevState = initTimer(timer, mode);

    int32_t start = read16(timerBase);
    int32_t end;
    int32_t diff;
    uint16_t flags = 0;
    for (int i = 0; i < testCount; i++) {
        start = read16(timerBase);

        delay(cycles);

        end = read16(timerBase);
        flags = read16(timerBase + 4);
        if (flags & (1 << 12)) end += 0xffff;

        diff = end - start;

        results[i] = diff;
    }
    restoreTimer(timer, prevState);

    // Print results
    printf("Timer%d, clock: %14s, %4d cycles delay: ", timer, modes[timer][mode], cycles);
    for (int i = 1; i < testCount; i++) {
        printf("%6d, ", results[i]);
    }
    printf("\n");
}

void testTimerWithFrameDelay(int timer, int mode) {
    uint32_t results[testCount];
    const uint32_t timerBase = 0x1f801100 + (timer * 0x10);
    uint16_t prevState = initTimer(timer, mode);

    // Save previous VBlank callback - we will do vsync manually
    void* prevVblankCallback = GetInterruptCallback(0);
    InterruptCallback(0, vblank_irq);

    for (int i = 0; i < testCount; i++)
    {
        // Reset timer
        write16(timerBase + 4, mode << 8);

        waitVsync();

        int32_t value = read16(timerBase);
        uint16_t flags = read16(timerBase + 4);
        if (flags & (1 << 12)) value += 0xffff;

        results[i] = value;
    }
    restoreTimer(timer, prevState);

    InterruptCallback(0, vblank_irq);

    // Print results
    printf("Timer%d, clock: %14s,       frame delay: ", timer, modes[timer][mode]);
    for (int i = 1; i < testCount; i++) {
        printf("%6d, ", results[i]);
    }
    printf("\n");
}

void testTimerWithCyclesDelaySyncEnabled(int timer, int mode, int sync, int cycles) {
    uint32_t results[testCount];
    const uint32_t timerBase = 0x1f801100 + (timer * 0x10);
    uint16_t prevState = initTimerWithSync(timer, mode, sync);

    int32_t end;
    uint16_t flags = 0;
    for (int i = 0; i < testCount; i++) {
        delay(cycles);

        end = read16(timerBase);
        flags = read16(timerBase + 4);
        if (flags & (1 << 12)) end += 0xffff;

        results[i] = end;
    }
    restoreTimer(timer, prevState);

    // Print results
    printf("%-61s", syncTypes[timer][sync]);
    for (int i = 1; i < testCount; i++) {
        printf("%6d, ", results[i]);
    }
    printf("\n");
}


void testTimerTargetBehaviour() 
{
    const int timer = 2;
    const int target = 10;
    const int mode = 0;
    
    uint16_t ticks[300];
    
    void __attribute__((optimize("Os"))) readTimerValues() 
    {
        volatile uint16_t* value = (uint16_t*)(0x1f801100 + (timer * 0x10));
        for (uint16_t i = 10000;i;i--) {
            ticks[*value]++;
        }
    }

    // Count from 0 to TARGET (included)
    printf("\nCheck when Timer%d counter (%s) is reset when reaching target (= %d).\n", timer, modes[timer][mode], target);

    const uint32_t timerBase = 0x1f801100 + (timer * 0x10);
    uint16_t prevState = read16(timerBase + 4);
    
    for (int i = 0; i < 300; i++) ticks[i] = 0;
     
    EnterCriticalSection();
    write16(timerBase + 8, target);
    write16(timerBase + 4, 
                    /* No sync */
        (1 << 3) |  /* Reset counter to 0 after reaching target */
        (0 << 4) |  /* Target irq */
        (0 << 5) |  /* Overflow irq */
        (mode << 8)
    );
    write16(timerBase, 0); // Reset current value
    read16(timerBase + 4); // Reset reached bits

    readTimerValues();
    uint16_t status = read16(timerBase + 4);
    ExitCriticalSection();

    for (int i = 0; i <= target + 1; i++) {
        printf("counter == %2d, %5d ticks  %s\n", i, ticks[i], (i == target) ? "<<<< This value should be non-zero" : "");
    }
    bool reachedTarget = (status & (1<<11)) != 0;
    bool reachedFFFF = (status & (1<<12)) != 0;
    bool resetAfterTarget       = ticks[target]     != 0;
    bool targetPlus1NotReached  = ticks[target + 1] == 0;
    printf("Reached target:                      %d (%s)\n", reachedTarget,                  reachedTarget ? "ok" : "fail");
    printf("Reached 0xFFFF:                      %d (%s)\n", reachedFFFF,                     !reachedFFFF ? "ok" : "fail");
    printf("Counter reset AFTER reaching target: %d (%s)\n", resetAfterTarget,            resetAfterTarget ? "ok" : "fail");
    printf("Target + 1 not reached:              %d (%s)\n", targetPlus1NotReached,  targetPlus1NotReached ? "ok" : "fail");

    restoreTimer(timer, prevState);
}

void testTimerResetFlagWritable() {
    printf("\n\nCheck if write to mode register acknowledges the targetReached flag\n");

    const uint16_t target = 65535;
    const int timer = 0;
    const uint32_t timerBase = 0x1f801100 + (timer * 0x10);
    uint16_t prevState = read16(timerBase + 4);
     
    EnterCriticalSection();

    write16(timerBase + 8, target);

    write16(timerBase + 4, 0x0000);
    uint16_t readMode = read16(timerBase + 4);
    printf("Write 0x%04x to Timer0.mode, read 0x%04x\n", 0x0000, readMode);

    write16(timerBase + 4, 0xffff);
    readMode = read16(timerBase + 4);
    printf("Write 0x%04x to Timer0.mode, read 0x%04x\n", 0xffff, readMode);

uint16_t setMode = 
                    /* No sync */
        (1 << 3) |  /* Reset counter to 0 after reaching target */
        (0 << 4) |  /* Target irq */
        (0 << 5) |  /* Overflow irq */
        (0 << 8);    /* Sysclk  */
    write16(timerBase + 4, setMode);
    write16(timerBase, 0); // Reset current value
    read16(timerBase + 4); // Reset reached bits

    printf("\n");


    delay(100000);
    readMode = read16(timerBase + 4);
    uint16_t readMode2 = read16(timerBase + 4);
    printf("Read mode after waiting for targetReached: 0x%04x\n", readMode);
    printf("Read again: 0x%04x\n", readMode2);

    printf("\n");

    delay(100000);
    write16(timerBase + 4, setMode);
    readMode = read16(timerBase + 4);
    readMode2 = read16(timerBase + 4);
    printf("Write mode after waiting for targetReached and then read: 0x%04x\n", readMode);
    printf("Read again: 0x%04x\n", readMode2);

    restoreTimer(timer, prevState);
    ExitCriticalSection();
}

void runTestsForMode(int videoMode) {
    SetVideoMode(videoMode);
    printf("\nForced %s system.\n", (GetVideoMode() == MODE_NTSC) ? "NTSC" : "PAL");

    for (int timer = 0; timer < 3; timer++)
    {
        for (int mode = 0; mode < 2; mode++)
        {
            testTimerWithCyclesDelay(timer, (timer == 2) ? mode * 2 : mode, 1000);
            testTimerWithCyclesDelay(timer, (timer == 2) ? mode * 2 : mode, 5000);
            testTimerWithFrameDelay(timer, (timer == 2) ? mode * 2 : mode);
        }
    }

    // Only timer0 is dotclock dependent
    {
        const int timer = 0;
        for (int mode = 0; mode < 2; mode++) {
            for (int res = 0; res < 5; res++) {
                setDisplayResolution(resolutions[res], 240);

                const int delayCycles = 100;

                const int resW = disp.disp.w;
                const int resH = disp.disp.h;
                const char* freq = getDotclockFrequencyForCurrentResolution();
                printf("\nTesting Timer%d sync modes (clock source = %s, %d delay cycles, resolution = %dx%d, dotclock @ %s, not resetted between runs)\n", timer, modes[timer][mode], delayCycles, resW, resH, freq);
                for (int sync = 0; sync < 4; sync++) {
                    testTimerWithCyclesDelaySyncEnabled(timer, (timer == 2) ? mode * 2 : mode, sync, delayCycles);
                }
            }
        }
    }
    setDisplayResolution(320, 240);

    for (int timer = 1; timer <= 2; timer++) {
        for (int mode = 0; mode < 2; mode++) {
            const int delayCycles = 100;

            printf("\nTesting Timer%d sync modes (clock source = %s, %d delay cycles, not resetted between runs)\n", timer, modes[timer][mode], delayCycles);
            for (int sync = 0; sync < 4; sync++) {
                testTimerWithCyclesDelaySyncEnabled(timer, mode, sync, delayCycles);
            }
        }
    }

    testTimerTargetBehaviour();

    testTimerResetFlagWritable();
}

int main()
{
    printf("timers test\n");
    // printf("initializing Serial\n");
    // AddSIO(115200);
    initVideo();
    // FntLoad(SCREEN_XRES, 0);
 
    printf("Detected %s system.\n", (GetVideoMode() == MODE_NTSC) ? "NTSC" : "PAL");

    // EnterCriticalSection();
    runTestsForMode(MODE_NTSC);
    // ExitCriticalSection();
    // runTestsForMode(MODE_PAL);

    printf("\n\nDone.\n");
    for (;;) {
        VSync(0);
    }
    return 0;
}