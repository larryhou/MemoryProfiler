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
            const int __indent = 5;
            help("alloc", "[FRAME_OFFSET] [FRAME_COUNT]", "搜索申请动态内存的帧", __indent);
            help("frame","[FRAME_INDEX]", "查看帧时间消耗详情", __indent);
            help("stat", NULL, "按照方法名统计时间消耗", __indent);
            help("find", "[FUNCTION_NAME_REF]*", "按照方法名索引查找调用帧", __indent);
            help("list", "[FRAME_OFFSET] [±FRAME_COUNT] [+|-]", "列举帧简报 支持排序(+按fps升序 -按fps降序)输出 默认不排序", __indent);
            help("next", "[STEP]", "查看后STEP[=1]帧时间消耗详情", __indent);
            help("prev", "[STEP]", "查看前STEP[=1]帧时间消耗详情", __indent);
            help("info", NULL, "性能摘要", __indent);
            help("fps", "[FPS] [>|=|<]", "搜索满足条件(>大于FPS =等于FPS <小于FPS[默认])的帧", __indent);
            help("help", NULL, "帮助", __indent);
            help("quit", NULL, "退出", __indent);
            cout << std::flush;
        }
        else
        {
            printf("not supported command [%s]\n", command);
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
