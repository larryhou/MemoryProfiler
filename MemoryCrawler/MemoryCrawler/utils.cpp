//
//  utils.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/5/5.
//  Copyright ? 2019 larryhou. All rights reserved.
//

#include "utils.h"
#include <iostream>

char __help_padding[64];

std::string comma(uint64_t v, uint32_t width)
{
    const int32_t SEGMENT_SIZE = 3;
    auto size = (int32_t)ceil(log10(fmax(10, v)));
    if (width < size) { width = size; }
    
    auto fsize = width + width / SEGMENT_SIZE;
    if (width != 0 && width % SEGMENT_SIZE == 0) { --fsize; }
    

	AutoCharVector tmp(fsize + 1);
	char* buf = tmp.p();
    auto ptr = buf + tmp.size() - 1;
    memset(ptr--, 0, 1);

    if (v == 0) { *ptr-- = '0'; }
    else
    {
        auto num = 0;
        while (v > 0)
        {
			if (ptr >= buf)
	            *ptr-- = '0' + (v % 10);
            if (++num % SEGMENT_SIZE == 0) 
			{
				if (ptr >= buf)
					*ptr-- = v < 10 ? ' ' : ','; 
			}
            v /= 10;
        }
    }
    
    if (ptr >= buf) { memset(buf, ' ', ptr - buf + 1); }
	string ret(buf);
    return ret;
}

std::string basename(const char *filepath)
{
    auto offset = filepath + strlen(filepath);
    const char *upper = nullptr;
    
    while (offset != filepath)
    {
        if (upper == nullptr && *offset == '.') {upper = offset;}
        if (*offset == '/')
        {
            ++offset;
            break;
        }
        --offset;
    }
    
    return std::string(offset, upper - offset);
}


void help(const char *command, const char *options, const char *description, const int width)
{
    auto indent = std::max(0, width - (int)strlen(command));
    memset(__help_padding, 0, indent + 1);
    if (indent > 0) {memset(__help_padding, '\x20', indent);}
    
    std::cout << EM36 << __help_padding << command ;
    if (options)
    {
        std::cout << EM36 << options << '\x20';
    }

    std::cout << EM33 << " " << description << '\n';
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
	std::vector<string> stringbuf;

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

                // auto option = new char[size];
                // strcpy(option, item);
				string option = item;
				stringbuf.push_back(option);

                options.push_back(stringbuf[stringbuf.size()-1].c_str());

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
}

bool CommandHistory::detect(std::string &command)
{
    if (command.size() <= 3) {return false;}
    
    if (command[0] == '\x1b' && command[1] == '\x5b')
    {
        switch (command[2])
        {
            case '\x41': // backward
                command = backward();
                return true;
                
            case '\x42': // forward
                command = forward();
                return true;
        }
    }
    
    return false;
}

void CommandHistory::accept(std::string command)
{
    __commands.emplace_back(command);
    __cursor = (int32_t)__commands.size() - 1;
}

std::string CommandHistory::forward()
{
    if (__cursor + 1 >= __commands.size()) {return "";}
    return __commands[++__cursor];
}

std::string CommandHistory::backward()
{
    if (__cursor - 1 < 0) {return __commands.front();}
    return __commands[--__cursor];
}

std::string CommandHistory::get()
{
    return __commands[__cursor];
}

