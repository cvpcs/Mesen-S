#pragma once
#include "stdafx.h"

enum class CpuType : uint8_t; 
enum class SnesMemoryType;
struct AddressInfo;
enum class BreakpointType;
enum class BreakpointTypeFlags;
enum class BreakpointCategory;

class Breakpoint
{
public:
	bool Matches(uint32_t memoryAddr, AddressInfo &info);
	bool HasBreakpointType(BreakpointType bpType);
	string GetCondition();
	bool HasCondition();

	CpuType GetCpuType();
	bool IsEnabled();
	bool IsMarked();

	CpuType cpuType;
	SnesMemoryType memoryType;
	BreakpointTypeFlags type;
	int32_t startAddr;
	int32_t endAddr;
	bool enabled;
	bool markEvent;
	char condition[1000];
};