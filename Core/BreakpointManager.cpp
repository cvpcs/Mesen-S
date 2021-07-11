#include "stdafx.h"
#include "BreakpointManager.h"
#include "DebugTypes.h"
#include "Debugger.h"
#include "Breakpoint.h"
#include "DebugUtilities.h"
#include "ExpressionEvaluator.h"
#include "EventManager.h"

BreakpointManager::BreakpointManager(Debugger *debugger, CpuType cpuType, IEventManager* eventManager)
{
	_debugger = debugger;
	_cpuType = cpuType;
	_hasBreakpoint = false;
	_eventManager = eventManager ? eventManager : debugger->GetEventManager(cpuType).get();
}

void BreakpointManager::SetBreakpoints(Breakpoint breakpoints[], uint32_t count)
{
	_hasBreakpoint = false;
	for(int i = 0; i < BreakpointManager::BreakpointTypeCount; i++) {
		_breakpoints[i].clear();
		_rpnList[i].clear();
		_hasBreakpointType[i] = false;
	}

	_bpExpEval.reset(new ExpressionEvaluator(_debugger, _cpuType));

	for(uint32_t j = 0; j < count; j++) {
		Breakpoint &bp = breakpoints[j];
		for(int i = 0; i < BreakpointManager::BreakpointTypeCount; i++) {
			BreakpointType bpType = (BreakpointType)i;
			if((bp.IsMarked() || bp.IsEnabled()) && bp.HasBreakpointType(bpType)) {
				CpuType cpuType = bp.GetCpuType();
				if(_cpuType != cpuType) {
					continue;
				}

				int curIndex = _breakpoints[i].size();
				_breakpoints[i].insert(std::make_pair(curIndex, bp));

				if(bp.HasCondition()) {
					bool success = true;
					ExpressionData data = _bpExpEval->GetRpnList(bp.GetCondition(), success);
					_rpnList[i].insert(std::make_pair(curIndex, success ? data : ExpressionData()));
				} else {
					_rpnList[i].insert(std::make_pair(curIndex, ExpressionData()));
				}
				
				_hasBreakpoint = true;
				_hasBreakpointType[i] = true;
			}
		}
	}
}

void BreakpointManager::GetBreakpoints(Breakpoint* breakpoints, int& execs, int& reads, int& writes)
{
	execs = _breakpoints[static_cast<int>(BreakpointType::Execute)].size();
	reads = _breakpoints[static_cast<int>(BreakpointType::Read)].size();
	writes = _breakpoints[static_cast<int>(BreakpointType::Write)].size();

	if (breakpoints == NULL) {
		return;
	}

	int offset = 0;
	for (auto it = _breakpoints[static_cast<int>(BreakpointType::Execute)].cbegin(); it != _breakpoints[static_cast<int>(BreakpointType::Execute)].cend(); it++) {
		breakpoints[offset++] = it->second;
	}

	for (auto it = _breakpoints[static_cast<int>(BreakpointType::Read)].cbegin(); it != _breakpoints[static_cast<int>(BreakpointType::Read)].cend(); it++) {
		breakpoints[offset++] = it->second;
	}

	for (auto it = _breakpoints[static_cast<int>(BreakpointType::Write)].cbegin(); it != _breakpoints[static_cast<int>(BreakpointType::Write)].cend(); it++) {
		breakpoints[offset++] = it->second;
	}
}

BreakpointType BreakpointManager::GetBreakpointType(MemoryOperationType type)
{
	switch(type) {
		default:
		case MemoryOperationType::ExecOperand:
		case MemoryOperationType::ExecOpCode:
			return BreakpointType::Execute;

		case MemoryOperationType::DmaRead:
		case MemoryOperationType::Read:
			return BreakpointType::Read;

		case MemoryOperationType::DmaWrite:
		case MemoryOperationType::Write:
			return BreakpointType::Write;
	}
}

int BreakpointManager::InternalCheckBreakpoint(MemoryOperationInfo operationInfo, AddressInfo &address)
{
	BreakpointType type = GetBreakpointType(operationInfo.Type);

	if(!_hasBreakpointType[(int)type]) {
		return -1;
	}

	DebugState state;
	_debugger->GetState(state, false);
	EvalResultType resultType;
	unordered_map<int, Breakpoint> &breakpoints = _breakpoints[(int)type];
	for (auto it = breakpoints.begin(); it != breakpoints.end(); it++) {
		if (it->second.Matches(operationInfo.Address, address)) {
			if (!it->second.HasCondition() || _bpExpEval->Evaluate(_rpnList[(int)type][it->first], state, resultType, operationInfo)) {
				if (it->second.IsMarked()) {
					_eventManager->AddEvent(DebugEventType::Breakpoint, operationInfo, it->first);
				}
				if (it->second.IsEnabled()) {
					return it->first;
				}
			}
		}
	}

	return -1;
}
