using System;
using System.Collections;
using System.IO;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEditor.MemoryProfiler;
using UnityEngine;

namespace Moobyte.MemoryProfiler
{
	public static class MemorySteamExtension
	{
		private static byte[] buffer;
		public static void Write(this Stream stream, string v)
		{
			var charCount = System.Text.Encoding.UTF8.GetByteCount(v);
			if (buffer == null)
			{
				buffer = new byte[1 << 16];
			}
			else if (buffer.Length < charCount)
			{
				buffer = new byte[(charCount / 4 + 1) * 4];
			}
			
			try
			{
				var size = System.Text.Encoding.UTF8.GetBytes(v, 0, charCount, buffer, 0);
				stream.Write(size);
				if (size > 0)
				{
					stream.Write(buffer, 0, size);
				}
			}
			catch (ArgumentOutOfRangeException e)
			{
				Console.WriteLine(e);
				
				var bytes = System.Text.Encoding.UTF8.GetBytes(v);
				stream.Write(bytes.Length);
				stream.Write(bytes, 0, bytes.Length);
			}
		}

		public static void Write(this Stream stream, DateTime timestamp)
		{
			var refer = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc);
			stream.Write((long) ((timestamp - refer).TotalSeconds * 1e6));
		}
		
		public static void Write(this Stream stream, float v)
		{
			var data = BitConverter.GetBytes(v);
			stream.Write(data, 0, 4);
		}
		
		public static void Write(this Stream stream, double v)
		{
			var data = BitConverter.GetBytes(v);
			stream.Write(data, 0, 8);
		}

		public static void Write(this Stream stream, char v)
		{
			stream.Write((byte)v);
		}
		
		public static void Write(this Stream stream, byte v)
		{
			stream.WriteByte(v);
		}

		public static void Write(this Stream stream, uint v)
		{
			int count = 0;
			int shift = 0;
			while (count++ < 4)
			{
				stream.WriteByte((byte) ((v >> shift) & 0xFF));
				shift += 8;
			}
		}
		
		public static void Write(this Stream stream, int v)
		{
			stream.Write((uint)v);
		}
		
		public static void Write(this Stream stream, ushort v)
		{
			int count = 0;
			int shift = 0;
			while (count++ < 2)
			{
				stream.WriteByte((byte) ((v >> shift) & 0xFF));
				shift += 8;
			}
		}
		
		public static void Write(this Stream stream, short v)
		{
			stream.Write((ushort)v);
		}
		
		public static void Write(this Stream stream, ulong v)
		{
			int count = 0;
			int shift = 0;
			while (count++ < 8)
			{
				stream.WriteByte((byte) ((v >> shift) & 0xFF));
				shift += 8;
			}
		}
		
		public static void Write(this Stream stream, long v)
		{
			stream.Write((ulong)v);
		}
		
		public static void Write(this Stream stream, bool v)
		{
			stream.WriteByte((byte)(v ? 1 : 0));
		}

		public static void WriteNull(this Stream stream)
		{
			stream.WriteByte(0x00);
		}

