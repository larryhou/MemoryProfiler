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
        public string path;
        public int callsCount;
        public int gcAllocBytes;
        public double totalTime;
        public double selfTime;

        public void encode(Stream stream)
        {
            stream.Write(id);
            stream.Write(name);
            stream.Write(path);
            stream.Write(callsCount);
            stream.Write(gcAllocBytes);
            stream.Write(totalTime);
            stream.Write(selfTime);
        }
    }
    
    public static class UnityProfilerRecorder
    {
        [MenuItem("性能/开始采样", false, 51)]
        public static void StartRecording()
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
            
            stream = new FileStream(string.Format("{0}/{1:yyyyMMddHHmmss}_PERF.pfc", spacedir, DateTime.Now), FileMode.Create);
            frameCount = 0;
            
            Profiler.enabled = true;
            EditorApplication.update -= Update;
            EditorApplication.update += Update;
        }

        [MenuItem("性能/停止采样", false, 52)]
        public static void StopRecording()
        {
            Profiler.enabled = false;
            EditorApplication.update -= Update;
            
            if (stream != null)
            {
                stream.Close();
                stream = null;
            }
        }

        private static int frameCount = 0;
        private static FileStream stream;
        
        static void Update()
        {
            var lastFrameIndex = ProfilerDriver.lastFrameIndex;
            if (lastFrameIndex >= 0)
            {
                if (frameCount++ >= 2)
                {
                    return;
                }

                var frameIndex = lastFrameIndex;

                var samples = new Dictionary<int, StackSample>();
                var connections = new Dictionary<int, List<int>>();
                var relations = new Dictionary<int, int>();

                var cursor = new Stack<int>();
                var sequence = 0;
                
                var root = new ProfilerProperty();
                root.SetRoot(frameIndex, ProfilerColumn.DontSort, ProfilerViewType.Hierarchy);
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
                        path = root.propertyPath,
                        callsCount = (int)root.GetColumnAsSingle(ProfilerColumn.Calls),
                        gcAllocBytes = (int)root.GetColumnAsSingle(ProfilerColumn.GCMemory),
                        totalTime = root.GetColumnAsSingle(ProfilerColumn.TotalTime),
                        selfTime = root.GetColumnAsSingle(ProfilerColumn.SelfTime)
                    });

                    if (cursor.Count != 0)
                    {
                        var top = cursor.Peek();
                        relations[sequence] = top;
                        
                        List<int> children;
                        if (!connections.TryGetValue(top, out children))
                        {
                            connections.Add(top, children = new List<int>());
                        }
                        children.Add(sequence);
                    }

                    if (root.HasChildren)
                    {
                        cursor.Push(sequence);
                    }

                    ++sequence;
                }
                
                // frame_index
                stream.Write(frameIndex);
                // samples
                stream.Write(samples.Count);
                foreach (var pair in samples)
                {
                    pair.Value.encode(stream);
                }
                // relations
                stream.Write(relations.Count);
                foreach (var pair in relations)
                {
                    stream.Write(pair.Key);
                    stream.Write(pair.Value);
                }
                
                Debug.Log(sequence);
            }
        }
    }
}