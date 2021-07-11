#include "stdafx.h"
#include "Cx4.h"
#include "Console.h"
#include "Cpu.h"
#include "MemoryManager.h"
#include "MemoryMappings.h"
#include "BaseCartridge.h"
#include "EmuSettings.h"
#include "RamHandler.h"
#include "../Utilities/HexUtilities.h"

//TODO: Proper open bus behavior (and return 0s for missing save ram, too)
//TODO: CPU shouldn't have access to PRG ROM while the CX4 is loading from PRG ROM
//TODO: Timings are apparently not perfect (desync in MMX2 intro)

Cx4::Cx4(Console* console) : BaseCoprocessor(SnesMemoryType::Register)
{
	_console = console;
	_memoryType = SnesMemoryType::Register;
	_memoryManager = console->GetMemoryManager().get();
	_cpu = console->GetCpu().get();
	
	console->GetSettings()->InitializeRam(_dataRam, Cx4::DataRamSize);
	
	auto &prgRomHandlers = console->GetCartridge()->GetPrgRomHandlers();
	auto &saveRamHandlers = console->GetCartridge()->GetSaveRamHandlers();
	MemoryMappings* cpuMappings = _memoryManager->GetMemoryMappings();

	//PRG ROM
	uint8_t bankCount = console->GetSettings()->GetEmulationConfig().EnableStrictBoardMappings ? 0x3F : 0x7F;
	cpuMappings->RegisterHandler(0x00, std::min<uint8_t>(0x7D, bankCount), 0x8000, 0xFFFF, prgRomHandlers);
	cpuMappings->RegisterHandler(0x80, 0x80 + bankCount, 0x8000, 0xFFFF, prgRomHandlers);
	_mappings.RegisterHandler(0x00, bankCount, 0x8000, 0xFFFF, prgRomHandlers);
	_mappings.RegisterHandler(0x80, 0x80 + bankCount, 0x8000, 0xFFFF, prgRomHandlers);

	//Save RAM
	cpuMappings->RegisterHandler(0x70, 0x7D, 0x0000, 0x7FFF, saveRamHandlers);
	cpuMappings->RegisterHandler(0xF0, 0xFF, 0x0000, 0x7FFF, saveRamHandlers);
	_mappings.RegisterHandler(0x70, 0x7D, 0x0000, 0x7FFF, saveRamHandlers);
	_mappings.RegisterHandler(0xF0, 0xFF, 0x0000, 0x7FFF, saveRamHandlers);

	//Registers
	cpuMappings->RegisterHandler(0x00, 0x3F, 0x6000, 0x7FFF, this);
	cpuMappings->RegisterHandler(0x80, 0xBF, 0x6000, 0x7FFF, this);
	_mappings.RegisterHandler(0x00, 0x3F, 0x6000, 0x7FFF, this);
	_mappings.RegisterHandler(0x80, 0xBF, 0x6000, 0x7FFF, this);

	_clockRatio = (double)20000000 / console->GetMasterClockRate();
	Reset();
}

void Cx4::Reset()
{
	_state = {};
	_state.Stopped = true;
	_state.SingleRom = true;
	_state.RomAccessDelay = 3;
	_state.RamAccessDelay = 3;
}

void Cx4::Run()
{
	uint64_t targetCycle = (uint64_t)(_memoryManager->GetMasterClock() * _clockRatio);

	while(_state.CycleCount < targetCycle) {
		if(_state.Locked) {
			Step(1);
		} else if(_state.Suspend.Enabled) {
			if(_state.Suspend.Duration == 0) {
				Step(1);
			} else {
				Step(1);
				_state.Suspend.Duration--;
				if(_state.Suspend.Duration == 0) {
					_state.Suspend.Enabled = false;
				}
			}
		} else if(_state.Cache.Enabled) {
			ProcessCache(targetCycle);
		} else if(_state.Dma.Enabled) {
			ProcessDma(targetCycle);
		} else if(_state.Stopped) {
			Step(targetCycle - _state.CycleCount);
		} else if(!ProcessCache(targetCycle)) {
			if(!_state.Cache.Enabled) {
				//Cache operation required, but both caches are locked, stop
				Stop();
			}
		} else {
			uint16_t opCode = _prgRam[_state.Cache.Page][_state.PC];
			_console->ProcessMemoryRead<CpuType::Cx4>(0, 0, MemoryOperationType::ExecOpCode);
			_state.PC++;
			
			if(_state.PC == 0) {
				//If execution reached the end of the page, start loading the next page
				//This must be done BEFORE running the instruction (otherwise a jump/branch to address 0 will trigger this)
				SwitchCachePage();
			}

			Exec(opCode);
		}
	}
}

