#ifndef LIBPMAGB_TRADE_H
#define LIBPMAGB_TRADE_H

struct UnkStruct_02023F50
{
    u32 hasPokedex:1;
    u32 isContinueGameWarp:1;
    u32 isFRLG:1;
    u32 isChampionSaveWarp:1;
    u32 language:4;
    u32 saveStatus:2;
    u8 playerName[8];
    u32 playerGender;
    u8 playerId[4];
    struct Pokemon party[6];
    u8 giftRibbons[11];
};

#endif //LIBPMAGB_TRADE_H
