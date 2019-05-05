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

#endif /* utils_h */
