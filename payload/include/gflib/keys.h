#ifndef GFLIB_KEYS_H
#define GFLIB_KEYS_H

extern u16 gHeldKeys;

void ReadKeys(void);
void SetKeyRepeatTiming(u16 delay, u16 rate);

#endif //GFLIB_KEYS_H
