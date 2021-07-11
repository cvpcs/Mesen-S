﻿using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows.Forms;

namespace Mesen.GUI.Debugger
{
	public class BreakpointManager
	{
		public static event EventHandler BreakpointsChanged;

		private static List<Breakpoint> _breakpoints = new List<Breakpoint>();
		private static HashSet<CpuType> _activeCpuTypes = new HashSet<CpuType>();

		public static ReadOnlyCollection<Breakpoint> Breakpoints
		{
			get { return _breakpoints.ToList().AsReadOnly(); }
		}

		public static List<Breakpoint> Asserts { internal get; set; }

		public static void AddCpuType(CpuType cpuType)
		{
			_activeCpuTypes.Add(cpuType);
			SetBreakpoints();
		}

		public static void RemoveCpuType(CpuType cpuType)
		{
			_activeCpuTypes.Remove(cpuType);
			SetBreakpoints();
		}

		public static void RefreshBreakpoints(Breakpoint bp = null)
		{
			if(BreakpointsChanged != null) {
				BreakpointsChanged(bp, null);
			}

			SetBreakpoints();
		}

		public static void SetBreakpoints(List<Breakpoint> breakpoints)
		{
			_breakpoints = breakpoints.ToList();
			RefreshBreakpoints();
		}

		public static void EditBreakpoint(Breakpoint bp)
		{
			if(new frmBreakpoint(bp).ShowDialog() == DialogResult.OK) {
				if(!_breakpoints.Contains(bp)) {
					_breakpoints.Add(bp);
				}
				RefreshBreakpoints(bp);
			}
		}

		public static void RemoveBreakpoint(Breakpoint bp)
		{
			_breakpoints.Remove(bp);
			RefreshBreakpoints(bp);
		}

		public static void AddBreakpoint(Breakpoint bp)
		{
			_breakpoints.Add(bp);
			RefreshBreakpoints(bp);
		}

		public static Breakpoint GetMatchingBreakpoint(AddressInfo info, CpuType cpuType)
		{
			return Breakpoints.Where((bp) => bp.Matches((UInt32)info.Address, info.Type, cpuType)).FirstOrDefault();
		}

		public static Breakpoint GetMatchingBreakpoint(UInt32 startAddress, UInt32 endAddress, SnesMemoryType memoryType)
		{
			bool isAddressRange = startAddress != endAddress;
			return Breakpoints.Where((bp) =>
					bp.MemoryType == memoryType &&
					((!isAddressRange && bp.Address == startAddress) || (isAddressRange && bp.StartAddress == startAddress && bp.EndAddress == endAddress))
				).FirstOrDefault();
		}

		public static bool EnableDisableBreakpoint(AddressInfo info, CpuType cpuType)
		{
			Breakpoint breakpoint = BreakpointManager.GetMatchingBreakpoint(info, cpuType);
			if(breakpoint != null) {
				breakpoint.SetEnabled(!breakpoint.Enabled);
				return true;
			}
			return false;
		}

		public static void ToggleBreakpoint(AddressInfo info, CpuType cpuType)
		{
			if(info.Address < 0) {
				return;
			}

			Breakpoint breakpoint = BreakpointManager.GetMatchingBreakpoint(info, cpuType);
			if(breakpoint != null) {
				BreakpointManager.RemoveBreakpoint(breakpoint);
			} else {
				breakpoint = new Breakpoint() {
					CpuType = cpuType,
					Enabled = true,
					BreakOnExec = true,
					Address = (UInt32)info.Address
				};

				if(info.Type != SnesMemoryType.PrgRom) {
					breakpoint.BreakOnRead = true;
					breakpoint.BreakOnWrite = true;
				}

				breakpoint.MemoryType = info.Type;
				BreakpointManager.AddBreakpoint(breakpoint);
			}
		}

		public static void SetBreakpoints()
		{
			List<InteropBreakpoint> breakpoints = new List<InteropBreakpoint>();

			ReadOnlyCollection<Breakpoint> userBreakpoints = BreakpointManager.Breakpoints;
			for(int i = 0; i < userBreakpoints.Count; i++) {
				if(_activeCpuTypes.Contains(userBreakpoints[i].CpuType)) {
					breakpoints.Add(userBreakpoints[i].ToInteropBreakpoint());
				}
			}

			List<Breakpoint> assertBreakpoints = BreakpointManager.Asserts;
			for(int i = 0; i < assertBreakpoints.Count; i++) {
				if(_activeCpuTypes.Contains(assertBreakpoints[i].CpuType)) {
					breakpoints.Add(assertBreakpoints[i].ToInteropBreakpoint());
				}
			}

			DebugApi.SetBreakpoints(breakpoints.ToArray(), (UInt32)breakpoints.Count);
		}

		public static Breakpoint GetBreakpointById(int breakpointId)
		{
			if(breakpointId < 0) {
				return null;
			} else if(breakpointId < _breakpoints.Count) {
				return _breakpoints[breakpointId];
			} else if(breakpointId < _breakpoints.Count + Asserts.Count) {
				return Asserts[breakpointId - _breakpoints.Count];
			}
			return null;
		}
	}
}
