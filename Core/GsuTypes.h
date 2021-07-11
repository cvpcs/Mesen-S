#pragma once
#include "stdafx.h"

struct GsuFlags
{
	bool Zero;
	bool Carry;
	bool Sign;
	bool Overflow;
	bool Running;
	bool RomReadPending;
	bool Alt1;
	bool Alt2;
	bool ImmLow;
	bool ImmHigh;
	bool Prefix;
	bool Irq;
	
	uint8_t GetFlagsLow()
	{
		return (
			(Zero << 1) |
			(Carry << 2) |
			(Sign << 3) |
			(Overflow << 4) |
			(Running << 5) |
			(RomReadPending << 6)
		);
	}

	uint8_t GetFlagsHigh()
	{
		return (
			(Alt1 << 0) |
			(Alt2 << 1) |
			(ImmLow << 2) |
			(ImmHigh << 3) |
			(Prefix << 4) |
			(Irq << 7)
		);
	}

	void SetFlags(uint16_t flags)
	{
		// 0 bit is clear
		Zero = flags & (1 << 1);
		Carry = flags & (1 << 2);
		Sign = flags & (1 << 3);
		Overflow = flags & (1 << 4);
		Running = flags & (1 << 5);
		RomReadPending = flags & (1 << 6);
		// 7 bit is clear

		Alt1 = flags & (1 << (8 + 0));
		Alt2 = flags & (1 << (8 + 1));
		ImmLow = flags & (1 << (8 + 2));
		ImmHigh = flags & (1 << (8 + 3));
		Prefix = flags & (1 << (8 + 4));
		// 5 bit is clear
		// 6 bit is clear
		Irq = flags & (1 << (8 + 7));
	}
};

struct GsuPixelCache
{
	uint8_t X;
	uint8_t Y;
	uint8_t Pixels[8];
	uint8_t ValidBits;
};

struct GsuState
{
	uint64_t CycleCount;

	uint16_t R[16];

	GsuFlags SFR;
	
	uint8_t RegisterLatch;

	uint8_t ProgramBank;
	uint8_t RomBank;
	uint8_t RamBank;

	bool IrqDisabled;
	bool HighSpeedMode;
	bool ClockSelect;
	bool BackupRamEnabled;
	uint8_t ScreenBase;
	
	uint8_t ColorGradient;
	uint8_t PlotBpp;
	uint8_t ScreenHeight;
	bool GsuRamAccess;
	bool GsuRomAccess;

	uint16_t CacheBase;

	bool PlotTransparent;
	bool PlotDither;
	bool ColorHighNibble;
	bool ColorFreezeHigh;
	bool ObjMode;

	uint8_t ColorReg;
	uint8_t SrcReg;
	uint8_t DestReg;
	
	uint8_t RomReadBuffer;
	uint8_t RomDelay;

	uint8_t ProgramReadBuffer;

	uint16_t RamWriteAddress;
	uint8_t RamWriteValue;
	uint8_t RamDelay;

	uint16_t RamAddress;
	
	GsuPixelCache PrimaryCache;
	GsuPixelCache SecondaryCache;
};