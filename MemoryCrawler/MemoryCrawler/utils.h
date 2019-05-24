//
//  utils.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/5/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef utils_h
#define utils_h

#include <string>
#include <vector>

bool strbeg(const char *str, const char *cmp);
void readCommandOptions(const char *command, std::function<void(std::vector<const char *> &)> callback);
void help(const char *command, const char *options, const char *description, const int width = 6);
const char *basename(const char *filepath);

class CommandHistory
{
private:
    std::vector<std::string> __commands;
    int32_t __cursor = 0;
    
public:
    bool detect(std::string &command);
    void accept(std::string command);
    std::string backward();
    std::string forward();
    std::string get();
};

#endif /* utils_h */