void Cx4::Step(uint64_t cycles)
{
	if(_state.Bus.Enabled) {
		if(_state.Bus.DelayCycles > cycles) {
			_state.Bus.DelayCycles -= (uint8_t)cycles;
		} else {
			_state.Bus.Enabled = false;
			_state.Bus.DelayCycles = 0;

			if(_state.Bus.Reading) {
				_state.MemoryDataReg = ReadCx4(_state.Bus.Address);
				_state.Bus.Reading = false;
			}

			if(_state.Bus.Writing) {
				WriteCx4(_state.Bus.Address, _state.MemoryDataReg);
				_state.Bus.Writing = false;
			}
		}
	}

	_state.CycleCount += cycles;
}

void Cx4::SwitchCachePage()
{
	if(_state.Cache.Page == 1) {
		Stop();
		return;
	}

	_state.Cache.Page = 1;
	if(_state.Cache.Lock[1]) {
		Stop();
		return;
	} 

	_state.PB = _state.P;

	uint64_t targetCycle = (uint64_t)(_memoryManager->GetMasterClock() * _clockRatio);
	if(!ProcessCache(targetCycle) && !_state.Cache.Enabled) {
		Stop();
	}
}

bool Cx4::ProcessCache(uint64_t targetCycle)
{
	uint32_t address = (_state.Cache.Base + (_state.PB << 9)) & 0xFFFFFF;

	if(_state.Cache.Pos == 0) {
		if(_state.Cache.Address[_state.Cache.Page] == address) {
			//Current cache page matches the needed address, keep using it
			_state.Cache.Enabled = false;
			return true;
		}

		//Check if the other page matches
		_state.Cache.Page ^= 1;

		if(_state.Cache.Address[_state.Cache.Page] == address) {
			//The other cache page matches, use it
			_state.Cache.Enabled = false;
			return true;
		}

		if(_state.Cache.Lock[_state.Cache.Page]) {
			//If it's locked, use the other page
			_state.Cache.Page ^= 1;
		}

		if(_state.Cache.Lock[_state.Cache.Page]) {
			//The both pages are locked, and the cache is invalid, give up.
			_state.Cache.Enabled = false;
			return false;
		}
	
		_state.Cache.Enabled = true;
	}

	//Populate the cache
	while(_state.Cache.Pos < 256) {
		uint8_t lsb = ReadCx4(address + (_state.Cache.Pos * 2));
		Step(GetAccessDelay(address + (_state.Cache.Pos * 2)));

		uint8_t msb = ReadCx4(address + (_state.Cache.Pos * 2) + 1);
		Step(GetAccessDelay(address + (_state.Cache.Pos * 2) + 1));

		_prgRam[_state.Cache.Page][_state.Cache.Pos] = (msb << 8) | lsb;
		_state.Cache.Pos++;

		if(_state.CycleCount > targetCycle) {
			break;
		}
	}

	if(_state.Cache.Pos >= 256) {
		_state.Cache.Address[_state.Cache.Page] = address;
		_state.Cache.Pos = 0;
		_state.Cache.Enabled = false;
		return true;
	}

	//Cache loading is not finished
	return false;
}

