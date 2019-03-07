using System;
using System.Collections;
using System.IO;
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
		
		public static void Write(this Stream stream, ushort v)
		{
			int count = 2;
			int shift = count * 8;
			while (count-- > 0)
			{
				shift -= 8;
				stream.WriteByte((byte) ((v >> shift) & 0xFF));
			}
		}
		
		public static void Write(this Stream stream, short v)
		{
			stream.Write((ushort)v);
		}

		public static void Write(this Stream stream, uint v)
		{
			int count = 4;
			int shift = count * 8;
			while (count-- > 0)
			{
				shift -= 8;
				stream.WriteByte((byte) ((v >> shift) & 0xFF));
			}
		}
		
		public static void Write(this Stream stream, int v)
		{
			stream.Write((uint)v);
		}
		
		public static void Write(this Stream stream, ulong v)
		{
			int count = 8;
			int shift = count * 8;
			while (count-- > 0)
			{
				shift -= 8;
				stream.WriteByte((byte) ((v >> shift) & 0xFF));
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
	}

	public static class MemoryCapture
	{
		[MenuItem("内存/捕获内存")]
		public static void Capture()
		{
			MemorySnapshot.OnSnapshotReceived += OnSnapshotComplete;
			MemorySnapshot.RequestNewSnapshot();
		}

		private static void OnSnapshotComplete(PackedMemorySnapshot snapshot)
		{
			MemorySnapshot.OnSnapshotReceived -= OnSnapshotComplete;
			var spacedir = string.Format("{0}/../MemoryCapture", Application.dataPath);
			if (!Directory.Exists(spacedir))
			{
				Directory.CreateDirectory(spacedir);
			}
			
			var filepath = string.Format("{0}/snapshot_{1:yyyyMMddHHmmss}.dat", spacedir, DateTime.Now);
			Stream stream = new FileStream(filepath, FileMode.CreateNew);
			EncodeObject(stream, snapshot);
			stream.Flush();
			Debug.LogFormat("+ {0}", filepath);
			stream.Close();
		}

		private static void EncodeObject(Stream output, object data)
		{
			var classType = data.GetType();
			output.Write(classType.FullName);
			var list = classType.GetProperties();
			output.Write((byte)list.Length);
			
			foreach(var property in classType.GetProperties())
			{
				output.Write(property.Name);
				output.Write(property.PropertyType.FullName);
				
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
						output.Write(value.ToString());
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
					output.Write(value.ToString());
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


