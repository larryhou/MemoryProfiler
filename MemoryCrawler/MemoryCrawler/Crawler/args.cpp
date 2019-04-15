//
//  args.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/13.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "args.h"
#include <exception>

void ArgumentManager::addOption(ArgumentOption *option)
{
    auto name = __map.find(option->name);
    if (name == __map.end())
    {
        __map.insert(std::pair<const char *, ArgumentOption *>(option->name, option));
        if (option->abbr != nullptr)
        {
            auto abbr = __map.find(option->abbr);
            if (abbr == __map.end())
            {
                __map.insert(std::pair<const char *, ArgumentOption *>(option->abbr, option));
            }
            else
            {
                char error[40];
                sprintf(error, "Option[%s] already exist!", option->abbr);
                throw std::invalid_argument(error);
            }
        }
        else
        {
            char error[40];
            sprintf(error, "Option[%s] already exist!", option->name);
            throw std::invalid_argument(error);
        }
        __options.emplace_back(option);
    }
}

void ArgumentManager::help()
{
    int sizeName = 0, sizeAbbr = 0;
    for (auto i = 0; i < __options.size(); i++)
    {
        auto &option = *__options[i];
        auto size = strlen(option.name);
        if (size > sizeName) { sizeName = (int)size; }
        if (option.abbr != nullptr)
        {
            size = strlen(option.abbr);
            if (size > sizeAbbr) { sizeAbbr = (int)size; }
        }
    }
    
    char format[20];
    sprintf(format, "  %%-%ds %%-%ds %%-s\n", sizeName, sizeAbbr);
    printf("Usage: MemoryCrawler [options]\n");
    for (auto i = 0; i < __options.size(); i++)
    {
        auto &option = *__options[i];
        printf(format, option.name, option.abbr, option.help);
    }
}

ArgumentOption *ArgumentManager::getOption(const char *name)
{
    auto iter = __map.find(name);
    return iter == __map.end() ? nullptr : iter->second;
}

void ArgumentManager::parseArguments(int argc, const char *argv[])
{
    for (auto i = 0; i < argc; i++)
    {
        printf("[%d] %s\n", i + 1, argv[i]);
    }
}