void Cx4::ProcessDma(uint64_t targetCycle)
{
	while(_state.Dma.Pos < _state.Dma.Length) {
		uint32_t src = (_state.Dma.Source + _state.Dma.Pos) & 0xFFFFFF;
		uint32_t dest = (_state.Dma.Dest + _state.Dma.Pos) & 0xFFFFFF;

		IMemoryHandler *srcHandler = _mappings.GetHandler(src);
		IMemoryHandler *destHandler = _mappings.GetHandler(dest);
		if(!srcHandler || !destHandler || srcHandler->GetMemoryType() == destHandler->GetMemoryType() || destHandler->GetMemoryType() == SnesMemoryType::PrgRom) {
			//Invalid DMA, the chip is locked until it gets restarted by a write to $7F53
			_state.Locked = true;
			_state.Dma.Pos = 0;
			_state.Dma.Enabled = false;
			return;
		}

		Step(GetAccessDelay(src));
		uint8_t value = ReadCx4(src);

		Step(GetAccessDelay(dest));
		WriteCx4(dest, value);
		_state.Dma.Pos++;

		if(_state.CycleCount > targetCycle) {
			break;
		}
	}

	if(_state.Dma.Pos >= _state.Dma.Length) {
		_state.Dma.Pos = 0;
		_state.Dma.Enabled = false;
	}
}

uint8_t Cx4::GetAccessDelay(uint32_t addr)
{
	IMemoryHandler* handler = _mappings.GetHandler(addr);
	if(handler->GetMemoryType() == SnesMemoryType::PrgRom) {
		return 1 + _state.RomAccessDelay;
	} else if(handler->GetMemoryType() == SnesMemoryType::SaveRam) {
		return 1 + _state.RamAccessDelay;
	}

	return 1;
}

uint8_t Cx4::ReadCx4(uint32_t addr)
{
	IMemoryHandler* handler = _mappings.GetHandler(addr);
	if(handler) {
		uint8_t value = handler->Read(addr);
		_console->ProcessMemoryRead<CpuType::Cx4>(addr, value, MemoryOperationType::Read);
		return value;
	}
	return 0;
}

void Cx4::WriteCx4(uint32_t addr, uint8_t value)
{
	IMemoryHandler* handler = _mappings.GetHandler(addr);
	if(handler) {
		_console->ProcessMemoryWrite<CpuType::Cx4>(addr, value, MemoryOperationType::Write);
		handler->Write(addr, value);
	}
}

uint8_t Cx4::Read(uint32_t addr)
{
	addr = 0x7000 | (addr & 0xFFF);
	if(addr <= 0x7BFF) {
		return _dataRam[addr & 0xFFF];
	} else if(addr >= 0x7F60 && addr <= 0x7F7F) {
		return _state.Vectors[addr & 0x1F];
	} else if((addr >= 0x7F80 && addr <= 0x7FAF) || (addr >= 0x7FC0 && addr <= 0x7FEF)) {
		addr &= 0x3F;
		uint32_t &reg = _state.Regs[addr / 3];
		switch(addr % 3) {
			case 0: return reg;
			case 1: return reg >> 8;
			case 2: return reg >> 16;
		}
	} else if(addr >= 0x7F53 && addr <= 0x7F5F) {
		return (uint8_t)_state.Suspend.Enabled | ((uint8_t)_state.IrqFlag << 1) | ((uint8_t)IsRunning() << 6) | ((uint8_t)IsBusy() << 7);
	}

	switch(addr) {
		case 0x7F40: return _state.Dma.Source;
		case 0x7F41: return _state.Dma.Source >> 8;
		case 0x7F42: return _state.Dma.Source >> 16;
		case 0x7F43: return (uint8_t)_state.Dma.Length;
		case 0x7F44: return _state.Dma.Length >> 8;
		case 0x7F45: return _state.Dma.Dest;
		case 0x7F46: return _state.Dma.Dest >> 8;
		case 0x7F47: return _state.Dma.Dest >> 16;
		case 0x7F48: return _state.Cache.Page;
		case 0x7F49: return _state.Cache.Base;
		case 0x7F4A: return _state.Cache.Base >> 8;
		case 0x7F4B: return _state.Cache.Base >> 16;
		case 0x7F4C: return (uint8_t)_state.Cache.Lock[0] | ((uint8_t)_state.Cache.Lock[1] << 1);
		case 0x7F4D: return (uint8_t)_state.Cache.ProgramBank;
		case 0x7F4E: return _state.Cache.ProgramBank >> 8;
		case 0x7F4F: return _state.Cache.ProgramCounter;
		case 0x7F50: return _state.RamAccessDelay | (_state.RomAccessDelay << 4);
		case 0x7F51: return _state.IrqDisabled;
		case 0x7F52: return _state.SingleRom;
	}

	return 0;
}

