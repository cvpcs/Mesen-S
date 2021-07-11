using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Http;
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
   internal class DeviceListResponseMessage
   {
		public string[] Results;

		public DeviceListResponseMessage(params string[] devices)
			=> Results = devices;
	}
}
