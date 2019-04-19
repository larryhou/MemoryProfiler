//
//  stat.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/19.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef stat_h
#define stat_h

#include <vector>
#include <map>

#include <stdio.h>
#include <math.h>

template <class T>
class Statistics
{
    std::vector<T> __samples;
    
public:
    T min;
    T max;
    
    T opt_min;
    T opt_max;
    
    double mean;
    double sd;
    
public:
    Statistics();
    void collect(T sample);
    void summarize();
    void clear();
};

template <class T>
Statistics<T>::Statistics()
{
    static_assert(std::is_arithmetic<T>::value, "NOT summarable type");
}

template <class T>
void Statistics<T>::collect(T sample)
{
    __samples.push_back(sample);
}

template <class T>
void Statistics<T>::summarize()
{
    T sum = 0;
    
    min = __samples[0];
    max = 0;
    for (auto iter = __samples.begin(); iter != __samples.end(); iter++)
    {
        auto v = *iter;
        sum += v;
        if (v > max) {max = v;}
        if (v < min) {min = v;}
    }
    
    mean = (double)sum / (double)__samples.size();
    
    double dx = 0;
    for (auto iter = __samples.begin(); iter != __samples.end(); iter++)
    {
        dx += pow((double)*iter - mean, 2);
    }
    
    double sd = pow(dx / (double)(__samples.size() - 1), 0.5);
    auto upper = mean - 3 * sd;
    auto lower = mean + 3 * sd;
    
    opt_max = max;
    opt_min = min;
    for (auto iter = __samples.begin(); iter != __samples.end(); iter++)
    {
        auto v = *iter;
        if (v > opt_max && v <= upper) {opt_max = v;}
        if (v < opt_min && v >= lower) {opt_min = v;}
    }
}

template <class T>
void Statistics<T>::clear()
{
    __samples.clear();
}

class TrackStatistics
{
    using sample_t = std::tuple<int32_t/*element_index*/, int32_t/*type_index*/, int32_t/*element_size*/>;
    
private:
    std::vector<sample_t> __samples;
    std::map<int32_t, int32_t> __memory;
    
public:
    void summarize();
    void collect(int32_t itemIndex, int32_t typeIndex, int32_t size);
    void foreach(std::function<void(int32_t, int32_t, int64_t)> callback, int32_t depth = 10);
};

#endif /* stat_h */
