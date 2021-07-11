#include "stdafx.h"
#include "../Utilities/Serializer.h"
#include "CpuTypes.h"
#include "Sa1Cpu.h"
#include "Console.h"
#include "MemoryManager.h"
#include "EventType.h"
#include "Sa1.h"
#include "MemoryMappings.h"

#define Cpu Sa1Cpu
#include "Cpu.Instructions.h"
#include "Cpu.Shared.h"
#undef Cpu

Sa1Cpu::Sa1Cpu(Sa1* sa1, Console* console)
{
	_sa1 = sa1;
	_console = console;
}

Sa1Cpu::~Sa1Cpu()
{
}

void Sa1Cpu::Exec()
{
	_immediateMode = false;

	switch(_state.StopState) {
		case CpuStopState::Running: RunOp(); break;
		case CpuStopState::Stopped:
			//STP was executed, CPU no longer executes any code
			_state.CycleCount++;
			return;

		case CpuStopState::WaitingForIrq:
			//WAI
			Idle();
			if(_state.IrqSource || _state.NeedNmi) {
				Idle();
				_state.StopState = CpuStopState::Running;
			}
			break;
	}

	//Use the state of the IRQ/NMI flags on the previous cycle to determine if an IRQ is processed or not
	if(_state.PrevNeedNmi) {
		_state.NeedNmi = false;
		uint32_t originalPc = GetProgramAddress(_state.PC);
		ProcessInterrupt(_state.EmulationMode ? Sa1Cpu::LegacyNmiVector : Sa1Cpu::NmiVector, true);
		_console->ProcessInterrupt<CpuType::Sa1>(originalPc, GetProgramAddress(_state.PC), true);
	} else if(_state.PrevIrqSource) {
		uint32_t originalPc = GetProgramAddress(_state.PC);
		ProcessInterrupt(_state.EmulationMode ? Sa1Cpu::LegacyIrqVector : Sa1Cpu::IrqVector, true);
		_console->ProcessInterrupt<CpuType::Sa1>(originalPc, GetProgramAddress(_state.PC), false);
	}
}

void Sa1Cpu::Idle()
{
	//Do not apply any delay to internal cycles: "internal SA-1 cycles are still 10.74 MHz."
	_state.CycleCount++;
	DetectNmiSignalEdge();
	UpdateIrqNmiFlags();
}

void Sa1Cpu::IdleEndJump()
{
	IMemoryHandler* handler = _sa1->GetMemoryMappings()->GetHandler(_state.PC);
	if(handler && handler->GetMemoryType() == SnesMemoryType::PrgRom) {
		//Jumps/returns in PRG ROM take an extra cycle
		_state.CycleCount++;
		if(_sa1->GetSnesCpuMemoryType() == SnesMemoryType::PrgRom) {
			//Add an extra wait cycle if a conflict occurs at the same time
			_state.CycleCount++;
		}
	}
}

void Sa1Cpu::IdleTakeBranch()
{
	if(_state.PC & 0x01) {
		IMemoryHandler* handler = _sa1->GetMemoryMappings()->GetHandler(_state.PC);
		if(handler && handler->GetMemoryType() == SnesMemoryType::PrgRom) {
			//Branches to an odd address take an extra cycle
			_state.CycleCount++;
		}
	}
}

bool Sa1Cpu::IsAccessConflict()
{
	return _sa1->GetSnesCpuMemoryType() == _sa1->GetSa1MemoryType() && _sa1->GetSa1MemoryType() != SnesMemoryType::Register;
}

void Sa1Cpu::ProcessCpuCycle(uint32_t addr)
{
	_state.CycleCount++;

	if(_sa1->GetSa1MemoryType() == SnesMemoryType::SaveRam) {
		//BWRAM (save ram) access takes 2 cycles
		_state.CycleCount++;
		if(IsAccessConflict()) {
			_state.CycleCount += 2;
		}
	} else if(IsAccessConflict()) {
		//Add a wait cycle when a conflict occurs between both CPUs
		_state.CycleCount++;
		if(_sa1->GetSa1MemoryType() == SnesMemoryType::Sa1InternalRam && _sa1->IsSnesCpuFastRomSpeed()) {
			//If it's an IRAM access during FastROM access (speed = 6), add another wait cycle
			_state.CycleCount++;
		}
	}

	DetectNmiSignalEdge();
	UpdateIrqNmiFlags();
}

uint8_t Sa1Cpu::Read(uint32_t addr, MemoryOperationType type)
{
	ProcessCpuCycle(addr);
	return _sa1->ReadSa1(addr, type);
}

void Sa1Cpu::Write(uint32_t addr, uint8_t value, MemoryOperationType type)
{
	ProcessCpuCycle(addr);
	_sa1->WriteSa1(addr, value, type);
}

uint16_t Sa1Cpu::ReadVector(uint16_t vector)
{
	return _sa1->ReadVector(vector);
}

uint16_t Sa1Cpu::GetResetVector()
{
	return _sa1->ReadVector(Sa1Cpu::ResetVector);
}

void Sa1Cpu::IncreaseCycleCount(uint64_t cycleCount)
{
	_state.CycleCount += cycleCount;
}

void Sa1Cpu::SetReg(CpuRegister reg, uint16_t value)
{
	switch (reg) {
	case CpuRegister::CpuRegA:
	{
		_state.A = value;
	} break;
	case CpuRegister::CpuRegX:
	{
		_state.X = value;
	} break;
	case CpuRegister::CpuRegY:
	{
		_state.Y = value;
	} break;
	case CpuRegister::CpuRegSP:
	{
		_state.SP = value;
	} break;
	case CpuRegister::CpuRegD:
	{
		_state.D = value;
	} break;
	case CpuRegister::CpuRegPC:
	{
		_state.PC = value;
	} break;
	case CpuRegister::CpuRegK:
	{
		_state.K = value & 0xFF;
	} break;
	case CpuRegister::CpuRegDBR:
	{
		_state.DBR = value & 0xFF;
	} break;
	case CpuRegister::CpuRegPS:
	{
		_state.PS = value & 0xFF;
	} break;
	}
}
