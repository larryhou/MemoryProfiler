//
//  main.cpp
//  UnityProfiler
//
//  Created by larryhou on 2019/5/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#if defined _MSC_VER
#include <direct.h>
#else 
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

#include "utils.h"
#include "Crawler/record.h"

using std::cout;
using std::endl;

std::ifstream *stream;

void processRecord(const char *filepath)
{
    RecordCrawler crawler;
    crawler.load(filepath);
    
    auto filename = basename(filepath);
    
    char cmdpath[256];

#if defined _MSC_VER
    _mkdir("__commands");
#else
    mkdir("__commands", 0777);
#endif

    memset(cmdpath, 0, sizeof(cmdpath));
    sprintf(cmdpath, "__commands/%s.plog", filename.c_str());
    
    std::ofstream plog;
    plog.open(cmdpath, std::ofstream::app);
    
    std::istream *stream = &std::cin;
    while (true)
    {
        auto replaying = typeid(*stream) != typeid(std::cin);
        auto recordable = true;
        
        std::cout << "\e[93m/> ";
        
        READ_LINE:
        std::string input;
        while (!getline(*stream, input))
        {
            if (replaying)
            {
                ((ifstream *)stream)->close();
                stream = &std::cin;
                replaying = false;
            }
            stream->clear();
            replaying ? std::this_thread::sleep_for(std::chrono::microseconds(500000)) : std::this_thread::sleep_for(std::chrono::microseconds(100000));
        }
        
        if (input.size() == 0 || input[0] == '#')
        {
            goto READ_LINE;
        }
        
        if (replaying)
        {
            auto iter = input.begin();
            while (iter != input.end())
            {
                cout << *iter++ << std::flush;
                std::this_thread::sleep_for(std::chrono::microseconds(500000));
            }
            cout << endl;
        }
        
        cout << "\e[0m" << "\e[36m";
        
        const char *command = input.c_str();
        if (strbeg(command, "frame"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.inspectFrame(-1);
                                   }
                                   else
                                   {
                                       int32_t depth = 0;
                                       if (options.size() >= 3)
                                       {
                                           depth = atoi(options[2]);
                                       }
                                       crawler.inspectFrame(atoi(options[1]), depth);
                                   }
                               });
        }
        else if (strbeg(command, "next"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.next();
                                   }
                                   else
                                   {
                                       crawler.next(atoi(options[1]), options.size() >= 3 ? atoi(options[2]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "prev"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.prev();
                                   }
                                   else
                                   {
                                       crawler.prev(atoi(options[1]), options.size() >= 3 ? atoi(options[2]) : -1);
                                   }
                               });
        }
        else if (strbeg(command, "info"))
        {
            crawler.summarize(false);
        }
        else if (strbeg(command, "alloc"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.findFramesWithAlloc();
                                   }
                                   else
                                   {
                                       if (options.size() >= 2)
                                       {
                                           auto count = -1;
                                           auto offset = atoi(options[1]);
                                           if (options.size() >= 3)
                                           {
                                               count = atoi(options[2]);
                                           }
                                           crawler.findFramesWithAlloc(offset, count);
                                       }
                                   }
                               });
        }
        else if (strbeg(command, "func"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.statByFunction();
                                   }
                                   else
                                   {
                                       crawler.statByFunction(atoi(options[1]));
                                   }
                               });
        }
        else if (strbeg(command, "sumf"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   for (auto i = 1; i < options.size(); i++)
                                   {
                                       crawler.inspectFunction(atoi(options[i]));
                                   }
                               });
        }
        else if (strbeg(command, "find"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       crawler.findFramesWithFunction(atoi(options[1]));
                                   }
                               });
        }
        else if (strbeg(command, "list"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.list();
                                   }
                                   else
                                   {
                                       int32_t count = 0;
                                       int32_t offset = atoi(options[1]);
                                       int32_t sorting = 0;
                                       
                                       if (options.size() >= 3)
                                       {
                                           count = atoi(options[2]);
                                           if (options.size() >= 4)
                                           {
                                               auto sign = options[3];
                                               if (strcmp(sign, "+"))
                                               {
                                                   sorting = 1;
                                               }
                                               else if( strcmp(sign, "-"))
                                               {
                                                   sorting = -1;
                                               }
                                           }
                                       }
                                       crawler.list(offset, count, sorting);
                                   }
                               });
        }
        else if (strbeg(command, "fps"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.summarize(true);
                                   }
                                   else
                                   {
                                       float fps = atof(options[1]);
                                       if (options.size() >= 3)
                                       {
                                           auto sign = options[2];
                                           if (strbeg(sign, "="))
                                           {
                                               crawler.findFramesWithFPS(fps, [](auto a, auto b) { return a == b; });
                                           }
                                           else if (strbeg(sign, "<"))
                                           {
                                               crawler.findFramesWithFPS(fps, [](auto a, auto b) { return a < b; });
                                           }
                                           else
                                           {
                                               crawler.findFramesWithFPS(fps, [](auto a, auto b) { return a > b; });
                                           }
                                       }
                                       else
                                       {
                                           crawler.findFramesWithFPS(fps, [](auto a, auto b) { return a < b; });
                                       }
                                   }
                               });
        }
        else if (strbeg(command, "replay"))
        {
            recordable = false;
            std::vector<string> commands;
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   auto ifs = new ifstream;
                                   if (options.size() == 1)
                                   {
                                       ifs->open(cmdpath);
                                   }
                                   else
                                   {
                                       ifs->open(options[1]);
                                   }
                                   stream = ifs;
                               });
        }
        else if (strbeg(command, "meta"))
        {
            crawler.dumpMetadatas();
        }
        else if (strbeg(command, "stat"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() >= 2)
                                   {
                                       int32_t property = 0;
                                       int32_t area = atoi(options[1]);
                                       if (options.size() >= 3)
                                       {
                                           property = atoi(options[2]);
                                       }
                                       crawler.statValues((ProfilerArea)area, property);
                                   }
                               });
        }
        else if (strbeg(command, "seek"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() >= 4)
                                   {
                                       int32_t property = 0;
                                       int32_t area = atoi(options[1]);
                                       property = atoi(options[2]);
                                       float threshold = atof(options[3]);
                                       if (options.size() >= 5)
                                       {
                                           auto sign = options[4];
                                           if (strbeg(sign, ">"))
                                           {
                                               crawler.findFramesMatchValue((ProfilerArea)area, property, threshold, [](float a, float b) {return a > b;});
                                           }
                                           else if (strbeg(sign, "<"))
                                           {
                                               crawler.findFramesMatchValue((ProfilerArea)area, property, threshold, [](float a, float b) {return a < b;});
                                           }
                                           else
                                           {
                                               crawler.findFramesMatchValue((ProfilerArea)area, property, threshold, [](float a, float b) {return a == b;});
                                           }
                                       }
                                       else
                                       {
                                           crawler.findFramesMatchValue((ProfilerArea)area, property, threshold, [](float a, float b) {return a > b;});
                                       }
                                   }
                                   else
                                   {
                                       printf("seek [PROFILER_AREA] [PROPERTY] [VALUE]\n");
                                   }
                                   
                               });
        }
        else if (strbeg(command, "lock"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.lock();
                                   }
                                   else
                                   {
                                       int32_t frameCount = -1;
                                       int32_t frameIndex = atoi(options[1]);
                                       if (options.size() >= 3)
                                       {
                                           frameCount = atoi(options[2]);
                                       }
                                       crawler.lock(frameIndex, frameCount);
                                   }
                               });
        }
        else if (strbeg(command, "quit"))
        {
            recordable = false;
            cout << "\e[0m";
            exit(0);
        }
        else if (strbeg(command, "help"))
        {
            recordable = false;
            const int __indent = 5;
            help("alloc", "[FRAME_OFFSET] [FRAME_COUNT]", "搜索申请动态内存的帧", __indent);
            help("frame","[FRAME_INDEX]", "查看帧时间消耗详情", __indent);
            help("func", NULL, "按照方法名统计时间消耗", __indent);
            help("sumf", "[FUNCTION_NAME_REF]*", "在当前帧区间统计函数时间开销", __indent);
            help("find", "[FUNCTION_NAME_REF]*", "按照方法名索引查找调用帧", __indent);
            help("list", "[FRAME_OFFSET] [±FRAME_COUNT] [+|-]", "列举帧简报 支持排序(+按fps升序 -按fps降序)输出 默认不排序", __indent);
            help("next", "[STEP]", "查看后STEP[=1]帧时间消耗详情", __indent);
            help("prev", "[STEP]", "查看前STEP[=1]帧时间消耗详情", __indent);
            help("meta", NULL, "查看性能指标参数", __indent);
            help("lock", "[FRAME_INDEX] [FRAME_COUNT]", "锁定帧范围", __indent);
            help("stat", "[PROFILER_AREA] [PROPERTY]", "统计性能指标", __indent);
            help("seek", "[PROFILER_AREA] [PROPERTY] [VALUE] [>|=|<]", "搜索性能指标满足条件(>大于VALUE[默认] =等于VALUE <小于VALUE)的帧", __indent);
            help("info", NULL, "性能摘要", __indent);
            help("fps", "[FPS] [>|=|<]", "搜索满足条件(>大于FPS =等于FPS <小于FPS[默认])的帧", __indent);
            help("help", NULL, "帮助", __indent);
            help("quit", NULL, "退出", __indent);
            cout << std::flush;
        }
        else
        {
            recordable = false;
            printf("not supported command [%s]\n", command);
        }
        
        if (!replaying && recordable)
        {
            plog.clear();
            plog << input.c_str() << endl;
        }
    }
}

int main(int argc, const char * argv[])
{
    cout << "argc=" << argc << endl;
    for (auto i = 0; i < argc; i++)
    {
        cout << "argv[" << i << "]=" << argv[i] << endl;
    }
    
    if (argc > 1)
    {
        processRecord(argv[1]);
    }
    
    return 0;
}
