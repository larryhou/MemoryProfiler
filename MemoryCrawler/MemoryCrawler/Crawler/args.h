//
//  args.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/13.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef args_h
#define args_h

#include <stdio.h>
#include <cassert>
#include <vector>
#include <map>

struct ArgumentOption
{
    const char *name;
    const char *abbr;
    const char *help;
    
    const bool hasOptionValue;
    const bool optional;
    
    std::vector<const char *> values;
    
    ArgumentOption(const char *name, const char *abbr, const char *help, bool hasOptionValue, bool optional = true): name(name), abbr(abbr), help(help), hasOptionValue(hasOptionValue), optional(optional)
    {
        assert(name[0] == '-' && name[1] == '-');
        assert(abbr == nullptr || (abbr[0] == '-' && abbr[1] != '-'));
    }
    
    ArgumentOption(const char *name, const char *help, bool hasOptionValue, bool optional = true): ArgumentOption(name, nullptr, help, optional, hasOptionValue) {}
    ArgumentOption(const char *name, bool hasOptionValue, bool optional = true): ArgumentOption(name, nullptr, nullptr, optional, hasOptionValue) {}
};

class ArgumentManager
{
    std::vector<ArgumentOption *> __options;
    std::map<const char *, ArgumentOption *> __map;
    
public:
    void addOption(ArgumentOption *option);
    ArgumentOption *getOption(const char *name);
    void parseArguments(int argc, const char **argv);
    void help();
};

#endif /* args_h */