void Cx4::Write(uint32_t addr, uint8_t value)
{
	addr = 0x7000 | (addr & 0xFFF);

	if(addr <= 0x7BFF) {
		_dataRam[addr & 0xFFF] = value;
		return;
	} 
	
	if(addr >= 0x7F60 && addr <= 0x7F7F) {
		_state.Vectors[addr & 0x1F] = value;
	} else if((addr >= 0x7F80 && addr <= 0x7FAF) || (addr >= 0x7FC0 && addr <= 0x7FEF)) {
		addr &= 0x3F;
		uint32_t &reg = _state.Regs[addr / 3];
		switch(addr % 3) {
			case 0: reg = (reg & 0xFFFF00) | value; break;
			case 1: reg = (reg & 0xFF00FF) | (value << 8); break;
			case 2: reg = (reg & 0x00FFFF) | (value << 16); break;
		}
	} else if(addr >= 0x7F55 && addr <= 0x7F5C) {
		_state.Suspend.Enabled = true;
		_state.Suspend.Duration = (addr - 0x7F55) * 32;
	} else {
		switch(addr) {
			case 0x7F40: _state.Dma.Source = (_state.Dma.Source & 0xFFFF00) | value; break;
			case 0x7F41: _state.Dma.Source = (_state.Dma.Source & 0xFF00FF) | (value << 8); break;
			case 0x7F42: _state.Dma.Source = (_state.Dma.Source & 0x00FFFF) | (value << 16); break;
			case 0x7F43: _state.Dma.Length = (_state.Dma.Length & 0xFF00) | value; break;
			case 0x7F44: _state.Dma.Length = (_state.Dma.Length & 0x00FF) | (value << 8); break;
			case 0x7F45: _state.Dma.Dest = (_state.Dma.Dest & 0xFFFF00) | value; break;
			case 0x7F46: _state.Dma.Dest = (_state.Dma.Dest & 0xFF00FF) | (value << 8); break;
			case 0x7F47:
				_state.Dma.Dest = (_state.Dma.Dest & 0x00FFFF) | (value << 16);
				if(_state.Stopped) {
					_state.Dma.Enabled = true;
				}
				break;

			case 0x7F48:
				_state.Cache.Page = value & 0x01;
				if(_state.Stopped) {
					_state.Cache.Enabled = true;
				}
				break;

			case 0x7F49: _state.Cache.Base = (_state.Cache.Base & 0xFFFF00) | value; break;
			case 0x7F4A: _state.Cache.Base = (_state.Cache.Base & 0xFF00FF) | (value << 8); break;
			case 0x7F4B: _state.Cache.Base = (_state.Cache.Base & 0x00FFFF) | (value << 16); break;

			case 0x7F4C:
				_state.Cache.Lock[0] = (value & 0x01) != 0;
				_state.Cache.Lock[1] = (value & 0x02) != 0;
				break;

			case 0x7F4D: _state.Cache.ProgramBank = (_state.Cache.ProgramBank & 0xFF00) | value; break;
			case 0x7F4E: _state.Cache.ProgramBank = (_state.Cache.ProgramBank & 0x00FF) | ((value & 0x7F) << 8); break;

			case 0x7F4F:
				_state.Cache.ProgramCounter = value;
				if(_state.Stopped) {
					_state.Stopped = false;
					_state.PB = _state.Cache.ProgramBank;
					_state.PC = _state.Cache.ProgramCounter;
				}
				break;

			case 0x7F50:
				_state.RamAccessDelay = value & 0x07;
				_state.RomAccessDelay = (value >> 4) & 0x07;
				break;

			case 0x7F51:
				_state.IrqDisabled = value & 0x01;
				if(_state.IrqDisabled) {
					_state.IrqFlag = true;
					_cpu->ClearIrqSource(IrqSource::Coprocessor);
				}
				break;

			case 0x7F52: _state.SingleRom = (value & 0x01) != 0; break;

			case 0x7F53:
				_state.Locked = false;
				_state.Stopped = true;
				break;

			case 0x7F5D: _state.Suspend.Enabled = false; break;

			case 0x7F5E: 
				//Clear IRQ flag in CX4, but keeps IRQ signal high
				_state.IrqFlag = false; 
				break;
		}
	}
}

