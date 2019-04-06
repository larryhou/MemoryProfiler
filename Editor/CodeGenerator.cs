using System.IO;
using UnityEditor;
using UnityEditor.MemoryProfiler;
using UnityEngine;

namespace Moobyte.MemoryProfiler
{
	public static class MemoryStreamStringWriting
{
	public static void Write(this MemoryStream stream, string v)
	{
		var data = System.Text.Encoding.UTF8.GetBytes(v);
		stream.Write(data, 0, data.Length);
	}
}

public static class CodeGenerator
{
	private static bool samplerEnabled = false;
	
	[MenuItem("内存/代码/生成C++反序列化代码[性能采样]")]
	static void GenerateCPlusPlusCodeSampler()
	{
		samplerEnabled = true;
		GenerateCPlusPlusCode();
		samplerEnabled = false;
	}

	[MenuItem("内存/代码/生成C++反序列化代码")]
	static void GenerateCPlusPlusCode ()
	{
		var stream = new MemoryStream();
		encode<PackedNativeUnityEngineObject>(stream);
		encode<PackedNativeType>(stream);
		encode<PackedGCHandle>(stream);
		encode<Connection>(stream);
		encode<MemorySection>(stream);
		encode<FieldDescription>(stream);
		encode<TypeDescription>(stream);
		encode<VirtualMachineInformation>(stream);
		encode<PackedMemorySnapshot>(stream);
		
		File.WriteAllBytes(string.Format("{0}/../serialize.cpp", Application.dataPath), stream.ToArray());
	}
	
	// Update is called once per frame
	static void encode<T>(MemoryStream stream)
	{
		var type = typeof(T);
		stream.Write(string.Format("static const string s{0}(\"{0}\");\n", type.Name));
		stream.Write(string.Format("void read{0}({0} &item, FileStream &fs)\n", type.Name));
		stream.Write("{\n");
		var typeProperties = type.GetProperties();
		if (samplerEnabled) {stream.Write(string.Format("    sampler.begin(\"read{0}\");\n", type.Name));}
		if (samplerEnabled) {stream.Write("    sampler.begin(\"readType\");\n");}

		stream.Write("    fs.skipString(true);\n");
		
		if (samplerEnabled) {stream.Write("    sampler.end();\n\n");}
		if (samplerEnabled) {stream.Write("    sampler.begin(\"readFields\");\n");}
		stream.Write(string.Format("    auto fieldCount = fs.readUInt8();\n"));
		stream.Write(string.Format("    assert(fieldCount == {0});\n\n", typeProperties.Length));
		foreach (var property in typeProperties)
		{
			stream.Write("    readField(fs);\n");
			if (property.PropertyType == typeof(string))
			{
				stream.Write(string.Format("    item.{0} = new string(fs.readString(true));\n", property.Name));
			}
			else if (property.PropertyType == typeof(ulong))
			{
				stream.Write(string.Format("    item.{0} = fs.readUInt64(true);\n", property.Name));
			}
			else if (property.PropertyType == typeof(long))
			{
				stream.Write(string.Format("    item.{0} = fs.readInt64(true);\n", property.Name));
			}
			else if (property.PropertyType == typeof(int))
			{
				stream.Write(string.Format("    item.{0} = fs.readInt32(true);\n", property.Name));
			}
			else if (property.PropertyType == typeof(uint))
			{
				stream.Write(string.Format("    item.{0} = fs.readUInt32(true);\n", property.Name));
			}
			else if (property.PropertyType == typeof(bool))
			{
				stream.Write(string.Format("    item.{0} = fs.readBoolean();\n", property.Name));
			}
			else if (property.PropertyType.IsEnum)
			{
				stream.Write(string.Format("    item.{0} = fs.readUInt32(true);\n", property.Name));
			}
			else if (property.PropertyType.IsArray)
			{
				stream.Write("    {\n");
				
				stream.Write("        auto size = fs.readUInt32(true);\n");
				var typeName = property.PropertyType.Name.Replace("[]", "");
				bool isByteArray = false;
				if (typeName == "Byte")
				{
					typeName = "byte_t";
					isByteArray = true;
				}
				
				if (isByteArray)
				{
					stream.Write(string.Format(string.Format("        item.{0} = new Array<byte_t>(size);\n", property.Name)));
					stream.Write(string.Format(string.Format("        fs.read((char *)(item.{0}->items), size);\n", property.Name)));
				}
				else
				{
					if (samplerEnabled){stream.Write(string.Format("        sampler.begin(\"{0}:{1}\");\n", property.Name, property.PropertyType.Name));}
					stream.Write(string.Format("        item.{0} = new Array<{1}>(size);\n", property.Name, typeName));
					stream.Write("        for (auto i = 0; i < size; i++)\n");
					stream.Write("        {\n");
					stream.Write(string.Format("        	read{0}(item.{1}->items[i], fs);\n", typeName, property.Name));
					stream.Write("        }\n");
					if (samplerEnabled){stream.Write("        sampler.end();\n");}
				}
				
				stream.Write("    }\n");
			}
			else
			{
				stream.Write(string.Format("    item.{0} = new {1};\n", property.Name, property.PropertyType.Name));
				stream.Write(string.Format("    read{0}(*item.{1}, fs);\n", property.PropertyType.Name, property.Name));
			}
		}
		if (samplerEnabled) {stream.Write("    sampler.end();\n");}
		if (samplerEnabled) {stream.Write("    sampler.end();\n");}
		stream.Write("}\n\n");
		
	}
}
}


