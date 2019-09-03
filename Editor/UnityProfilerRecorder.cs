using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditorInternal;
#if UNITY_2018_1_OR_NEWER
using UnityEditorInternal.Profiling;
#endif
using UnityEngine;
using UnityEngine.Profiling;

#if UNITY_2019_1_OR_NEWER
public static class ProfilerColumn
{
    public static int DontSort = -1;
    public static int FunctionName = 0;
    public static int TotalPercent = 1;
    public static int SelfPercent = 2;
    public static int Calls = 3;
    public static int GCMemory = 4;
    public static int TotalTime = 5;
    public static int SelfTime = 6;
    public static int DrawCalls = 7;
    public static int TotalGPUTime = 8;
    public static int SelfGPUTime = 9;
    public static int TotalGPUPercent = 10; // 0x0000000A
    public static int SelfGPUPercent = 11; // 0x0000000B
    public static int WarningCount = 12; // 0x0000000C
    public static int ObjectName = 13; // 0x0000000D
}

public static class ProfilerViewType
{
    public static int Hierarchy = 0;
    public static int Timeline = 1;
    public static int RawHierarchy = 2;
}

#endif

namespace Moobyte.MemoryProfiler
{
    internal struct StackSample
    {
        public int id;
        public string name;
        public int callsCount;
        public int gcAllocBytes;
        public float totalTime;
        public float selfTime;
    }
    
    public static class UnityProfilerRecorder
    {
        [MenuItem("性能/开始采样+原生录像", false, 52)]
        private static void StartRecordingWithUnityFormat()
        {
            StartRecording(true);
        }
    
        [MenuItem("性能/开始采样", false, 51)]
        private static void StartRecordingQuickly()
        {
            StartRecording(false);
        }

        private static Dictionary<int, List<int>> metadatas;
        private static byte profilerAreaCount = 0;
        
        public static void StartRecording(bool includeUnityFormat = false)
        {
            var spacedir = string.Format("{0}/../ProfilerCapture", Application.dataPath);
            if (!Directory.Exists(spacedir))
            {
                Directory.CreateDirectory(spacedir);
            }

            if (stream != null)
            {
                stream.Close();
                stream = null;
            }

            {
                frameCursor = 0;
                strmap = new Dictionary<string, int>();
                strseq = 0;
            }
            var filepath = string.Format("{0}/{1:yyyyMMddHHmmss}_PERF.pfc", spacedir, DateTime.Now);
            stream = new FileStream(filepath, FileMode.Create, FileAccess.ReadWrite, FileShare.Inheritable, 1 << 21);
            stream.Write('P'); // + 1
            stream.Write('F'); // + 1
            stream.Write('C'); // + 1
            stream.Write(DateTime.Now); // + 8
            stream.Write((uint)0); // + 4
            
            stream.Write((uint)0); // meta data size
            
            var offset = stream.Position;
            metadatas = new Dictionary<int, List<int>>();
            var paType = typeof(ProfilerArea);
            var areaNames = Enum.GetNames(paType);
            
            profilerAreaCount = (byte)areaNames.Length;
            if (areaNames[areaNames.Length - 1].Equals("AreaCount"))
            {
                --profilerAreaCount;
            }
            
            stream.Write(profilerAreaCount); // area count
            for (var area = 0; area  < profilerAreaCount; area++)
            {
                var areaValue = (ProfilerArea) area;
                stream.Write((byte)area);
                stream.Write(areaValue.ToString());
                List<int> children;
                metadatas.Add((int)area, children = new List<int>());
                var properties = ProfilerDriver.GetGraphStatisticsPropertiesForArea(areaValue);
                stream.Write((byte)properties.Length);
                for (var i = 0; i < properties.Length; i++)
                {
                    var name = properties[i];
#if UNITY_2018_1_OR_NEWER
                    var identifier = ProfilerDriver.GetStatisticsIdentifierForArea(areaValue, name);
#else
                    var identifier = ProfilerDriver.GetStatisticsIdentifier(name);
#endif
                    stream.Write(name);
                    
                    children.Add(identifier);
                }
            }

            var position = stream.Position;
            var size = stream.Position - offset;
            stream.Seek(offset - 4, SeekOrigin.Begin);
            stream.Write((uint)size);
            stream.Seek(position, SeekOrigin.Begin);
            
            provider = new float[ProfilerDriver.maxHistoryLength];
            
            Profiler.enabled = true;
            if (includeUnityFormat)
            {
                Profiler.logFile = filepath.Substring(0, filepath.Length - 4);
                Profiler.enableBinaryLog = true;
            }
            
            EditorApplication.update -= Update;
            EditorApplication.update += Update;
        }

        [MenuItem("性能/停止采样", false, 55)]
        public static void StopRecording()
        {
            Profiler.logFile = null;
            EditorApplication.update -= Update;
            
            if (stream != null)
            {
                // encode strings
                var offset = (int)stream.Position;
                stream.Write(strmap.Count);
                
                var collection = new string[strmap.Count];
                foreach (var pair in strmap)
                {
                    collection[pair.Value] = pair.Key;
                }

                for (var i = 0; i < collection.Length; i++)
                {
                    stream.Write(collection[i]);
                }
                
                stream.Write(DateTime.Now);
                stream.Write("GENERATED THROUGH PROFILERRECORDER DEVELOPED BY LARRYHOU");
                stream.Write(Application.unityVersion);
                stream.Write(SystemInfo.operatingSystem);
                stream.Write(Guid.NewGuid().ToByteArray());
                
                // encode string offset
                stream.Seek(11, SeekOrigin.Begin);
                stream.Write(offset);

                stream.Close();
                stream = null;
            }
        }

