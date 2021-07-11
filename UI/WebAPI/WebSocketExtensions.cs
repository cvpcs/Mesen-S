using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Mesen.GUI.WebAPI
{
   internal static class WebSocketExtensions
   {
		public static async Task<string> ReceiveStringAsync(this WebSocket webSocket, CancellationToken cancellationToken)
		{
			IEnumerable<byte> bytes = new List<byte>();
			var buffer = new Memory<byte>(new byte[1000]);

			ValueWebSocketReceiveResult result;
			do
			{
				result = await webSocket.ReceiveAsync(buffer, cancellationToken);
				bytes = bytes.Concat(buffer.Slice(0, result.Count).ToArray());
			} while (!result.EndOfMessage);

			return Encoding.UTF8.GetString(bytes.ToArray());
		}

		public static async Task<T> ReceiveObjectAsync<T>(this WebSocket webSocket, CancellationToken cancellationToken)
			where T : class
		{
			string json = await webSocket.ReceiveStringAsync(cancellationToken);
			return JsonConvert.DeserializeObject<T>(json);
		}

		public static async Task SendBytesAsync(this WebSocket webSocket, byte[] bytes, CancellationToken cancellationToken)
		{
			await webSocket.SendAsync(new ArraySegment<byte>(bytes), WebSocketMessageType.Binary, true, cancellationToken);
		}

		public static async Task SendStringAsync(this WebSocket webSocket, string message, CancellationToken cancellationToken)
		{
			await webSocket.SendAsync(new ArraySegment<byte>(Encoding.UTF8.GetBytes(message)), WebSocketMessageType.Text, true, cancellationToken);
		}

		public static async Task SendObjectAsync<T>(this WebSocket webSocket, T obj, CancellationToken cancellationToken)
	   {
			string json = JsonConvert.SerializeObject(obj);
			await webSocket.SendStringAsync(json, cancellationToken);
	   }
   }
}
