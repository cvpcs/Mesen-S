using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Http;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Mesen.GUI.WebAPI
{
   class Host
   {
		public static void Start()
	   {
			new WebHostBuilder()
				.UseHttpSys()
				.UseUrls("http://127.0.0.1:8080")
				.Configure(app => app
					.UseWebSockets()
					.Use(async (context, next) =>
					{
						if (context.WebSockets.IsWebSocketRequest)
						{
							using (WebSocket webSocket = await context.WebSockets.AcceptWebSocketAsync())
							{
								await Process(context, webSocket);
							}
						}
					}))
				.Build()
				.Start();
	   }

		private static async Task Process(HttpContext context, WebSocket webSocket)
		{
			while (webSocket.State == WebSocketState.Open)
			{
				RequestMessage message = await webSocket.ReceiveObjectAsync<RequestMessage>(CancellationToken.None);
				switch (message?.Opcode)
				{
					case "DeviceList":
						await webSocket.SendObjectAsync(new DeviceListResponseMessage("Mesen-S"), CancellationToken.None);
						break;
					case "GetAddress":
						if (EmuApi.IsRunning())
						{
							var address = uint.Parse(message.Operands[0], NumberStyles.HexNumber);
							var length = uint.Parse(message.Operands[1], NumberStyles.HexNumber);
							var type = SnesMemoryType.PrgRom;
							if (address >= 0xF50000 && address < 0xF70000)
							{
								type = SnesMemoryType.WorkRam;
								address -= 0xF50000;
							}
							else if (address >= 0xE00000)
							{
								type = SnesMemoryType.SaveRam;
								address -= 0xE00000;
							}

							var data = new byte[length];
							for (uint i = 0; i < length; i++)
							{
								data[i] = DebugApi.GetMemoryValue(type, address + i);
							}
							
							await webSocket.SendBytesAsync(data, CancellationToken.None);
						}
						break;
					default:
						break;
				}
			}

			await webSocket.CloseAsync(WebSocketCloseStatus.NormalClosure, "Closing", CancellationToken.None);
		}
	}
}