using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using Mesen.GUI.Controls;
using Mesen.GUI.Forms;
using System.Globalization;

namespace Mesen.GUI.Debugger.Controls
{
	public partial class ctrlCpuStatus : BaseControl
	{
		private EntityBinder _cpuBinder = new EntityBinder();
		private CpuState _lastState;

		public ctrlCpuStatus()
		{
			InitializeComponent();
			if(IsDesignMode) {
				return;
			}

			_cpuBinder.Entity = new CpuState();
			_cpuBinder.AddBinding(nameof(CpuState.A), txtA, onEditHandler: (s, e) => { DebugApi.SetCpuRegister(CpuRegister.CpuRegA, UInt16.Parse(txtA.Text, NumberStyles.HexNumber)); });
			_cpuBinder.AddBinding(nameof(CpuState.X), txtX, onEditHandler: (s, e) => { DebugApi.SetCpuRegister(CpuRegister.CpuRegX, UInt16.Parse(txtX.Text, NumberStyles.HexNumber)); });
			_cpuBinder.AddBinding(nameof(CpuState.Y), txtY, onEditHandler: (s, e) => { DebugApi.SetCpuRegister(CpuRegister.CpuRegY, UInt16.Parse(txtY.Text, NumberStyles.HexNumber)); });
			_cpuBinder.AddBinding(nameof(CpuState.D), txtD, onEditHandler: (s, e) => { DebugApi.SetCpuRegister(CpuRegister.CpuRegD, UInt16.Parse(txtD.Text, NumberStyles.HexNumber)); });
			_cpuBinder.AddBinding(nameof(CpuState.DBR), txtDB, onEditHandler: (s, e) => { DebugApi.SetCpuRegister(CpuRegister.CpuRegDBR, UInt16.Parse(txtDB.Text, NumberStyles.HexNumber)); });
			_cpuBinder.AddBinding(nameof(CpuState.SP), txtS, onEditHandler: (s, e) => { DebugApi.SetCpuRegister(CpuRegister.CpuRegSP, UInt16.Parse(txtS.Text, NumberStyles.HexNumber)); });
			_cpuBinder.AddBinding(nameof(CpuState.PS), txtP, onEditHandler: (s, e) => { DebugApi.SetCpuRegister(CpuRegister.CpuRegPS, UInt16.Parse(txtP.Text, NumberStyles.HexNumber)); });

			_cpuBinder.AddBinding(nameof(CpuState.NmiFlag), chkNmi, onEditHandler: (s, e) => { DebugApi.SetCpuRegister(CpuRegister.CpuFlagNmi, (UInt16)(chkNmi.Checked ? 1 : 0)); });

			chkIndex.CheckedChanged += (s, e) => { DebugApi.SetCpuProcFlag(ProcFlags.IndexMode8, chkIndex.Checked); };
			chkCarry.CheckedChanged += (s, e) => { DebugApi.SetCpuProcFlag(ProcFlags.Carry, chkCarry.Checked); };
			chkDecimal.CheckedChanged += (s, e) => { DebugApi.SetCpuProcFlag(ProcFlags.Decimal, chkDecimal.Checked); };
			chkInterrupt.CheckedChanged += (s, e) => { DebugApi.SetCpuProcFlag(ProcFlags.IrqDisable, chkInterrupt.Checked); };
			chkNegative.CheckedChanged += (s, e) => { DebugApi.SetCpuProcFlag(ProcFlags.Negative, chkNegative.Checked); };
			chkOverflow.CheckedChanged += (s, e) => { DebugApi.SetCpuProcFlag(ProcFlags.Overflow, chkOverflow.Checked); };
			chkMemory.CheckedChanged += (s, e) => { DebugApi.SetCpuProcFlag(ProcFlags.MemoryMode8, chkMemory.Checked); };
			chkZero.CheckedChanged += (s, e) => { DebugApi.SetCpuProcFlag(ProcFlags.Zero, chkZero.Checked); };
		}

		public void UpdateStatus(CpuState state)
		{
			_lastState = state;

			_cpuBinder.Entity = state;
			_cpuBinder.UpdateUI();

			txtPC.Text = ((state.K << 16) | state.PC).ToString("X6");

			UpdateCpuFlags();

			chkIrq.Checked = state.IrqSource != 0;
			UpdateStack();
		}

		private void UpdateCpuFlags()
		{
			ProcFlags flags = _lastState.PS;
			chkIndex.Checked = flags.HasFlag(ProcFlags.IndexMode8);
			chkCarry.Checked = flags.HasFlag(ProcFlags.Carry);
			chkDecimal.Checked = flags.HasFlag(ProcFlags.Decimal);
			chkInterrupt.Checked = flags.HasFlag(ProcFlags.IrqDisable);
			chkNegative.Checked = flags.HasFlag(ProcFlags.Negative);
			chkOverflow.Checked = flags.HasFlag(ProcFlags.Overflow);
			chkMemory.Checked = flags.HasFlag(ProcFlags.MemoryMode8);
			chkZero.Checked = flags.HasFlag(ProcFlags.Zero);
		}

		private void UpdateStack()
		{
			StringBuilder sb = new StringBuilder();
			for(UInt32 i = (uint)_lastState.SP + 1; (i & 0xFF) != 0; i++) {
				sb.Append("$");
				sb.Append(DebugApi.GetMemoryValue(SnesMemoryType.CpuMemory, i).ToString("X2"));
				sb.Append(", ");
			}
			string stack = sb.ToString();
			if(stack.Length > 2) {
				stack = stack.Substring(0, stack.Length - 2);
			}
			txtStack.Text = stack;
		}
	}
}
