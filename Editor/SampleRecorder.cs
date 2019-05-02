using System;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.Profiling;

namespace Moobyte.MemoryProfiler
{
    public static class SampleRecorder
    {
        [MenuItem("性能/开始采样", false, 51)]
        public static void StartRecording()
        {
            var spacedir = string.Format("{0}/../SamplesCapture", Application.dataPath);
            if (!Directory.Exists(spacedir))
            {
                Directory.CreateDirectory(spacedir);
            }

            var filepath = string.Format("{0}/{1:yyyyMMddHHmmss}_rec", spacedir, DateTime.Now);
            Profiler.logFile = filepath;
            Profiler.enableBinaryLog = true;
            Profiler.enabled = true;
        }

        [MenuItem("性能/停止采样", false, 52)]
        public static void StopRecording()
        {
            Profiler.enabled = false;
            Profiler.enableBinaryLog = false;
            Profiler.logFile = null;
        }
    }
}