bool Cx4::IsRunning()
{
	return IsBusy() || !_state.Stopped;
}

bool Cx4::IsBusy()
{
	return _state.Cache.Enabled || _state.Dma.Enabled || _state.Bus.DelayCycles > 0;
}

void Cx4::Serialize(Serializer &s)
{
	s.Stream(
		_state.CycleCount, _state.PB, _state.PC, _state.A, _state.P, _state.SP, _state.Mult, _state.RomBuffer,
		_state.RamBuffer[0], _state.RamBuffer[1], _state.RamBuffer[2], _state.MemoryDataReg, _state.MemoryAddressReg,
		_state.DataPointerReg, _state.Negative, _state.Zero, _state.Carry, _state.Overflow, _state.IrqFlag, _state.Stopped,
		_state.Locked, _state.IrqDisabled, _state.SingleRom, _state.RamAccessDelay, _state.RomAccessDelay, _state.Bus.Address,
		_state.Bus.DelayCycles, _state.Bus.Enabled, _state.Bus.Reading, _state.Bus.Writing, _state.Dma.Dest, _state.Dma.Enabled,
		_state.Dma.Length, _state.Dma.Source, _state.Dma.Pos, _state.Suspend.Duration, _state.Suspend.Enabled, _state.Cache.Enabled,
		_state.Cache.Lock[0], _state.Cache.Lock[1], _state.Cache.Address[0], _state.Cache.Address[1], _state.Cache.Base,
		_state.Cache.Page, _state.Cache.ProgramBank, _state.Cache.ProgramCounter, _state.Cache.Pos
	);
	
	s.StreamArray(_state.Stack, 8);
	s.StreamArray(_state.Regs, 16);
	s.StreamArray(_state.Vectors, 0x20);
	s.StreamArray(_prgRam[0], 256);
	s.StreamArray(_prgRam[1], 256);
	s.StreamArray(_dataRam, Cx4::DataRamSize);
}

uint8_t Cx4::Peek(uint32_t addr)
{
	return 0;
}

void Cx4::PeekBlock(uint32_t addr, uint8_t* output)
{
	memset(output, 0, 0x1000);
}

AddressInfo Cx4::GetAbsoluteAddress(uint32_t address)
{
	return { -1, SnesMemoryType::Register };
}

MemoryMappings* Cx4::GetMemoryMappings()
{
	return &_mappings;
}

uint8_t* Cx4::DebugGetDataRam()
{
	return _dataRam;
}

uint32_t Cx4::DebugGetDataRamSize()
{
	return Cx4::DataRamSize;
}

Cx4State Cx4::GetState()
{
	return _state;
}

void Cx4::SetReg(Cx4Register reg, uint32_t value)
{
	switch (reg)
	{
	case Cx4Register::Cx4Reg0: case Cx4Register::Cx4Reg1: case Cx4Register::Cx4Reg2: case Cx4Register::Cx4Reg3:
	case Cx4Register::Cx4Reg4: case Cx4Register::Cx4Reg5: case Cx4Register::Cx4Reg6: case Cx4Register::Cx4Reg7:
	case Cx4Register::Cx4Reg8: case Cx4Register::Cx4Reg9: case Cx4Register::Cx4Reg10: case Cx4Register::Cx4Reg11:
	case Cx4Register::Cx4Reg12: case Cx4Register::Cx4Reg13: case Cx4Register::Cx4Reg14: case Cx4Register::Cx4Reg15:
		_state.Regs[(static_cast<int>(reg) - static_cast<int>(Cx4Register::Cx4Reg0)) & 0x0F] = value & 0xFFFFFF; // 24-bit
		break;
	case Cx4Register::Cx4RegPB:
	{
		_state.PB = value & 0xFFFF;
	} break;
	case Cx4Register::Cx4RegPC:
	{
		_state.PC = value & 0xFF;
	} break;
	case Cx4Register::Cx4RegA:
	{
		_state.A = value & 0xFFFFFF; // 24-bit
	} break;
	case Cx4Register::Cx4RegP:
	{
		_state.P = value & 0xFFFF;
	} break;
	case Cx4Register::Cx4RegSP:
	{
		_state.SP = value & 0xFF;
	} break;
	}
}