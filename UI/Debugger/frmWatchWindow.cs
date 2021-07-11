using Mesen.GUI.Config;
using Mesen.GUI.Forms;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Mesen.GUI.Debugger
{
	public partial class frmWatchWindow : BaseForm
	{
		private NotificationListener _notifListener;

		public frmWatchWindow()
		{
			InitializeComponent();

			if(!DesignMode) {
				this.toolTip.SetToolTip(picWatchHelp, ctrlWatch.GetTooltipText());
			}
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);
			// Allow more CPU types later
			ctrlWatch.CpuType = EmuApi.GetRomInfo().CoprocessorType == CoprocessorType.Gameboy ? CpuType.Gameboy : CpuType.Cpu;

			if(!DesignMode) {
				RestoreLocation(ConfigManager.Config.WatchWindow.WindowLocation, ConfigManager.Config.WatchWindow.WindowSize);
				_notifListener = new NotificationListener();
				_notifListener.OnNotification += _notifListener_OnNotification;
				ctrlWatch.UpdateWatch(true);
			}
		}

		protected override void OnFormClosing(FormClosingEventArgs e)
		{
			base.OnFormClosing(e);

			ConfigManager.Config.WatchWindow.WindowSize = this.WindowState != FormWindowState.Normal ? this.RestoreBounds.Size : this.Size;
			ConfigManager.Config.WatchWindow.WindowLocation = this.WindowState != FormWindowState.Normal ? this.RestoreBounds.Location : this.Location;
			ConfigManager.ApplyChanges();
		}

		protected override void OnFormClosed(FormClosedEventArgs e)
		{
			base.OnFormClosed(e);

			if(_notifListener != null) {
				_notifListener.Dispose();
				_notifListener = null;
			}
		}

		private void _notifListener_OnNotification(NotificationEventArgs e)
		{
			switch(e.NotificationType) {
				case ConsoleNotificationType.PpuFrameDone:
					this.BeginInvoke((MethodInvoker)(() => {
						ctrlWatch.UpdateWatch(false);
					}));
					break;

				case ConsoleNotificationType.CodeBreak:
					this.BeginInvoke((MethodInvoker)(() => {
						ctrlWatch.UpdateWatch(false);
					}));
					break;
			}
		}
	}
}
