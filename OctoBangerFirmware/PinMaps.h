/*
 * PinMaps.h
 */
#ifndef _PINMAPS_H
#define _PINMAPS_H

enum {
   DEFAULT_TTL,
   SHIELD,
   CUSTOM,
   PIN_MAP_COUNT
};

// Each TTL pin map starts with eight output pins.  The last three pins are:
//   1) Tr-in (Trigger-in) Pin
//   3) Tr-out (Trigger-out) Pin
//   4) MP-out (Media Player Serial Out) Pin 

enum {D0,D1,D2,D3,D4,D5,D6,D7,D8,D9,D10,D11,D12,D13};

int8_t PIN_MAPS[PIN_MAP_COUNT][11] = {
//  <------------ Output Pins ------------>  Tr-in  Tr-out  MP-out
   { D2,  D3,  D4,  D5,  D6,  D7,  D8,  D9,    D11,    D10,    D12},  // Default_TTL
   { D7,  D6,  D5,  D4,  D8,  D9, D10, D11,    A0,    A1,    A2},  // Shield 
   {  0,   0,   0,   0,   0,   0,   0,   0,    0,     0,     0}   // Custom
};

#endif // _PINMAPS_H
