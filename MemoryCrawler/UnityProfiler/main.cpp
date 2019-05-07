//
//  main.cpp
//  UnityProfiler
//
//  Created by larryhou on 2019/5/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

#include "utils.h"
#include "Crawler/record.h"

using std::cout;
using std::endl;

std::ifstream *stream;

void processRecord(const char *filepath)
{
    RecordCrawler crawler;
    crawler.load(filepath);
    
    std::istream *stream = &std::cin;
    while (true)
    {
        auto replaying = typeid(*stream) != typeid(std::cin);
        
        std::cout << "\e[93m/> ";
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
            replaying ? usleep(500000) : usleep(100000);
        }
        
        cout << "\e[0m" << "\e[36m";
        
        const char *command = input.c_str();
        if (strbeg(command, "frame"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.inspectFrame();
                                   }
                                   else
                                   {
                                       for (auto i = 1; i < options.size(); i++)
                                       {
                                           crawler.inspectFrame(atoi(options[i]));
                                       }
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
                                       crawler.next(atoi(options[1]));
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
                                       crawler.prev(atoi(options[1]));
                                   }
                               });
        }
        else if (strbeg(command, "info"))
        {
            crawler.summary();
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
        else if (strbeg(command, "stat"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1)
                                   {
                                       crawler.generateStatistics();
                                   }
                                   else
                                   {
                                       crawler.generateStatistics(atoi(options[1]));
                                   }
                               });
        }
        else if (strbeg(command, "find"))
        {
            readCommandOptions(command, [&](std::vector<const char *> &options)
                               {
                                   if (options.size() > 1)
                                   {
                                       crawler.findFramesContains(atoi(options[1]));
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
                                       crawler.summary();
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
        else if (strbeg(command, "quit"))
        {
            cout << "\e[0m";
            exit(0);
        }
        else if (strbeg(command, "help"))
        {
            cout << "\e[36m" << "alloc [FRAME_OFFSET] [FRAME_COUNT]" << "\n    \e[33m搜索执行GC.Alloc方法的帧" << endl;
            cout << "\e[36m" << "frame [FRAME_INDEX]" << "\n    \e[33m查看帧时间消耗详情" << endl;
            cout << "\e[36m" << "stat" << "\n    \e[33m按照方法名统计时间消耗" << endl;
            cout << "\e[36m" << "find FUNCTION_NAME_REF" << "\n    \e[33m按照方法名索引查找调用帧" << endl;
            cout << "\e[36m" << "list [FRAME_OFFSET] [FRAME_COUNT] [+|-]" << "\n    \e[33m列举帧简报 +: 按照fps升序输出 -: 按照fps降序输出" << endl;
            cout << "\e[36m" << "next [STEP]" << "\n    \e[33m查看后STEP帧时间消耗详情 默认STEP=1" << endl;
            cout << "\e[36m" << "prev [STEP]" << "\n    \e[33m查看前STEP帧时间消耗详情 默认STEP=1" << endl;
            cout << "\e[36m" << "info" << "\n    \e[33m性能摘要" << endl;
            cout << "\e[36m" << "fps [FPS_THRESHOLD] [>|=|<]" << "\n    \e[33m搜索满足FPS条件的帧 >: 列举大于THRESHOLD的帧 =: 列举等于THRESHOLD的帧 <: 列举小于THRESHOLD的帧" << endl;
            cout << "\e[36m" << "quit" << "\n    \e[33m退出" << endl;
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