		public static void Write(this Stream stream, byte[] v)
		{
			stream.Write(v, 0, v.Length);
		}
	}

	public static class MemoryCapture
	{
		[MenuItem("性能/捕获快照", false, 20)]
		public static void Capture()
		{
			MemorySnapshot.OnSnapshotReceived += OnSnapshotComplete;
			MemorySnapshot.RequestNewSnapshot();
		}
		
//		[MenuItem("性能分析/捕获快照和原生内存")]
		public static void CaptureWithNativeMemeory()
		{
			MemorySnapshot.OnSnapshotReceived += OnSnapshotCompleteForCrawling;
			MemorySnapshot.RequestNewSnapshot();
		}

		private static void OnSnapshotComplete(PackedMemorySnapshot snapshot)
		{
			MemorySnapshot.OnSnapshotReceived -= OnSnapshotComplete;
			ExportMemorySnapshot(snapshot, false);
		}
		
		private static void OnSnapshotCompleteForCrawling(PackedMemorySnapshot snapshot)
		{
			MemorySnapshot.OnSnapshotReceived -= OnSnapshotCompleteForCrawling;
			ExportMemorySnapshot(snapshot, true);
		}

		public static void AcceptMemorySnapshot(PackedMemorySnapshot snapshot)
		{
			ExportMemorySnapshot(snapshot, false);
		}

		private static void ExportMemorySnapshot(PackedMemorySnapshot snapshot, bool nativeEnabled)
		{
			var spacedir = string.Format("{0}/../MemoryCapture", Application.dataPath);
			if (!Directory.Exists(spacedir))
			{
				Directory.CreateDirectory(spacedir);
			}

			var filepath = string.Format("{0}/{1:yyyyMMddHHmmss}_snapshot.pms", spacedir, DateTime.Now);
			FileStream stream = new FileStream(filepath, FileMode.CreateNew);
			
			// Write snapshot header
			long offset = 0;
			stream.Write('P');
			stream.Write('M');
			stream.Write('S');
			stream.Write("GENERATED THROUGH MEMORYPROFILER DEVELOPED BY LARRYHOU");
			stream.Write(Application.unityVersion);
			stream.Write(SystemInfo.operatingSystem);
			stream.Write(Guid.NewGuid().ToByteArray());
			offset = stream.Position;
			stream.Write((uint) 0);
			stream.Write(DateTime.Now);
			
			// Write basic snapshot memory
			PackSnapshotMemory(stream, snapshot);

			if (nativeEnabled)
			{			
				// Write native object memory
				PackNativeObjectMemory(stream, snapshot);	
			}
			
			RepackUInt32(stream, offset, (uint)stream.Length);
			
			stream.Close();
			Debug.LogFormat("+ {0}", filepath);
			
		}

		private static void RepackUInt32(Stream input, long offset, uint size)
		{
			var position = input.Position;
			input.Seek(offset, SeekOrigin.Begin);
			input.Write(size);
			input.Seek(position, SeekOrigin.Begin);
		}

		private static void PackSnapshotMemory(Stream input, PackedMemorySnapshot snapshot)
		{
			var offset = input.Position;
			input.Write((uint)0);
			input.Write('0');
			
			EncodeObject(input, snapshot.virtualMachineInformation);
			EncodeObject(input, snapshot);
			
			{
				RepackUInt32(input, offset, (uint)(input.Position - offset));
				input.Write(DateTime.Now);
			}
			
			input.Flush();
		}
		
		private static void PackNativeObjectMemory(Stream input, PackedMemorySnapshot snapshot)
		{
			var offset = input.Position;
			input.Write((uint)0);
			input.Write('1');
			
			// Write the number of native objects
			input.Write(snapshot.nativeObjects.Length);
			
			// Write native objects memory
			var buffer = new byte[1 << 25];
			foreach (var no in snapshot.nativeObjects)
			{
				input.Write(no.nativeObjectAddress);
				input.Write(no.size);
				if (no.size > buffer.Length || no.size < 0)
				{
					Debug.LogFormat("[SIZE] address=0x{0:X} name='{1}' type='{2}' size={3}", no.nativeObjectAddress, no.name,
						snapshot.nativeTypes[no.nativeTypeArrayIndex].name, no.size);
					input.Write((uint)0);
				}
				else
				{
					Marshal.Copy(new IntPtr(no.nativeObjectAddress), buffer, 0, no.size);
					input.Write(buffer, 0, no.size);
				}
			}
			
			{
				RepackUInt32(input, offset, (uint)(input.Position - offset));
				input.Write(DateTime.Now);
			}
			
			input.Flush();
		}

		private static void EncodeObject(Stream output, object data)
		{
			var classType = data.GetType();
			var list = classType.GetProperties();
			output.Write((byte)list.Length);
			
			foreach(var property in classType.GetProperties())
			{	
				var value = property.GetValue(data, null);
				var type = property.PropertyType;
				
				if ("UnityEditor.MemoryProfiler".Equals(type.Namespace))
				{
					if (type.IsArray)
					{
						if (value == null)
						{
							output.WriteNull();
						}
						else
						{
							EncodeArray(output, (IList) value);
						}
					}
					else if (type.IsClass)
					{
						if (value == null)
						{
							output.WriteNull();
						}
						else
						{
							EncodeObject(output, value);
						}
					}
					else if (type.IsEnum)
					{
						output.Write((int)value);
					}
					else if (type.IsValueType)
					{
						EncodeObject(output, value);
					}
					else
					{
						throw new NotImplementedException(type.FullName);
					}
				}
				else if (type == typeof(int))
				{
					output.Write((int)value);
				}
				else if (type == typeof(uint))
				{
					output.Write((uint)value);
				}
				else if (type == typeof(long))
				{
					output.Write((long)value);
				}
				else if (type == typeof(ulong))
				{
					output.Write((ulong)value);
				}
				else if (type == typeof(bool))
				{
					output.Write((bool)value);
				}
				else if (type == typeof(string))
				{
					output.Write(value == null ? string.Empty : (string) value);
				}
				else if (type.IsArray)
				{
					if (type == typeof(byte[]))
					{
						var bytes = (byte[]) value;
						output.Write(bytes.Length);
						output.Write(bytes, 0, bytes.Length);
					}
					else
					{
						throw new NotImplementedException(type.FullName);
					}
				}
				else if (type.IsEnum)
				{
					output.Write((int)value);
				}
				else if (type.IsClass)
				{
					EncodeObject(output, value);
				}
				else
				{
					throw new NotImplementedException(type.FullName);
				}
			}
		}

		private static void EncodeArray(Stream output, IList list)
		{
			output.Write(list.Count);
			for (var i = 0; i < list.Count; i++)
			{
				EncodeObject(output, list[i]);
			}
		}
	}
}


