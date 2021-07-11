using Mesen.GUI.Config;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Mesen.GUI.Forms
{
	public partial class frmHistoryViewer : BaseForm
	{
		private bool _paused = true;

		public frmHistoryViewer()
		{
			InitializeComponent();
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);

			RestoreLocation(ConfigManager.Config.HistoryViewer.WindowLocation, ConfigManager.Config.HistoryViewer.WindowSize);

			tlpRenderer.Visible = true;
			picNsfIcon.Visible = false;
		}

		protected override void OnClosed(EventArgs e)
		{
			base.OnClosed(e);

			ConfigManager.Config.HistoryViewer.WindowLocation = this.WindowState == FormWindowState.Normal ? this.Location : this.RestoreBounds.Location;
			ConfigManager.Config.HistoryViewer.WindowSize = this.WindowState == FormWindowState.Normal ? this.Size : this.RestoreBounds.Size;
			ConfigManager.Config.HistoryViewer.Volume = trkVolume.Value;
			ConfigManager.ApplyChanges();
		}

		protected override void OnShown(EventArgs e)
		{
			base.OnShown(e);

			HistoryViewerApi.HistoryViewerInitialize(this.Handle, ctrlRenderer.Handle);
			trkPosition.Maximum = (int)(HistoryViewerApi.HistoryViewerGetHistoryLength() / 60);
			UpdatePositionLabel(0);
			EmuApi.Resume(EmuApi.ConsoleId.HistoryViewer);
			tmrUpdatePosition.Start();
			trkVolume.Value = ConfigManager.Config.HistoryViewer.Volume;
			btnPausePlay.Focus();

			UpdateScale();
			this.Resize += (s, evt) => {
				UpdateScale();
			};
		}

		protected override void OnClosing(CancelEventArgs e)
		{
			tmrUpdatePosition.Stop();
			HistoryViewerApi.HistoryViewerRelease();
			base.OnClosing(e);
		}

		private void TogglePause()
		{
			if(trkPosition.Value == trkPosition.Maximum) {
				HistoryViewerApi.HistoryViewerSetPosition(0);
			}
			if(_paused) {
				EmuApi.Resume(EmuApi.ConsoleId.HistoryViewer);
			} else {
				EmuApi.Pause(EmuApi.ConsoleId.HistoryViewer);
			}
		}

		private void trkPosition_ValueChanged(object sender, EventArgs e)
		{
			HistoryViewerApi.HistoryViewerSetPosition((UInt32)trkPosition.Value);
		}

		private void SetScale(int scale)
		{
			ScreenSize size = EmuApi.GetScreenSize(true, EmuApi.ConsoleId.HistoryViewer);
			Size newSize = new Size(size.Width * scale, size.Height * scale);
			if(this.WindowState != FormWindowState.Maximized) {
				Size sizeGap = newSize - ctrlRenderer.Size;
				this.ClientSize += sizeGap;
			}
			ctrlRenderer.Size = newSize;
		}

		private void UpdateScale()
		{
			Size dimensions = pnlRenderer.ClientSize;
			ScreenSize size = EmuApi.GetScreenSize(true, EmuApi.ConsoleId.HistoryViewer);

			double verticalScale = (double)dimensions.Height / size.Height;
			double horizontalScale = (double)dimensions.Width / size.Width;
			double scale = Math.Min(verticalScale, horizontalScale);
			EmuApi.SetVideoScale(scale, EmuApi.ConsoleId.HistoryViewer);
		}

		private void tmrUpdatePosition_Tick(object sender, EventArgs e)
		{
			ScreenSize size = EmuApi.GetScreenSize(false, EmuApi.ConsoleId.HistoryViewer);
			if(size.Width != ctrlRenderer.ClientSize.Width || size.Height != ctrlRenderer.ClientSize.Height) {
				ctrlRenderer.ClientSize = new Size(size.Width, size.Height);
			}

			_paused = EmuApi.IsPaused(EmuApi.ConsoleId.HistoryViewer);
			if(_paused) {
				btnPausePlay.Image = Properties.Resources.MediaPlay;
			} else {
				btnPausePlay.Image = Properties.Resources.MediaPause;
			}

			UInt32 positionInSeconds = HistoryViewerApi.HistoryViewerGetPosition();
			UpdatePositionLabel(positionInSeconds);

			if(positionInSeconds <= trkPosition.Maximum) {
				trkPosition.ValueChanged -= trkPosition_ValueChanged;
				trkPosition.Value = (int)positionInSeconds;
				trkPosition.ValueChanged += trkPosition_ValueChanged;
			}
		}

		private void UpdatePositionLabel(uint positionInSeconds)
		{
			TimeSpan currentPosition = new TimeSpan(0, 0, (int)positionInSeconds);
			TimeSpan totalLength = new TimeSpan(0, 0, trkPosition.Maximum);
			lblPosition.Text = (
				currentPosition.Minutes.ToString("00") + ":" + currentPosition.Seconds.ToString("00")
				+ " / " +
				totalLength.Minutes.ToString("00") + ":" + totalLength.Seconds.ToString("00")
			);
		}

		private void mnuClose_Click(object sender, EventArgs e)
		{
			this.Close();
		}

		private void mnuResumeGameplay_Click(object sender, EventArgs e)
		{
			HistoryViewerApi.HistoryViewerResumeGameplay(HistoryViewerApi.HistoryViewerGetPosition());
		}
		
		private void mnuFile_DropDownOpening(object sender, EventArgs e)
		{
			mnuExportMovie.DropDownItems.Clear();

			List<UInt32> segments = new List<UInt32>(HistoryViewerApi.HistoryViewerGetSegments());
			UInt32 segmentStart = 0;
			segments.Add(HistoryViewerApi.HistoryViewerGetHistoryLength() / 60);

			for(int i = 0; i < segments.Count; i++) {
				if(segments[i] - segmentStart > 2) {
					//Only list segments that are at least 2 seconds long
					UInt32 segStart = segmentStart;
					UInt32 segEnd = segments[i];
					TimeSpan start = new TimeSpan(0, 0, (int)(segmentStart));
					TimeSpan end = new TimeSpan(0, 0, (int)(segEnd));

					string segmentName = ResourceHelper.GetMessage("MovieSegment", (mnuExportMovie.DropDownItems.Count + 1).ToString());
					ToolStripMenuItem segmentItem = new ToolStripMenuItem(segmentName + ", " + start.ToString() + " - " + end.ToString());

					ToolStripMenuItem exportFull  = new ToolStripMenuItem(ResourceHelper.GetMessage("MovieExportEntireSegment"));
					exportFull.Click += (s, evt) => {
						ExportMovie(segStart, segEnd);
					};

					ToolStripMenuItem exportCustomRange = new ToolStripMenuItem(ResourceHelper.GetMessage("MovieExportSpecificRange"));
					exportCustomRange.Click += (s, evt) => {
						using(frmSelectExportRange frm = new frmSelectExportRange(segStart, segEnd)) {
							if(frm.ShowDialog(this) == DialogResult.OK) {
								ExportMovie(frm.ExportStart, frm.ExportEnd);
							}
						}
					};

					segmentItem.DropDown.Items.Add(exportFull);
					segmentItem.DropDown.Items.Add(exportCustomRange);
					mnuExportMovie.DropDownItems.Add(segmentItem);
				}
				segmentStart = segments[i] + 1;
			}

			mnuImportMovie.Visible = false;
			mnuExportMovie.Enabled = mnuExportMovie.HasDropDownItems;
		}

		private void ExportMovie(UInt32 segStart, UInt32 segEnd)
		{
			using(SaveFileDialog sfd = new SaveFileDialog()) {
				sfd.SetFilter(ResourceHelper.GetMessage("FilterMovie"));
				sfd.InitialDirectory = ConfigManager.MovieFolder;
				sfd.FileName = EmuApi.GetRomInfo().GetRomName() + ".mmo";
				if(sfd.ShowDialog() == DialogResult.OK) {
					if(!HistoryViewerApi.HistoryViewerSaveMovie(sfd.FileName, segStart, segEnd)) {
						MesenMsgBox.Show("MovieSaveError", MessageBoxButtons.OK, MessageBoxIcon.Error);
					}
				}
			}
		}

		private void mnuCreateSaveState_Click(object sender, EventArgs e)
		{
			using(SaveFileDialog sfd = new SaveFileDialog()) {
				sfd.SetFilter(ResourceHelper.GetMessage("FilterSavestate"));
				sfd.InitialDirectory = ConfigManager.SaveStateFolder;
				sfd.FileName = EmuApi.GetRomInfo().GetRomName() + ".mst";
				if(sfd.ShowDialog() == DialogResult.OK) {
					if(!HistoryViewerApi.HistoryViewerCreateSaveState(sfd.FileName, HistoryViewerApi.HistoryViewerGetPosition())) {
						MesenMsgBox.Show("FileSaveError", MessageBoxButtons.OK, MessageBoxIcon.Error);
					}
				}
			}
		}

		private void btnPausePlay_Click(object sender, EventArgs e)
		{
			TogglePause();
		}

		private void ctrlRenderer_MouseClick(object sender, MouseEventArgs e)
		{
			if(e.Button == MouseButtons.Left) {
				TogglePause();
			}
		}

		private void trkVolume_ValueChanged(object sender, EventArgs e)
		{
			EmuApi.SetMasterVolume(trkVolume.Value, EmuApi.ConsoleId.HistoryViewer);
		}

		private void mnuScale1x_Click(object sender, EventArgs e)
		{
			SetScale(1);
		}

		private void mnuScale2x_Click(object sender, EventArgs e)
		{
			SetScale(2);
		}

		private void mnuScale3x_Click(object sender, EventArgs e)
		{
			SetScale(3);
		}

		private void mnuScale4x_Click(object sender, EventArgs e)
		{
			SetScale(4);
		}

		private void mnuScale5x_Click(object sender, EventArgs e)
		{
			SetScale(5);
		}

		private void mnuScale6x_Click(object sender, EventArgs e)
		{
			SetScale(6);
		}
	}
}
