﻿using System;
using System.Runtime.InteropServices;

namespace Mesen.GUI.Debugger
{
	public struct InteropBreakpoint
	{
		public CpuType CpuType;
		public SnesMemoryType MemoryType;
		public BreakpointTypeFlags Type;
		public Int32 StartAddress;
		public Int32 EndAddress;

		[MarshalAs(UnmanagedType.I1)]
		public bool Enabled;

		[MarshalAs(UnmanagedType.I1)]
		public bool MarkEvent;
		
		[MarshalAs(UnmanagedType.ByValArray, SizeConst = 1000)]
		public byte[] Condition;
	}
}