        internal static int getStringRef(string v)
        {
            int index = -1;
            if (!strmap.TryGetValue(v, out index))
            {
                strmap.Add(v, index = strseq++);
            }

            return index;
        }

        private static int frameCursor = 0;
        private static FileStream stream;
        
        private static Dictionary<string, int> strmap;
        private static int strseq;

        private static float[] provider;
        static void Update()
        {
            var stopFrameIndex = ProfilerDriver.lastFrameIndex;
            var frameIndex = Math.Max(frameCursor + 1, ProfilerDriver.firstFrameIndex);
            while (frameIndex <= stopFrameIndex)
            {
                var samples = new Dictionary<int, StackSample>();
                var relations = new Dictionary<int, int>();

                var cursor = new Stack<int>();
                var sequence = 0;
                
                var root = new ProfilerProperty();
                root.SetRoot(frameIndex, ProfilerColumn.TotalTime, ProfilerViewType.Hierarchy);
                root.onlyShowGPUSamples = false;
                while (root.Next(true))
                {
                    var depth = root.depth;
                    while (cursor.Count + 1 > depth)
                    {
                        cursor.Pop();
                    }
                    
#if UNITY_2017_1_OR_NEWER
                    samples.Add(sequence, new StackSample
                    {
                        id = sequence,
                        name = root.propertyName,
                        callsCount = (int)root.GetColumnAsSingle(ProfilerColumn.Calls),
                        gcAllocBytes = (int)root.GetColumnAsSingle(ProfilerColumn.GCMemory),
                        totalTime = root.GetColumnAsSingle(ProfilerColumn.TotalTime),
                        selfTime = root.GetColumnAsSingle(ProfilerColumn.SelfTime),
                    });
#else
                    var ss = new StackSample {id = sequence, name = root.propertyName};
                    int.TryParse(root.GetColumn(ProfilerColumn.Calls), out ss.callsCount);
                    int.TryParse(root.GetColumn(ProfilerColumn.GCMemory), out ss.gcAllocBytes);
                    float.TryParse(root.GetColumn(ProfilerColumn.TotalTime), out ss.totalTime);
                    float.TryParse(root.GetColumn(ProfilerColumn.SelfTime), out ss.selfTime);
                    samples.Add(sequence, ss);
#endif
                    
                    relations[sequence] = cursor.Count != 0 ? cursor.Peek() : -1;

                    if (root.HasChildren)
                    {
                        cursor.Push(sequence);
                    }

                    ++sequence;
                }
                
                // encode frame index
                var frameFPS = float.Parse(root.frameFPS);
                stream.Write(frameIndex);
                stream.Write(string.IsNullOrEmpty(root.frameTime) ? (1000f / frameFPS) : float.Parse(root.frameTime));
                stream.Write(frameFPS);
                if (frameIndex == stopFrameIndex) // encode memory info
                {
                    stream.Write((ushort) (8 * 6));
                    stream.Write(Profiler.usedHeapSizeLong); // 1
                    stream.Write(Profiler.GetMonoUsedSizeLong()); // 2
                    stream.Write(Profiler.GetMonoHeapSizeLong()); // 3
                    stream.Write(Profiler.GetTotalAllocatedMemoryLong()); // 4
                    stream.Write(Profiler.GetTotalReservedMemoryLong()); // 5
                    stream.Write(Profiler.GetTotalUnusedReservedMemoryLong()); // 6
                }
                else
                {
                    stream.Write((ushort) 0);
                }

                //encode statistics
                for (var area = 0; area < profilerAreaCount; area++)
                {
                    var statistics = metadatas[(int)area];
                    stream.Write((byte)area);
                    for (var i = 0; i < statistics.Count; i++)
                    {
                        var maxValue = 0.0f;
                        var identifier = statistics[i];
                        ProfilerDriver.GetStatisticsValues(identifier, frameIndex, 1.0f, provider, out maxValue);
                        stream.Write(provider[0]);
                    }
                }
                
                // encode samples
                stream.Write(samples.Count);
                foreach (var pair in samples)
                {
                    var v = pair.Value;
                    stream.Write(v.id);
                    stream.Write(getStringRef(v.name));
                    stream.Write(v.callsCount);
                    stream.Write(v.gcAllocBytes);
                    stream.Write(v.totalTime);
                    stream.Write(v.selfTime);
                }
                // encode relations
                stream.Write(relations.Count);
                foreach (var pair in relations)
                {
                    stream.Write(pair.Key);
                    stream.Write(pair.Value);
                }
                stream.Write((uint)0x12345678); // magic number
                
                // next frame
                ++frameIndex;
            }

            frameCursor = stopFrameIndex;
        }
    }
}