using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices;

namespace Mesen.GUI
{
   class HistoryViewerApi
   {
		private const string DllPath = "MesenSCore.dll";

		[DllImport(DllPath)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool HistoryViewerEnabled();
		[DllImport(DllPath)] public static extern void HistoryViewerInitialize(IntPtr windowHandle, IntPtr viewerHandle);
		[DllImport(DllPath)] public static extern void HistoryViewerRelease();
		[DllImport(DllPath)] public static extern void HistoryViewerStop();

		[DllImport(DllPath)] public static extern UInt32 HistoryViewerGetHistoryLength();
		[DllImport(DllPath)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool HistoryViewerSaveMovie([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))] string movieFile, UInt32 startPosition, UInt32 endPosition);
		[DllImport(DllPath)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool HistoryViewerCreateSaveState([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(Utf8Marshaler))] string outfileFile, UInt32 position);
		[DllImport(DllPath)] public static extern void HistoryViewerSetPosition(UInt32 seekPosition);
		[DllImport(DllPath)] public static extern void HistoryViewerResumeGameplay(UInt32 seekPosition);
		[DllImport(DllPath)] public static extern UInt32 HistoryViewerGetPosition();
		[DllImport(DllPath, EntryPoint = "HistoryViewerGetSegments")] public static extern void HistoryViewerGetSegmentsWrapper(IntPtr segmentBuffer, ref UInt32 bufferSize);

		public static UInt32[] HistoryViewerGetSegments()
		{
			UInt32[] segmentBuffer = new UInt32[HistoryViewerApi.HistoryViewerGetHistoryLength() / 60];
			UInt32 bufferSize = (UInt32)segmentBuffer.Length;

			GCHandle hSegmentBuffer = GCHandle.Alloc(segmentBuffer, GCHandleType.Pinned);
			try
			{
				HistoryViewerApi.HistoryViewerGetSegmentsWrapper(hSegmentBuffer.AddrOfPinnedObject(), ref bufferSize);
			}
			finally
			{
				hSegmentBuffer.Free();
			}
			Array.Resize(ref segmentBuffer, (int)bufferSize);

			return segmentBuffer;
		}
	}
}
