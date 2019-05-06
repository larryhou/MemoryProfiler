using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditorInternal;
using UnityEngine;
using UnityEngine.Profiling;

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
            Profiler.enabled = false;
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
                    
                    samples.Add(sequence, new StackSample
                    {
                        id = sequence,
                        name = root.propertyName,
                        callsCount = (int)root.GetColumnAsSingle(ProfilerColumn.Calls),
                        gcAllocBytes = (int)root.GetColumnAsSingle(ProfilerColumn.GCMemory),
                        totalTime = root.GetColumnAsSingle(ProfilerColumn.TotalTime),
                        selfTime = root.GetColumnAsSingle(ProfilerColumn.SelfTime),
                    });

                    if (cursor.Count != 0)
                    {
                        relations[sequence] = cursor.Peek();
                    }

                    if (root.HasChildren)
                    {
                        cursor.Push(sequence);
                    }

                    ++sequence;
                }
                
                // encode frame index
                stream.Write(frameIndex);
                stream.Write(float.Parse(root.frameTime));
                stream.Write(float.Parse(root.frameFPS));
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