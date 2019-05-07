//
//  utils.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/5/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "utils.h"
#include <iostream>

char __help_padding[64];

void help(const char *command, const char *options, const char *description, const int width)
{
    auto indent = std::max(0, width - (int)strlen(command));
    memset(__help_padding, 0, indent + 1);
    if (indent > 0) {memset(__help_padding, '\x20', indent);}
    
    std::cout
    << "\e[36m\e[1m" << __help_padding << command << " \e[0m";
    if (options)
    {
        std::cout << "\e[36m" << options << '\x20';
    }
    std::cout << "\e[33m" << description << '\n';
}

bool strbeg(const char *str, const char *cmp)
{
    auto size = strlen(cmp);
    for (auto i = 0; i < size; i++)
    {
        if (str[i] != cmp[i]){return false;}
    }
    if (strlen(str) > size)
    {
        return str[size] == ' ';
    }
    return true;
}

void readCommandOptions(const char *command, std::function<void(std::vector<const char *> &)> callback)
{
    std::vector<const char *> options;
    auto charCount = strlen(command);
    char *item = new char[256];
    auto iter = 0;
    for (auto i = 0; i < charCount; i++)
    {
        if (command[i] == ' ')
        {
            if (iter > 0)
            {
                item[iter] = 0; // end c string
                auto size = strlen(item);
                auto option = new char[size];
                strcpy(option, item);
                options.push_back(option);
                iter = 0;
            }
        }
        else
        {
            item[iter++] = command[i];
        }
    }
    
    item[iter] = 0; // end c string
    if (strlen(item) > 0) { options.push_back(item); }
    
    callback(options);
    
    for (auto i = 0; i < options.size(); i++) { delete [] options[i]; }
}
