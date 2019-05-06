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
            crawler.next();
        }
        else if (strbeg(command, "prev"))
        {
            crawler.prev();
        }
        else if (strbeg(command, "info"))
        {
            crawler.summary();
        }
        else if (strbeg(command, "quit"))
        {
            cout << "\e[0m";
            exit(0);
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
